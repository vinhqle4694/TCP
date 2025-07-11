#pragma once

#include "tcp_socket.h"
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace tcp {

class TcpServer : public TcpSocket {
public:
    TcpServer();
    ~TcpServer();

    // Server lifecycle
    bool bind(const std::string& address, uint16_t port);
    bool bind(uint16_t port); // Bind to all interfaces
    bool listen(int backlog = 10);
    bool start(const std::string& address, uint16_t port, int backlog = 10);
    void stop();
    bool isRunning() const { return running_; }

    // Connection management
    std::shared_ptr<TcpConnection> acceptConnection();
    std::vector<std::shared_ptr<TcpConnection>> getConnections() const;
    size_t getConnectionCount() const;
    void closeConnection(std::shared_ptr<TcpConnection> connection);
    void closeAllConnections();

    // Server info
    std::string getLocalAddress() const { return localAddress_; }
    uint16_t getLocalPort() const { return localPort_; }
    
    // Callbacks
    void setOnConnected(OnConnectedCallback callback) { onConnected_ = callback; }
    void setOnDisconnected(OnDisconnectedCallback callback) { onDisconnected_ = callback; }
    void setOnDataReceived(OnDataReceivedCallback callback) { onDataReceived_ = callback; }
    void setOnError(OnErrorCallback callback) { onError_ = callback; }

    // SSL/TLS
    void enableSsl(std::shared_ptr<SslContext> context);
    bool isSslEnabled() const { return sslEnabled_; }

    // Async operations
    void startAsync();
    void stopAsync();

    // Broadcast to all connections
    void broadcast(const std::vector<uint8_t>& data);
    void broadcast(const std::string& data);
    void broadcast(const void* data, size_t length);

    // Statistics
    struct Statistics {
        size_t totalConnections = 0;
        size_t activeConnections = 0;
        size_t totalBytesReceived = 0;
        size_t totalBytesSent = 0;
        std::chrono::system_clock::time_point startTime;
    };
    Statistics getStatistics() const;

private:
    std::string localAddress_;
    uint16_t localPort_;
    std::atomic<bool> running_;
    std::atomic<bool> shouldStop_;
    
    // Connection management
    std::vector<std::shared_ptr<TcpConnection>> connections_;
    mutable std::mutex connectionsMutex_;
    
    // SSL/TLS
    bool sslEnabled_;
    std::shared_ptr<SslContext> sslContext_;
    
    // Callbacks
    OnConnectedCallback onConnected_;
    OnDisconnectedCallback onDisconnected_;
    OnDataReceivedCallback onDataReceived_;
    OnErrorCallback onError_;
    
    // Threading
    std::thread acceptThread_;
    std::thread cleanupThread_;
    std::condition_variable stopCondition_;
    std::mutex stopMutex_;
    
    // Statistics
    mutable Statistics statistics_;
    mutable std::mutex statisticsMutex_;
    
    // Internal methods
    void acceptLoop();
    void cleanupLoop();
    void handleNewConnection(std::shared_ptr<TcpConnection> connection);
    void handleDisconnection(std::shared_ptr<TcpConnection> connection);
    void removeConnection(std::shared_ptr<TcpConnection> connection);
    void updateStatistics();
    void setupConnectionCallbacks(std::shared_ptr<TcpConnection> connection);
};

} // namespace tcp
