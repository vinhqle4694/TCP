#include "tcp_socket.h"
#include <iostream>
#include <cstring>
#include <sstream>
#include <future>

#ifdef _WIN32
bool tcp::TcpSocket::wsaInitialized_ = false;
std::mutex tcp::TcpSocket::wsaMutex_;
#else
#include <netinet/tcp.h>
#endif

namespace tcp {

TcpSocket::TcpSocket() : socket_(INVALID_SOCKET), nonBlocking_(false) {
    initialize();
}

TcpSocket::~TcpSocket() {
    close();
    cleanup();
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept
    : socket_(other.socket_), nonBlocking_(other.nonBlocking_), options_(other.options_) {
    other.socket_ = INVALID_SOCKET;
    other.nonBlocking_ = false;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = other.socket_;
        nonBlocking_ = other.nonBlocking_;
        options_ = other.options_;
        other.socket_ = INVALID_SOCKET;
        other.nonBlocking_ = false;
    }
    return *this;
}

void TcpSocket::initialize() {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(wsaMutex_);
    if (!wsaInitialized_) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
        }
        wsaInitialized_ = true;
    }
#endif
}

void TcpSocket::cleanup() {
#ifdef _WIN32
    // Note: WSACleanup() should be called when the last socket is destroyed
    // This is a simplified implementation
#endif
}

bool TcpSocket::create() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (isValid()) {
        close();
    }
    
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET) {
        return false;
    }
    
    // Set default options (don't fail if some options can't be set)
    setSocketOptionsInternal(options_);
    
    return true;
}

void TcpSocket::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (isValid()) {
#ifdef _WIN32
        closesocket(socket_);
#else
        ::close(socket_);
#endif
        socket_ = INVALID_SOCKET;
    }
}

bool TcpSocket::isValid() const {
    return socket_ != INVALID_SOCKET;
}

bool TcpSocket::setSocketOptions(const SocketOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isValid()) {
        return false;
    }
    
    options_ = options;
    return setSocketOptionsInternal(options);
}

bool TcpSocket::setSocketOptionsInternal(const SocketOptions& options) {
    bool success = true;
    
    // Set reuse address
    int optval = options.reuseAddress ? 1 : 0;
    if (!setSocketOption(SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) {
        success = false;
    }
    
    // Set keep alive
    optval = options.keepAlive ? 1 : 0;
    if (!setSocketOption(SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval))) {
        success = false;
    }
    
    // Set no delay (disable Nagle's algorithm)
    optval = options.noDelay ? 1 : 0;
    if (!setSocketOption(IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval))) {
        success = false;
    }
    
    // Set send buffer size
    optval = options.sendBufferSize;
    if (!setSocketOption(SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval))) {
        success = false;
    }
    
    // Set receive buffer size
    optval = options.receiveBufferSize;
    if (!setSocketOption(SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval))) {
        success = false;
    }
    
    // Set timeouts
#ifdef _WIN32
    DWORD timeout = static_cast<DWORD>(options.sendTimeout.count());
    if (!setSocketOption(SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout))) {
        success = false;
    }
    
    timeout = static_cast<DWORD>(options.receiveTimeout.count());
    if (!setSocketOption(SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
        success = false;
    }
#else
    struct timeval timeout;
    timeout.tv_sec = options.sendTimeout.count() / 1000;
    timeout.tv_usec = (options.sendTimeout.count() % 1000) * 1000;
    if (!setSocketOption(SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout))) {
        success = false;
    }
    
    timeout.tv_sec = options.receiveTimeout.count() / 1000;
    timeout.tv_usec = (options.receiveTimeout.count() % 1000) * 1000;
    if (!setSocketOption(SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
        success = false;
    }
#endif
    
    return success;
}

SocketOptions TcpSocket::getSocketOptions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return options_;
}

bool TcpSocket::setNonBlocking(bool nonBlocking) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isValid()) {
        return false;
    }
    
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    if (ioctlsocket(socket_, FIONBIO, &mode) != 0) {
        return false;
    }
#else
    int flags = fcntl(socket_, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    
    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (fcntl(socket_, F_SETFL, flags) == -1) {
        return false;
    }
#endif
    
    nonBlocking_ = nonBlocking;
    return true;
}

bool TcpSocket::setSocketOption(int level, int optname, const void* optval, socklen_t optlen) {
    return setsockopt(socket_, level, optname, static_cast<const char*>(optval), optlen) == 0;
}

bool TcpSocket::getSocketOption(int level, int optname, void* optval, socklen_t* optlen) const {
    return getsockopt(socket_, level, optname, static_cast<char*>(optval), optlen) == 0;
}

ErrorCode TcpSocket::getLastError() const {
#ifdef _WIN32
    int error = WSAGetLastError();
    switch (error) {
        case WSAEWOULDBLOCK: return ErrorCode::WouldBlock;
        case WSAECONNRESET: return ErrorCode::ConnectionClosed;
        case WSAECONNREFUSED: return ErrorCode::ConnectionFailed;
        case WSAETIMEDOUT: return ErrorCode::Timeout;
        default: return ErrorCode::UnknownError;
    }
#else
    switch (errno) {
        case EAGAIN:
#if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:
#endif
            return ErrorCode::WouldBlock;
        case ECONNRESET: return ErrorCode::ConnectionClosed;
        case ECONNREFUSED: return ErrorCode::ConnectionFailed;
        case ETIMEDOUT: return ErrorCode::Timeout;
        default: return ErrorCode::UnknownError;
    }
#endif
}

std::string TcpSocket::errorToString(ErrorCode error) const {
    switch (error) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::InvalidSocket: return "Invalid socket";
        case ErrorCode::ConnectionFailed: return "Connection failed";
        case ErrorCode::ConnectionClosed: return "Connection closed";
        case ErrorCode::SendFailed: return "Send failed";
        case ErrorCode::ReceiveFailed: return "Receive failed";
        case ErrorCode::BindFailed: return "Bind failed";
        case ErrorCode::ListenFailed: return "Listen failed";
        case ErrorCode::AcceptFailed: return "Accept failed";
        case ErrorCode::InvalidAddress: return "Invalid address";
        case ErrorCode::Timeout: return "Timeout";
        case ErrorCode::WouldBlock: return "Would block";
        case ErrorCode::SslError: return "SSL error";
        case ErrorCode::UnknownError: return "Unknown error";
        default: return "Unknown error";
    }
}

std::string TcpSocket::getLocalAddress() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        struct addrinfo* result = nullptr;
        if (getaddrinfo(hostname, nullptr, &hints, &result) == 0) {
            if (result && result->ai_family == AF_INET) {
                struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
                std::string ip = inet_ntoa(addr->sin_addr);
                freeaddrinfo(result);
                return ip;
            }
            freeaddrinfo(result);
        }
    }
    return "127.0.0.1";
}

std::vector<std::string> TcpSocket::getLocalAddresses() {
    std::vector<std::string> addresses;
    
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints = {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        
        struct addrinfo* result = nullptr;
        if (getaddrinfo(hostname, nullptr, &hints, &result) == 0) {
            for (struct addrinfo* addr = result; addr != nullptr; addr = addr->ai_next) {
                if (addr->ai_family == AF_INET) {
                    struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(addr->ai_addr);
                    addresses.push_back(inet_ntoa(addr_in->sin_addr));
                }
            }
            freeaddrinfo(result);
        }
    }
    
    if (addresses.empty()) {
        addresses.push_back("127.0.0.1");
    }
    
    return addresses;
}

bool TcpSocket::resolveAddress(const std::string& hostname, std::string& ip) {
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    struct addrinfo* result = nullptr;
    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &result) == 0) {
        if (result && result->ai_family == AF_INET) {
            struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
            ip = inet_ntoa(addr->sin_addr);
            freeaddrinfo(result);
            return true;
        }
        freeaddrinfo(result);
    }
    return false;
}

// TcpConnection implementation
TcpConnection::TcpConnection(socket_t socket, const std::string& remoteAddress, uint16_t remotePort)
    : socket_(socket), remoteAddress_(remoteAddress), remotePort_(remotePort),
      localPort_(0), state_(ConnectionState::Connected), bytesSent_(0), bytesReceived_(0),
      sslEnabled_(false), sslContext_(nullptr), sslHandle_(nullptr), shouldStop_(false) {
    
    connectedAt_ = std::chrono::system_clock::now();
    initializeLocalAddress();
    startReceiveThread();
}

TcpConnection::~TcpConnection() {
    close();
}

void TcpConnection::close() {
    std::shared_ptr<TcpConnection> self;
    bool callDisconnectedCallback = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != ConnectionState::Disconnected) {
            setState(ConnectionState::Disconnecting);
            shouldStop_ = true;
            
            // Close the socket first to interrupt any blocking recv calls
            if (socket_ != INVALID_SOCKET) {
#ifdef _WIN32
                closesocket(socket_);
#else
                ::close(socket_);
#endif
                socket_ = INVALID_SOCKET;
            }
        }
    }
    
    // Wait for receive thread to finish without holding the mutex
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ConnectionState::Disconnected) {
            setState(ConnectionState::Disconnected);
            
            // Only call callback if we can safely get shared_from_this
            if (onDisconnected_) {
                try {
                    self = shared_from_this();
                    callDisconnectedCallback = true;
                } catch (const std::bad_weak_ptr&) {
                    // Object is being destroyed, skip callback
                }
            }
        }
    }
    
    // Call disconnected callback outside the mutex if safe
    if (callDisconnectedCallback && self) {
        onDisconnected_(self);
    }
}

bool TcpConnection::isConnected() const {
    return state_ == ConnectionState::Connected;
}

ConnectionState TcpConnection::getState() const {
    return state_;
}

ConnectionInfo TcpConnection::getInfo() const {
    ConnectionInfo info;
    info.remoteAddress = remoteAddress_;
    info.remotePort = remotePort_;
    info.localAddress = localAddress_;
    info.localPort = localPort_;
    info.state = state_;
    info.connectedAt = connectedAt_;
    info.bytesSent = bytesSent_;
    info.bytesReceived = bytesReceived_;
    return info;
}

bool TcpConnection::send(const std::vector<uint8_t>& data) {
    return send(data.data(), data.size());
}

bool TcpConnection::send(const std::string& data) {
    return send(data.data(), data.size());
}

bool TcpConnection::send(const void* data, size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected() || socket_ == INVALID_SOCKET) {
        return false;
    }
    
    size_t totalSent = 0;
    const char* buffer = static_cast<const char*>(data);
    
    while (totalSent < length) {
        int sent = ::send(socket_, buffer + totalSent, length - totalSent, 0);
        if (sent == SOCKET_ERROR) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                continue;
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
#endif
            handleError(ErrorCode::SendFailed, "Send failed");
            return false;
        }
        
        totalSent += sent;
        bytesSent_ += sent;
    }
    
    return true;
}

std::vector<uint8_t> TcpConnection::receive(size_t maxLength) {
    std::vector<uint8_t> buffer(maxLength);
    int received = receiveRaw(buffer.data(), maxLength);
    if (received > 0) {
        buffer.resize(received);
        return buffer;
    }
    return {};
}

std::string TcpConnection::receiveString(size_t maxLength) {
    std::vector<uint8_t> data = receive(maxLength);
    return std::string(data.begin(), data.end());
}

int TcpConnection::receiveRaw(void* buffer, size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected() || socket_ == INVALID_SOCKET) {
        return -1;
    }
    
    int received = ::recv(socket_, static_cast<char*>(buffer), length, 0);
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
    
    bytesReceived_ += received;
    return received;
}

void TcpConnection::sendAsync(const std::vector<uint8_t>& data, std::function<void(bool)> callback) {
    // Make a copy of the data and capture shared_ptr to keep connection alive
    auto self = shared_from_this();
    std::thread([self, data, callback]() {
        bool success = self->send(data);
        if (callback) {
            callback(success);
        }
    }).detach();
}

void TcpConnection::sendAsync(const std::string& data, std::function<void(bool)> callback) {
    // Make a copy of the data and capture shared_ptr to keep connection alive
    auto self = shared_from_this();
    std::thread([self, data, callback]() {
        bool success = self->send(data);
        if (callback) {
            callback(success);
        }
    }).detach();
}

void TcpConnection::receiveAsync(size_t maxLength, std::function<void(const std::vector<uint8_t>&)> callback) {
    std::thread([this, maxLength, callback]() {
        std::vector<uint8_t> data = receive(maxLength);
        if (callback) {
            callback(data);
        }
    }).detach();
}

bool TcpConnection::enableSsl(std::shared_ptr<SslContext> context) {
    // SSL implementation would go here
    sslContext_ = context;
    sslEnabled_ = true;
    return true;
}

void TcpConnection::startReceiveThread() {
    receiveThread_ = std::thread(&TcpConnection::receiveLoop, this);
}

void TcpConnection::receiveLoop() {
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
                try {
                    onDataReceived_(shared_from_this(), data);
                } catch (const std::exception& e) {
                    // Handle any exceptions in the callback gracefully
                    break;
                }
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

void TcpConnection::setState(ConnectionState state) {
    state_ = state;
}

void TcpConnection::handleError(ErrorCode error, const std::string& message) {
    setState(ConnectionState::Error);
    if (onError_) {
        onError_(shared_from_this(), error, message);
    }
}

bool TcpConnection::initializeLocalAddress() {
    if (socket_ == INVALID_SOCKET) {
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

} // namespace tcp
