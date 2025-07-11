#include "tcp_client.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>

namespace tcp {

TcpClient::TcpClient() 
    : remotePort_(0), localPort_(0), state_(ConnectionState::Disconnected),
      sslEnabled_(false), sslContext_(nullptr), sslHandle_(nullptr),
      shouldStop_(false), autoReconnect_(false), reconnectInterval_(5000),
      heartbeatEnabled_(false), heartbeatInterval_(30000) {
}

TcpClient::~TcpClient() {
    disconnect();
}

bool TcpClient::connect(const std::string& address, uint16_t port) {
    return connect(address, port, options_.connectTimeout);
}

bool TcpClient::connect(const std::string& address, uint16_t port, std::chrono::milliseconds timeout) {
    return connectInternal(address, port, timeout);
}

void TcpClient::disconnect() {
    setState(ConnectionState::Disconnecting);
    shouldStop_ = true;
    
    {
        std::lock_guard<std::mutex> lock(reconnectMutex_);
        autoReconnect_ = false;
        reconnectCondition_.notify_all();
    }
    
    {
        std::lock_guard<std::mutex> lock(heartbeatMutex_);
        heartbeatEnabled_ = false;
        heartbeatCondition_.notify_all();
    }
    
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
    
    if (reconnectThread_.joinable()) {
        reconnectThread_.join();
    }
    
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
    
    cleanupSsl();
    close();
    setState(ConnectionState::Disconnected);
    
    if (onDisconnected_) {
        onDisconnected_();
    }
}

bool TcpClient::isConnected() const {
    return state_ == ConnectionState::Connected;
}

std::future<bool> TcpClient::connectAsync(const std::string& address, uint16_t port) {
    return connectAsync(address, port, options_.connectTimeout);
}

std::future<bool> TcpClient::connectAsync(const std::string& address, uint16_t port, std::chrono::milliseconds timeout) {
    return std::async(std::launch::async, [this, address, port, timeout]() {
        return connectInternal(address, port, timeout);
    });
}

void TcpClient::disconnectAsync() {
    std::thread([this]() {
        disconnect();
    }).detach();
}

bool TcpClient::send(const std::vector<uint8_t>& data) {
    return send(data.data(), data.size());
}

bool TcpClient::send(const std::string& data) {
    return send(data.data(), data.size());
}

bool TcpClient::send(const void* data, size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected() || !isValid()) {
        return false;
    }
    
    int sent;
    if (sslEnabled_) {
        sent = sendSsl(data, length);
    } else {
        sent = ::send(socket_, static_cast<const char*>(data), length, 0);
    }
    
    if (sent == SOCKET_ERROR) {
        handleError(ErrorCode::SendFailed, "Send failed");
        return false;
    }
    
    {
        std::lock_guard<std::mutex> statsLock(statisticsMutex_);
        statistics_.bytesSent += sent;
    }
    
    return true;
}

std::vector<uint8_t> TcpClient::receive(size_t maxLength) {
    std::vector<uint8_t> buffer(maxLength);
    int received = receiveRaw(buffer.data(), maxLength);
    if (received > 0) {
        buffer.resize(received);
        return buffer;
    }
    return {};
}

std::string TcpClient::receiveString(size_t maxLength) {
    std::vector<uint8_t> data = receive(maxLength);
    return std::string(data.begin(), data.end());
}

int TcpClient::receiveRaw(void* buffer, size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected() || !isValid()) {
        return -1;
    }
    
    int received;
    if (sslEnabled_) {
        received = receiveSsl(buffer, length);
    } else {
        received = ::recv(socket_, static_cast<char*>(buffer), length, MSG_DONTWAIT);
    }
    
    if (received == SOCKET_ERROR) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            return 0;
        }
        // Don't report connection reset as an error if we're already stopping
        if (shouldStop_ && error == WSAECONNRESET) {
            return -1;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        // Don't report connection reset as an error if we're already stopping
        if (shouldStop_ && errno == ECONNRESET) {
            return -1;
        }
#endif
        handleError(ErrorCode::ReceiveFailed, "Receive failed");
        return -1;
    }
    
    if (received == 0) {
        // Connection closed by peer
        setState(ConnectionState::Disconnected);
        return -1;
    }
    
    {
        std::lock_guard<std::mutex> statsLock(statisticsMutex_);
        statistics_.bytesReceived += received;
    }
    
    return received;
}

void TcpClient::sendAsync(const std::vector<uint8_t>& data, std::function<void(bool)> callback) {
    std::thread([this, data, callback]() {
        // Check if we're still connected before sending
        if (isConnected()) {
            bool success = send(data);
            if (callback) {
                callback(success);
            }
        } else {
            // Not connected
            if (callback) {
                callback(false);
            }
        }
    }).detach();
}

void TcpClient::sendAsync(const std::string& data, std::function<void(bool)> callback) {
    std::thread([this, data, callback]() {
        // Check if we're still connected before sending
        if (isConnected()) {
            bool success = send(data);
            if (callback) {
                callback(success);
            }
        } else {
            // Not connected
            if (callback) {
                callback(false);
            }
        }
    }).detach();
}

void TcpClient::receiveAsync(size_t maxLength, std::function<void(const std::vector<uint8_t>&)> callback) {
    std::thread([this, maxLength, callback]() {
        std::vector<uint8_t> data = receive(maxLength);
        if (callback) {
            callback(data);
        }
    }).detach();
}

bool TcpClient::enableSsl(std::shared_ptr<SslContext> context) {
    sslContext_ = context;
    sslEnabled_ = true;
    return setupSsl();
}

void TcpClient::enableAutoReconnect(bool enable, std::chrono::milliseconds interval) {
    std::lock_guard<std::mutex> lock(reconnectMutex_);
    autoReconnect_ = enable;
    reconnectInterval_ = interval;
    
    if (enable && !reconnectThread_.joinable()) {
        reconnectThread_ = std::thread(&TcpClient::reconnectLoop, this);
    }
}

TcpClient::Statistics TcpClient::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    return statistics_;
}

void TcpClient::enableHeartbeat(bool enable, std::chrono::milliseconds interval) {
    std::lock_guard<std::mutex> lock(heartbeatMutex_);
    heartbeatEnabled_ = enable;
    heartbeatInterval_ = interval;
    
    if (enable && !heartbeatThread_.joinable()) {
        heartbeatThread_ = std::thread(&TcpClient::heartbeatLoop, this);
    }
}

void TcpClient::setHeartbeatData(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(heartbeatMutex_);
    heartbeatData_ = data;
}

void TcpClient::setHeartbeatData(const std::string& data) {
    std::lock_guard<std::mutex> lock(heartbeatMutex_);
    heartbeatData_.assign(data.begin(), data.end());
}

bool TcpClient::connectInternal(const std::string& address, uint16_t port, std::chrono::milliseconds timeout) {
    if (isConnected()) {
        disconnect();
    }
    
    setState(ConnectionState::Connecting);
    
    if (!create()) {
        setState(ConnectionState::Error);
        return false;
    }
    
    // Resolve address
    std::string resolvedAddress = address;
    if (!resolveAddress(address, resolvedAddress)) {
        resolvedAddress = address; // Use as-is if resolution fails
    }
    
    // Set up address structure
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, resolvedAddress.c_str(), &serverAddr.sin_addr) <= 0) {
        handleError(ErrorCode::InvalidAddress, "Invalid address: " + resolvedAddress);
        setState(ConnectionState::Error);
        return false;
    }
    
    // Set non-blocking for timeout control
    setNonBlocking(true);
    
    // Connect
    int result = ::connect(socket_, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr));
    
    if (result == SOCKET_ERROR) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
#else
        if (errno != EINPROGRESS) {
#endif
            handleError(ErrorCode::ConnectionFailed, "Connection failed");
            setState(ConnectionState::Error);
            return false;
        }
        
        // Wait for connection with timeout
        fd_set writeSet, errorSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&errorSet);
        FD_SET(socket_, &writeSet);
        FD_SET(socket_, &errorSet);
        
        struct timeval tv;
        tv.tv_sec = timeout.count() / 1000;
        tv.tv_usec = (timeout.count() % 1000) * 1000;
        
        result = select(socket_ + 1, nullptr, &writeSet, &errorSet, &tv);
        
        if (result == 0) {
            handleError(ErrorCode::Timeout, "Connection timeout");
            setState(ConnectionState::Error);
            return false;
        }
        
        if (result == SOCKET_ERROR || FD_ISSET(socket_, &errorSet)) {
            handleError(ErrorCode::ConnectionFailed, "Connection failed");
            setState(ConnectionState::Error);
            return false;
        }
        
        // Check if connection was successful
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) == SOCKET_ERROR || error != 0) {
            handleError(ErrorCode::ConnectionFailed, "Connection failed");
            setState(ConnectionState::Error);
            return false;
        }
    }
    
    // Restore blocking mode
    setNonBlocking(false);
    
    // Store connection info
    remoteAddress_ = resolvedAddress;
    remotePort_ = port;
    connectedAt_ = std::chrono::system_clock::now();
    initializeLocalAddress();
    
    // Setup SSL if enabled
    if (sslEnabled_ && !setupSsl()) {
        handleError(ErrorCode::SslError, "SSL setup failed");
        setState(ConnectionState::Error);
        return false;
    }
    
    setState(ConnectionState::Connected);
    updateStatistics();
    
    // Start receive thread
    startReceiveThread();
    
    if (onConnected_) {
        onConnected_();
    }
    
    return true;
}

void TcpClient::setState(ConnectionState state) {
    state_ = state;
}

void TcpClient::startReceiveThread() {
    receiveThread_ = std::thread(&TcpClient::receiveLoop, this);
}

void TcpClient::receiveLoop() {
    std::vector<uint8_t> buffer(4096);
    
    while (!shouldStop_ && isConnected()) {
        // Check if we should stop before attempting to receive
        if (shouldStop_) {
            break;
        }
        
        int received = receiveRaw(buffer.data(), buffer.size());
        if (received > 0) {
            std::vector<uint8_t> data(buffer.begin(), buffer.begin() + received);
            if (onDataReceived_) {
                onDataReceived_(data);
            }
        } else if (received == -1) {
            // Connection closed or error occurred
            break;
        } else if (received == 0) {
            // No data available, continue
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void TcpClient::reconnectLoop() {
    while (!shouldStop_) {
        std::unique_lock<std::mutex> lock(reconnectMutex_);
        reconnectCondition_.wait(lock, [this] { return !autoReconnect_ || shouldStop_; });
        
        if (shouldStop_) {
            break;
        }
        
        if (autoReconnect_ && !isConnected()) {
            lock.unlock();
            
            std::this_thread::sleep_for(reconnectInterval_);
            
            if (!shouldStop_ && !isConnected()) {
                if (connectInternal(remoteAddress_, remotePort_, options_.connectTimeout)) {
                    std::lock_guard<std::mutex> statsLock(statisticsMutex_);
                    statistics_.reconnections++;
                }
            }
            
            lock.lock();
        }
    }
}

void TcpClient::heartbeatLoop() {
    while (!shouldStop_) {
        std::unique_lock<std::mutex> lock(heartbeatMutex_);
        heartbeatCondition_.wait_for(lock, heartbeatInterval_, [this] { return !heartbeatEnabled_ || shouldStop_; });
        
        if (shouldStop_) {
            break;
        }
        
        if (heartbeatEnabled_ && isConnected()) {
            std::vector<uint8_t> data = heartbeatData_;
            lock.unlock();
            
            if (!data.empty()) {
                send(data);
            }
            
            lock.lock();
        }
    }
}

void TcpClient::handleError(ErrorCode error, const std::string& message) {
    setState(ConnectionState::Error);
    if (onError_) {
        onError_(error, message);
    }
}

bool TcpClient::initializeLocalAddress() {
    if (!isValid()) {
        return false;
    }
    
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(socket_, reinterpret_cast<struct sockaddr*>(&addr), &len) == 0) {
        localAddress_ = inet_ntoa(addr.sin_addr);
        localPort_ = ntohs(addr.sin_port);
        return true;
    }
    
    return false;
}

void TcpClient::updateStatistics() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.totalConnections++;
    statistics_.lastConnectedAt = connectedAt_;
}

void TcpClient::cleanupSsl() {
    if (sslHandle_) {
        // SSL cleanup would go here
        sslHandle_ = nullptr;
    }
}

bool TcpClient::setupSsl() {
    if (!sslContext_) {
        return false;
    }
    
    // SSL setup would go here
    return true;
}

int TcpClient::sendSsl(const void* data, size_t length) {
    // SSL send implementation would go here
    return ::send(socket_, static_cast<const char*>(data), length, 0);
}

int TcpClient::receiveSsl(void* buffer, size_t length) {
    // SSL receive implementation would go here
    return ::recv(socket_, static_cast<char*>(buffer), length, 0);
}

} // namespace tcp
