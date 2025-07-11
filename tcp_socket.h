#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <queue>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <netdb.h>
    typedef int socket_t;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

namespace tcp {

// Forward declarations
class TcpServer;
class TcpClient;
class TcpConnection;
class SslContext;

// Error codes
enum class ErrorCode {
    Success = 0,
    InvalidSocket,
    ConnectionFailed,
    ConnectionClosed,
    SendFailed,
    ReceiveFailed,
    BindFailed,
    ListenFailed,
    AcceptFailed,
    InvalidAddress,
    Timeout,
    WouldBlock,
    SslError,
    UnknownError
};

// Connection state
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

// Socket options
struct SocketOptions {
    bool reuseAddress = true;
    bool keepAlive = true;
    bool noDelay = true;
    int sendBufferSize = 8192;
    int receiveBufferSize = 8192;
    std::chrono::milliseconds sendTimeout{5000};
    std::chrono::milliseconds receiveTimeout{5000};
    std::chrono::milliseconds connectTimeout{10000};
};

// Connection info
struct ConnectionInfo {
    std::string remoteAddress;
    uint16_t remotePort;
    std::string localAddress;
    uint16_t localPort;
    ConnectionState state;
    std::chrono::system_clock::time_point connectedAt;
    size_t bytesSent = 0;
    size_t bytesReceived = 0;
};

// Callback types
using OnConnectedCallback = std::function<void(std::shared_ptr<TcpConnection>)>;
using OnDisconnectedCallback = std::function<void(std::shared_ptr<TcpConnection>)>;
using OnDataReceivedCallback = std::function<void(std::shared_ptr<TcpConnection>, const std::vector<uint8_t>&)>;
using OnErrorCallback = std::function<void(std::shared_ptr<TcpConnection>, ErrorCode, const std::string&)>;

// Base TCP socket class
class TcpSocket {
public:
    TcpSocket();
    virtual ~TcpSocket();

    // Non-copyable
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // Movable
    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;

    // Socket management
    bool create();
    void close();
    bool isValid() const;
    socket_t getHandle() const { return socket_; }

    // Socket options
    bool setSocketOptions(const SocketOptions& options);
    SocketOptions getSocketOptions() const;

    // Non-blocking mode
    bool setNonBlocking(bool nonBlocking);
    bool isNonBlocking() const { return nonBlocking_; }

    // Address utilities
    static std::string getLocalAddress();
    static std::vector<std::string> getLocalAddresses();
    static bool resolveAddress(const std::string& hostname, std::string& ip);

protected:
    socket_t socket_;
    bool nonBlocking_;
    SocketOptions options_;
    mutable std::mutex mutex_;

    // Internal helpers
    bool setSocketOption(int level, int optname, const void* optval, socklen_t optlen);
    bool getSocketOption(int level, int optname, void* optval, socklen_t* optlen) const;
    bool setSocketOptionsInternal(const SocketOptions& options);
    ErrorCode getLastError() const;
    std::string errorToString(ErrorCode error) const;

private:
    void initialize();
    void cleanup();
    
#ifdef _WIN32
    static bool wsaInitialized_;
    static std::mutex wsaMutex_;
#endif
};

// TCP Connection class
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(socket_t socket, const std::string& remoteAddress, uint16_t remotePort);
    ~TcpConnection();

    // Non-copyable, non-movable
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&&) = delete;
    TcpConnection& operator=(TcpConnection&&) = delete;

    // Connection management
    void close();
    bool isConnected() const;
    ConnectionState getState() const;
    ConnectionInfo getInfo() const;

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
    void setOnDataReceived(OnDataReceivedCallback callback) { onDataReceived_ = callback; }
    void setOnDisconnected(OnDisconnectedCallback callback) { onDisconnected_ = callback; }
    void setOnError(OnErrorCallback callback) { onError_ = callback; }

    // SSL/TLS
    bool enableSsl(std::shared_ptr<SslContext> context);
    bool isSslEnabled() const { return sslEnabled_; }

private:
    socket_t socket_;
    std::string remoteAddress_;
    uint16_t remotePort_;
    std::string localAddress_;
    uint16_t localPort_;
    std::atomic<ConnectionState> state_;
    std::chrono::system_clock::time_point connectedAt_;
    std::atomic<size_t> bytesSent_;
    std::atomic<size_t> bytesReceived_;
    
    bool sslEnabled_;
    std::shared_ptr<SslContext> sslContext_;
    void* sslHandle_; // SSL* handle
    
    mutable std::mutex mutex_;
    std::thread receiveThread_;
    std::atomic<bool> shouldStop_;
    
    // Callbacks
    OnDataReceivedCallback onDataReceived_;
    OnDisconnectedCallback onDisconnected_;
    OnErrorCallback onError_;
    
    // Internal methods
    void startReceiveThread();
    void receiveLoop();
    void setState(ConnectionState state);
    void handleError(ErrorCode error, const std::string& message);
    bool initializeLocalAddress();
};

} // namespace tcp
