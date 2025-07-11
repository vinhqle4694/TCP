#pragma once

// Main TCP library header - includes all components
#include "tcp_socket.h"
#include "tcp_client.h"
#include "tcp_server.h"
#include "ssl_context.h"
#include "tcp_utils.h"

/**
 * @file tcp.h
 * @brief Comprehensive C++ TCP Library
 * 
 * This library provides a complete TCP networking solution with the following features:
 * 
 * Core Components:
 * - TcpSocket: Base socket class with cross-platform support
 * - TcpClient: TCP client with async operations, auto-reconnect, and heartbeat
 * - TcpServer: TCP server with connection management and broadcasting
 * - TcpConnection: Individual connection management
 * 
 * Security:
 * - SSL/TLS support with OpenSSL integration
 * - Certificate management and validation
 * - Secure client/server connections
 * 
 * Utilities:
 * - Message framing (length-prefixed, delimiter-based)
 * - Connection pooling
 * - Rate limiting
 * - Buffer management
 * - Network utilities
 * - Protocol helpers (HTTP, WebSocket, JSON, Base64)
 * - Logging system
 * 
 * Features:
 * - Cross-platform (Windows, Linux, macOS)
 * - Thread-safe operations
 * - Asynchronous I/O
 * - Connection management
 * - Error handling
 * - Statistics and monitoring
 * - Callback-based event handling
 * 
 * Usage Examples:
 * 
 * @code
 * // TCP Client Example
 * tcp::TcpClient client;
 * client.setOnConnected([]() {
 *     std::cout << "Connected to server!" << std::endl;
 * });
 * client.setOnDataReceived([](const std::vector<uint8_t>& data) {
 *     std::cout << "Received: " << std::string(data.begin(), data.end()) << std::endl;
 * });
 * 
 * if (client.connect("127.0.0.1", 8080)) {
 *     client.send("Hello, Server!");
 * }
 * @endcode
 * 
 * @code
 * // TCP Server Example
 * tcp::TcpServer server;
 * server.setOnConnected([](std::shared_ptr<tcp::TcpConnection> connection) {
 *     std::cout << "Client connected: " << connection->getInfo().remoteAddress << std::endl;
 * });
 * server.setOnDataReceived([](std::shared_ptr<tcp::TcpConnection> connection, const std::vector<uint8_t>& data) {
 *     std::string message(data.begin(), data.end());
 *     std::cout << "Received: " << message << std::endl;
 *     connection->send("Echo: " + message);
 * });
 * 
 * server.start("127.0.0.1", 8080);
 * @endcode
 * 
 * @code
 * // SSL/TLS Example
 * auto sslContext = tcp::SslContext::createClientContext();
 * sslContext->loadCaCertificate("ca.pem");
 * 
 * tcp::TcpClient client;
 * client.enableSsl(sslContext);
 * client.connect("secure.example.com", 443);
 * @endcode
 */

namespace tcp {

// Library version information
struct Version {
    static constexpr int MAJOR = 1;
    static constexpr int MINOR = 0;
    static constexpr int PATCH = 0;
    
    static std::string getString() {
        return std::to_string(MAJOR) + "." + std::to_string(MINOR) + "." + std::to_string(PATCH);
    }
    
    static int getNumber() {
        return MAJOR * 10000 + MINOR * 100 + PATCH;
    }
};

// Library initialization and cleanup
class Library {
public:
    /**
     * @brief Initialize the TCP library
     * 
     * This function must be called before using any library functions.
     * It initializes platform-specific networking components.
     * 
     * @return true if initialization was successful, false otherwise
     */
    static bool initialize();
    
    /**
     * @brief Cleanup the TCP library
     * 
     * This function should be called when the library is no longer needed.
     * It cleans up platform-specific networking components.
     */
    static void cleanup();
    
    /**
     * @brief Check if the library is initialized
     * 
     * @return true if the library is initialized, false otherwise
     */
    static bool isInitialized();
    
    /**
     * @brief Get library version information
     * 
     * @return Version structure containing version information
     */
    static Version getVersion() { return Version{}; }
    
    /**
     * @brief Get library build information
     * 
     * @return String containing build information
     */
    static std::string getBuildInfo();
    
    /**
     * @brief Get supported features
     * 
     * @return Vector of supported feature names
     */
    static std::vector<std::string> getSupportedFeatures();

private:
    static bool initialized_;
    static std::mutex mutex_;
};

// Global library functions
namespace global {

/**
 * @brief Set global logger output function
 * 
 * @param output Function to handle log output
 */
void setLogOutput(std::function<void(Logger::Level, const std::string&)> output);

/**
 * @brief Set global log level
 * 
 * @param level Minimum log level to output
 */
void setLogLevel(Logger::Level level);

/**
 * @brief Get last error message
 * 
 * @return Last error message from any library component
 */
std::string getLastError();

/**
 * @brief Set default socket options
 * 
 * @param options Default socket options for new connections
 */
void setDefaultSocketOptions(const SocketOptions& options);

/**
 * @brief Get default socket options
 * 
 * @return Current default socket options
 */
SocketOptions getDefaultSocketOptions();

} // namespace global

} // namespace tcp

// Convenience macros
#define TCP_VERSION_MAJOR tcp::Version::MAJOR
#define TCP_VERSION_MINOR tcp::Version::MINOR
#define TCP_VERSION_PATCH tcp::Version::PATCH
#define TCP_VERSION_STRING tcp::Version::getString()
#define TCP_VERSION_NUMBER tcp::Version::getNumber()

// Feature detection macros
#ifdef TCP_SSL_SUPPORT
#define TCP_HAS_SSL 1
#else
#define TCP_HAS_SSL 0
#endif

#ifdef _WIN32
#define TCP_PLATFORM_WINDOWS 1
#define TCP_PLATFORM_POSIX 0
#else
#define TCP_PLATFORM_WINDOWS 0
#define TCP_PLATFORM_POSIX 1
#endif
