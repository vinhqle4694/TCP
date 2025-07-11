#pragma once

#include "tcp_socket.h"
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

namespace tcp {

class TcpClient : public TcpSocket {
public:
    TcpClient();
    ~TcpClient();

    // Connection management
    bool connect(const std::string& address, uint16_t port);
    bool connect(const std::string& address, uint16_t port, std::chrono::milliseconds timeout);
    void disconnect();
    bool isConnected() const;
    
    // Async connection
    std::future<bool> connectAsync(const std::string& address, uint16_t port);
    std::future<bool> connectAsync(const std::string& address, uint16_t port, std::chrono::milliseconds timeout);
    void disconnectAsync();

    // Connection info
    std::string getRemoteAddress() const { return remoteAddress_; }
    uint16_t getRemotePort() const { return remotePort_; }
    std::string getLocalAddress() const { return localAddress_; }
    uint16_t getLocalPort() const { return localPort_; }
    ConnectionState getState() const { return state_; }

    // Data transmission
    bool send(const std::vector<uint8_t>& data);
    bool send(const std::string& data);
    bool send(const void* data, size_t length);
    
    std::vector<uint8_t> receive(size_t maxLength = 4096);
    std::string receiveString(size_t maxLength = 4096);
    int receiveRaw(void* buffer, size_t length);

    // Async operations
    void sendAsync(const std::vector<uint8_t>& data, std::function<void(bool)> callback = nullptr);
    void sendAsync(const std::string& data, std::function<void(bool)> callback = nullptr);
    void receiveAsync(size_t maxLength, std::function<void(const std::vector<uint8_t>&)> callback);

    // Callbacks
    void setOnConnected(std::function<void()> callback) { onConnected_ = callback; }
    void setOnDisconnected(std::function<void()> callback) { onDisconnected_ = callback; }
    void setOnDataReceived(std::function<void(const std::vector<uint8_t>&)> callback) { onDataReceived_ = callback; }
    void setOnError(std::function<void(ErrorCode, const std::string&)> callback) { onError_ = callback; }

    // SSL/TLS
    bool enableSsl(std::shared_ptr<SslContext> context = nullptr);
    bool isSslEnabled() const { return sslEnabled_; }

    // Reconnection
    void enableAutoReconnect(bool enable, std::chrono::milliseconds interval = std::chrono::milliseconds{5000});
    bool isAutoReconnectEnabled() const { return autoReconnect_; }
    
    // Statistics
    struct Statistics {
        size_t totalConnections = 0;
        size_t reconnections = 0;
        size_t bytesReceived = 0;
        size_t bytesSent = 0;
        std::chrono::system_clock::time_point lastConnectedAt;
        std::chrono::milliseconds totalConnectedTime{0};
    };
    Statistics getStatistics() const;

    // Heartbeat/Keep-alive
    void enableHeartbeat(bool enable, std::chrono::milliseconds interval = std::chrono::milliseconds{30000});
    void setHeartbeatData(const std::vector<uint8_t>& data);
    void setHeartbeatData(const std::string& data);

private:
    std::string remoteAddress_;
    uint16_t remotePort_;
    std::string localAddress_;
    uint16_t localPort_;
    std::atomic<ConnectionState> state_;
    std::chrono::system_clock::time_point connectedAt_;
    
    // SSL/TLS
    bool sslEnabled_;
    std::shared_ptr<SslContext> sslContext_;
    void* sslHandle_; // SSL* handle
    
    // Threading
    std::thread receiveThread_;
    std::thread reconnectThread_;
    std::thread heartbeatThread_;
    std::atomic<bool> shouldStop_;
    
    // Callbacks
    std::function<void()> onConnected_;
    std::function<void()> onDisconnected_;
    std::function<void(const std::vector<uint8_t>&)> onDataReceived_;
    std::function<void(ErrorCode, const std::string&)> onError_;
    
    // Auto-reconnect
    std::atomic<bool> autoReconnect_;
    std::chrono::milliseconds reconnectInterval_;
    std::condition_variable reconnectCondition_;
    std::mutex reconnectMutex_;
    
    // Heartbeat
    std::atomic<bool> heartbeatEnabled_;
    std::chrono::milliseconds heartbeatInterval_;
    std::vector<uint8_t> heartbeatData_;
    std::condition_variable heartbeatCondition_;
    std::mutex heartbeatMutex_;
    
    // Statistics
    mutable Statistics statistics_;
    mutable std::mutex statisticsMutex_;
    
    // Internal methods
    bool connectInternal(const std::string& address, uint16_t port, std::chrono::milliseconds timeout);
    void setState(ConnectionState state);
    void startReceiveThread();
    void receiveLoop();
    void reconnectLoop();
    void heartbeatLoop();
    void handleError(ErrorCode error, const std::string& message);
    bool initializeLocalAddress();
    void updateStatistics();
    void cleanupSsl();
    bool setupSsl();
    int sendSsl(const void* data, size_t length);
    int receiveSsl(void* buffer, size_t length);
};

} // namespace tcp
