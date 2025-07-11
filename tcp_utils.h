#pragma once

#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace tcp {

// Forward declarations
class TcpConnection;

// Message framing for protocols
class MessageFramer {
public:
    virtual ~MessageFramer() = default;
    virtual std::vector<uint8_t> frame(const std::vector<uint8_t>& data) = 0;
    virtual std::vector<std::vector<uint8_t>> unframe(const std::vector<uint8_t>& data) = 0;
    virtual bool isComplete(const std::vector<uint8_t>& data) = 0;
    virtual void reset() = 0;
};

// Length-prefixed message framer
class LengthPrefixedFramer : public MessageFramer {
public:
    enum class LengthType {
        UInt8,
        UInt16,
        UInt32,
        UInt64
    };

    LengthPrefixedFramer(LengthType lengthType = LengthType::UInt32, bool bigEndian = true);
    
    std::vector<uint8_t> frame(const std::vector<uint8_t>& data) override;
    std::vector<std::vector<uint8_t>> unframe(const std::vector<uint8_t>& data) override;
    bool isComplete(const std::vector<uint8_t>& data) override;
    void reset() override;

private:
    LengthType lengthType_;
    bool bigEndian_;
    std::vector<uint8_t> buffer_;
    size_t expectedLength_;
    bool lengthReceived_;
    
    size_t getLengthSize() const;
    void writeLength(std::vector<uint8_t>& data, size_t length) const;
    size_t readLength(const std::vector<uint8_t>& data, size_t offset) const;
};

// Delimiter-based message framer
class DelimiterFramer : public MessageFramer {
public:
    DelimiterFramer(const std::vector<uint8_t>& delimiter, bool includeDelimiter = false);
    DelimiterFramer(const std::string& delimiter, bool includeDelimiter = false);
    
    std::vector<uint8_t> frame(const std::vector<uint8_t>& data) override;
    std::vector<std::vector<uint8_t>> unframe(const std::vector<uint8_t>& data) override;
    bool isComplete(const std::vector<uint8_t>& data) override;
    void reset() override;

private:
    std::vector<uint8_t> delimiter_;
    bool includeDelimiter_;
    std::vector<uint8_t> buffer_;
    
    size_t findDelimiter(const std::vector<uint8_t>& data, size_t startPos = 0) const;
};

// Connection pool for managing multiple connections
class ConnectionPool {
public:
    ConnectionPool(size_t maxConnections = 10);
    ~ConnectionPool();

    // Pool management
    std::shared_ptr<TcpConnection> acquire();
    void release(std::shared_ptr<TcpConnection> connection);
    void clear();
    
    // Configuration
    void setMaxConnections(size_t maxConnections);
    size_t getMaxConnections() const { return maxConnections_; }
    size_t getActiveConnections() const;
    size_t getIdleConnections() const;

    // Connection factory
    void setConnectionFactory(std::function<std::shared_ptr<TcpConnection>()> factory);

private:
    size_t maxConnections_;
    std::vector<std::shared_ptr<TcpConnection>> idleConnections_;
    std::vector<std::shared_ptr<TcpConnection>> activeConnections_;
    std::function<std::shared_ptr<TcpConnection>()> connectionFactory_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};

// Rate limiter for controlling data flow
class RateLimiter {
public:
    RateLimiter(size_t bytesPerSecond, size_t bucketSize = 0);
    
    // Rate limiting
    bool allowBytes(size_t bytes);
    std::chrono::milliseconds getDelay(size_t bytes) const;
    void waitForBytes(size_t bytes);
    
    // Configuration
    void setRate(size_t bytesPerSecond);
    size_t getRate() const { return bytesPerSecond_; }
    void setBucketSize(size_t bucketSize);
    size_t getBucketSize() const { return bucketSize_; }
    
    // Statistics
    size_t getAvailableBytes() const;
    double getUtilization() const;
    void reset();

private:
    size_t bytesPerSecond_;
    size_t bucketSize_;
    std::atomic<size_t> availableBytes_;
    std::chrono::steady_clock::time_point lastRefill_;
    mutable std::mutex mutex_;
    
    void refillBucket();
};

// Buffer management utilities
class BufferManager {
public:
    static std::vector<uint8_t> allocateBuffer(size_t size);
    static void deallocateBuffer(std::vector<uint8_t>& buffer);
    static std::vector<uint8_t> resizeBuffer(std::vector<uint8_t>& buffer, size_t newSize);
    static void copyBuffer(const std::vector<uint8_t>& source, std::vector<uint8_t>& destination);
    static std::vector<uint8_t> concatenateBuffers(const std::vector<std::vector<uint8_t>>& buffers);
    static std::vector<std::vector<uint8_t>> splitBuffer(const std::vector<uint8_t>& buffer, size_t chunkSize);
    
    // Circular buffer
    class CircularBuffer {
    public:
        CircularBuffer(size_t capacity);
        
        size_t write(const void* data, size_t length);
        size_t read(void* data, size_t length);
        size_t peek(void* data, size_t length) const;
        void skip(size_t length);
        
        size_t getCapacity() const { return capacity_; }
        size_t getSize() const { return size_; }
        size_t getAvailableSpace() const { return capacity_ - size_; }
        bool isEmpty() const { return size_ == 0; }
        bool isFull() const { return size_ == capacity_; }
        void clear();
        
    private:
        std::vector<uint8_t> buffer_;
        size_t capacity_;
        size_t size_;
        size_t head_;
        size_t tail_;
        mutable std::mutex mutex_;
    };
};

// Network utilities
class NetworkUtils {
public:
    // Address utilities
    static bool isValidIPv4(const std::string& ip);
    static bool isValidIPv6(const std::string& ip);
    static bool isValidHostname(const std::string& hostname);
    static bool isValidPort(int port);
    
    // DNS resolution
    static std::vector<std::string> resolveHostname(const std::string& hostname);
    static std::string getHostnameFromIP(const std::string& ip);
    
    // Network interface information
    static std::vector<std::string> getNetworkInterfaces();
    static std::string getInterfaceAddress(const std::string& interfaceName);
    static std::vector<std::string> getInterfaceAddresses(const std::string& interfaceName);
    
    // Port utilities
    static bool isPortAvailable(const std::string& address, uint16_t port);
    static uint16_t findAvailablePort(const std::string& address, uint16_t startPort = 8000);
    static std::vector<uint16_t> findAvailablePorts(const std::string& address, size_t count, uint16_t startPort = 8000);
    
    // Address conversion
    static std::string ipv4ToString(uint32_t ip);
    static uint32_t ipv4FromString(const std::string& ip);
    static std::string ipv6ToString(const std::vector<uint8_t>& ip);
    static std::vector<uint8_t> ipv6FromString(const std::string& ip);
    
    // MAC address
    static std::string getMacAddress(const std::string& interfaceName);
    static std::vector<std::string> getAllMacAddresses();
    
    // Network statistics
    struct InterfaceStats {
        std::string name;
        std::string address;
        std::string macAddress;
        uint64_t bytesReceived;
        uint64_t bytesSent;
        uint64_t packetsReceived;
        uint64_t packetsSent;
        uint64_t errorsReceived;
        uint64_t errorsSent;
        bool isUp;
        bool isLoopback;
    };
    
    static std::vector<InterfaceStats> getInterfaceStatistics();
    static InterfaceStats getInterfaceStatistics(const std::string& interfaceName);
};

// Protocol helpers
class ProtocolHelper {
public:
    // HTTP utilities
    static std::string buildHttpRequest(const std::string& method, const std::string& path,
                                      const std::vector<std::pair<std::string, std::string>>& headers = {},
                                      const std::string& body = "");
    static std::string buildHttpResponse(int statusCode, const std::string& reasonPhrase,
                                       const std::vector<std::pair<std::string, std::string>>& headers = {},
                                       const std::string& body = "");
    
    // WebSocket utilities
    static std::string buildWebSocketHandshake(const std::string& host, const std::string& path,
                                             const std::vector<std::pair<std::string, std::string>>& headers = {});
    static std::vector<uint8_t> buildWebSocketFrame(const std::vector<uint8_t>& payload, bool mask = true);
    static std::vector<uint8_t> parseWebSocketFrame(const std::vector<uint8_t>& data);
    
    // JSON utilities
    static std::string escapeJson(const std::string& str);
    static std::string unescapeJson(const std::string& str);
    static bool isValidJson(const std::string& json);
    
    // Base64 utilities
    static std::string base64Encode(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> base64Decode(const std::string& encoded);
    
    // URL utilities
    static std::string urlEncode(const std::string& str);
    static std::string urlDecode(const std::string& str);
    
    // Hash utilities
    static std::string sha1Hash(const std::vector<uint8_t>& data);
    static std::string sha256Hash(const std::vector<uint8_t>& data);
    static std::string md5Hash(const std::vector<uint8_t>& data);
    
    // Random utilities
    static std::vector<uint8_t> generateRandomBytes(size_t length);
    static std::string generateRandomString(size_t length, const std::string& charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
};

// Logging utilities
class Logger {
public:
    enum class Level {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };
    
    static void setLevel(Level level);
    static Level getLevel();
    static void setOutput(std::function<void(Level, const std::string&)> output);
    
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    static void critical(const std::string& message);
    
    template<typename... Args>
    static void debug(const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void info(const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void warning(const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void error(const std::string& format, Args&&... args);
    
    template<typename... Args>
    static void critical(const std::string& format, Args&&... args);

private:
    static Level currentLevel_;
    static std::function<void(Level, const std::string&)> output_;
    static std::mutex mutex_;
    
    static void log(Level level, const std::string& message);
    static std::string formatMessage(Level level, const std::string& message);
    static std::string levelToString(Level level);
    
    template<typename... Args>
    static std::string format(const std::string& format, Args&&... args);
};

} // namespace tcp
