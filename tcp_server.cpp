#include "tcp_server.h"
#include <iostream>
#include <algorithm>
#include <cstring>

namespace tcp {

TcpServer::TcpServer() 
    : localPort_(0), running_(false), shouldStop_(false),
      sslEnabled_(false), sslContext_(nullptr) {
    statistics_.startTime = std::chrono::system_clock::now();
}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::bind(const std::string& address, uint16_t port) {
    if (running_) {
        return false;
    }
    
    if (!create()) {
        return false;
    }
    
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (address.empty() || address == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        localAddress_ = "0.0.0.0";
    } else {
        if (inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr) <= 0) {
            return false;
        }
        localAddress_ = address;
    }
    
    if (::bind(socket_, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        return false;
    }
    
    localPort_ = port;
    return true;
}

bool TcpServer::bind(uint16_t port) {
    return bind("0.0.0.0", port);
}

bool TcpServer::listen(int backlog) {
    if (!isValid()) {
        return false;
    }
    
    if (::listen(socket_, backlog) == SOCKET_ERROR) {
        return false;
    }
    
    return true;
}

bool TcpServer::start(const std::string& address, uint16_t port, int backlog) {
    if (!bind(address, port)) {
        return false;
    }
    
    if (!listen(backlog)) {
        return false;
    }
    
    running_ = true;
    shouldStop_ = false;
    
    // Start accept thread
    acceptThread_ = std::thread(&TcpServer::acceptLoop, this);
    
    // Start cleanup thread
    cleanupThread_ = std::thread(&TcpServer::cleanupLoop, this);
    
    return true;
}

void TcpServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    shouldStop_ = true;
    
    // Close server socket to unblock accept
    close();
    
    // Notify cleanup thread
    {
        std::lock_guard<std::mutex> lock(stopMutex_);
        stopCondition_.notify_all();
    }
    
    // Wait for threads to finish
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }
    
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
    
    // Close all connections
    closeAllConnections();
}

std::shared_ptr<TcpConnection> TcpServer::acceptConnection() {
    if (!isValid() || !running_) {
        return nullptr;
    }
    
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    socket_t clientSocket = ::accept(socket_, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
    if (clientSocket == INVALID_SOCKET) {
        return nullptr;
    }
    
    std::string clientAddress = inet_ntoa(clientAddr.sin_addr);
    uint16_t clientPort = ntohs(clientAddr.sin_port);
    
    auto connection = std::make_shared<TcpConnection>(clientSocket, clientAddress, clientPort);
    setupConnectionCallbacks(connection);
    
    return connection;
}

std::vector<std::shared_ptr<TcpConnection>> TcpServer::getConnections() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return connections_;
}

size_t TcpServer::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return connections_.size();
}

void TcpServer::closeConnection(std::shared_ptr<TcpConnection> connection) {
    if (connection) {
        connection->close();
        removeConnection(connection);
    }
}

void TcpServer::closeAllConnections() {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    for (auto& connection : connections_) {
        if (connection) {
            connection->close();
        }
    }
    
    connections_.clear();
}

void TcpServer::enableSsl(std::shared_ptr<SslContext> context) {
    sslContext_ = context;
    sslEnabled_ = true;
}

void TcpServer::startAsync() {
    std::thread([this]() {
        acceptLoop();
    }).detach();
}

void TcpServer::stopAsync() {
    std::thread([this]() {
        stop();
    }).detach();
}

void TcpServer::broadcast(const std::vector<uint8_t>& data) {
    broadcast(data.data(), data.size());
}

void TcpServer::broadcast(const std::string& data) {
    broadcast(data.data(), data.size());
}

void TcpServer::broadcast(const void* data, size_t length) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    for (auto& connection : connections_) {
        if (connection && connection->isConnected()) {
            connection->send(data, length);
        }
    }
}

TcpServer::Statistics TcpServer::getStatistics() const {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    
    Statistics stats = statistics_;
    stats.activeConnections = getConnectionCount();
    
    // Calculate total bytes from all connections
    {
        std::lock_guard<std::mutex> connLock(connectionsMutex_);
        for (const auto& connection : connections_) {
            if (connection) {
                auto info = connection->getInfo();
                stats.totalBytesReceived += info.bytesReceived;
                stats.totalBytesSent += info.bytesSent;
            }
        }
    }
    
    return stats;
}

void TcpServer::acceptLoop() {
    while (!shouldStop_ && running_) {
        auto connection = acceptConnection();
        if (connection) {
            handleNewConnection(connection);
        } else {
            // Brief pause to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void TcpServer::cleanupLoop() {
    while (!shouldStop_) {
        // Clean up disconnected connections
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            connections_.erase(
                std::remove_if(connections_.begin(), connections_.end(),
                    [](const std::shared_ptr<TcpConnection>& conn) {
                        return !conn || !conn->isConnected();
                    }),
                connections_.end()
            );
        }
        
        // Wait for stop signal or timeout
        std::unique_lock<std::mutex> lock(stopMutex_);
        stopCondition_.wait_for(lock, std::chrono::seconds(5), [this] { return shouldStop_.load(); });
    }
}

void TcpServer::handleNewConnection(std::shared_ptr<TcpConnection> connection) {
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.push_back(connection);
    }
    
    {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.totalConnections++;
    }
    
    // Enable SSL if configured
    if (sslEnabled_ && sslContext_) {
        connection->enableSsl(sslContext_);
    }
    
    if (onConnected_) {
        onConnected_(connection);
    }
}

void TcpServer::handleDisconnection(std::shared_ptr<TcpConnection> connection) {
    removeConnection(connection);
    
    if (onDisconnected_) {
        onDisconnected_(connection);
    }
}

void TcpServer::removeConnection(std::shared_ptr<TcpConnection> connection) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [connection](const std::shared_ptr<TcpConnection>& conn) {
                return conn == connection;
            }),
        connections_.end()
    );
}

void TcpServer::updateStatistics() {
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.activeConnections = getConnectionCount();
}

void TcpServer::setupConnectionCallbacks(std::shared_ptr<TcpConnection> connection) {
    // Set up connection callbacks
    connection->setOnDataReceived([this](std::shared_ptr<TcpConnection> conn, const std::vector<uint8_t>& data) {
        if (onDataReceived_) {
            onDataReceived_(conn, data);
        }
    });
    
    connection->setOnDisconnected([this](std::shared_ptr<TcpConnection> conn) {
        handleDisconnection(conn);
    });
    
    connection->setOnError([this](std::shared_ptr<TcpConnection> conn, ErrorCode error, const std::string& message) {
        if (onError_) {
            onError_(conn, error, message);
        }
    });
}

} // namespace tcp
