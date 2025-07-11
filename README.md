# TCP Library - Comprehensive C++ TCP Networking Library

A full-featured, cross-platform C++ TCP networking library with support for both client and server applications. This library provides a modern, easy-to-use interface for TCP network programming with advanced features like SSL/TLS support, connection pooling, rate limiting, and message framing.

## Features

### Core Components
- **TcpSocket**: Base socket class with cross-platform support (Windows/Linux/macOS)
- **TcpClient**: TCP client with async operations, auto-reconnect, and heartbeat
- **TcpServer**: TCP server with connection management and broadcasting
- **TcpConnection**: Individual connection management with statistics

### Advanced Features
- **SSL/TLS Support**: Secure connections with OpenSSL integration
- **Asynchronous I/O**: Non-blocking operations with callback-based event handling
- **Connection Management**: Automatic connection lifecycle management
- **Message Framing**: Length-prefixed and delimiter-based message protocols
- **Connection Pooling**: Efficient connection reuse for high-performance applications
- **Rate Limiting**: Traffic control and bandwidth management
- **Auto-reconnect**: Automatic reconnection with configurable intervals
- **Heartbeat/Keep-alive**: Connection health monitoring
- **Thread Safety**: Safe for multi-threaded applications
- **Comprehensive Error Handling**: Detailed error reporting and recovery

### Utilities
- **Network Utils**: Address resolution, interface enumeration, port scanning
- **Protocol Helpers**: HTTP, WebSocket, JSON, Base64 utilities
- **Buffer Management**: Efficient memory management and circular buffers
- **Logging System**: Configurable logging with multiple levels
- **Statistics**: Connection and performance monitoring

## Requirements

### Minimum Requirements
- C++17 compatible compiler
- CMake 3.10 or higher
- Platform: Windows, Linux, macOS

### Optional Dependencies
- OpenSSL (for SSL/TLS support)
- GTest (for running tests)

## Installation

### Using CMake

```bash
# Clone the repository
git clone https://github.com/your-repo/tcp-library.git
cd tcp-library

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make -j4

# Install (optional)
sudo make install
```

### Build Options

```bash
# Enable SSL/TLS support (requires OpenSSL)
cmake -DTCP_SSL_SUPPORT=ON ..

# Build examples
cmake -DBUILD_EXAMPLES=ON ..

# Build tests
cmake -DBUILD_TESTS=ON ..

# Release build
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## Quick Start

### Basic TCP Client

```cpp
#include <tcp/tcp.h>
#include <iostream>

int main() {
    // Initialize library
    tcp::Library::initialize();
    
    // Create client
    tcp::TcpClient client;
    
    // Set up callbacks
    client.setOnConnected([]() {
        std::cout << "Connected!" << std::endl;
    });
    
    client.setOnDataReceived([](const std::vector<uint8_t>& data) {
        std::string message(data.begin(), data.end());
        std::cout << "Received: " << message << std::endl;
    });
    
    // Connect and send data
    if (client.connect("127.0.0.1", 8080)) {
        client.send("Hello, Server!");
        
        // Keep client running
        std::cin.get();
    }
    
    // Cleanup
    tcp::Library::cleanup();
    return 0;
}
```

### Basic TCP Server

```cpp
#include <tcp/tcp.h>
#include <iostream>

int main() {
    // Initialize library
    tcp::Library::initialize();
    
    // Create server
    tcp::TcpServer server;
    
    // Set up callbacks
    server.setOnConnected([](std::shared_ptr<tcp::TcpConnection> connection) {
        auto info = connection->getInfo();
        std::cout << "Client connected: " << info.remoteAddress << std::endl;
    });
    
    server.setOnDataReceived([](std::shared_ptr<tcp::TcpConnection> connection, 
                                const std::vector<uint8_t>& data) {
        std::string message(data.begin(), data.end());
        std::cout << "Received: " << message << std::endl;
        
        // Echo back
        connection->send("Echo: " + message);
    });
    
    // Start server
    if (server.start("127.0.0.1", 8080)) {
        std::cout << "Server started on port 8080" << std::endl;
        std::cin.get();
    }
    
    // Cleanup
    tcp::Library::cleanup();
    return 0;
}
```

## Advanced Usage

### SSL/TLS Client

```cpp
#include <tcp/tcp.h>

int main() {
    tcp::Library::initialize();
    
    // Create SSL context
    auto sslContext = tcp::SslContext::createClientContext();
    sslContext->loadCaCertificate("ca.pem");
    
    // Create client with SSL
    tcp::TcpClient client;
    client.enableSsl(sslContext);
    
    // Connect to secure server
    if (client.connect("secure.example.com", 443)) {
        client.send("GET / HTTP/1.1\r\nHost: secure.example.com\r\n\r\n");
        // ... handle response
    }
    
    tcp::Library::cleanup();
    return 0;
}
```

### Auto-reconnect Client

```cpp
#include <tcp/tcp.h>

int main() {
    tcp::Library::initialize();
    
    tcp::TcpClient client;
    
    // Enable auto-reconnect with 5-second interval
    client.enableAutoReconnect(true, std::chrono::seconds(5));
    
    // Enable heartbeat every 30 seconds
    client.enableHeartbeat(true, std::chrono::seconds(30));
    client.setHeartbeatData("PING");
    
    client.connect("127.0.0.1", 8080);
    
    // Client will automatically reconnect if connection is lost
    std::cin.get();
    
    tcp::Library::cleanup();
    return 0;
}
```

### Message Framing

```cpp
#include <tcp/tcp.h>

int main() {
    tcp::Library::initialize();
    
    // Create length-prefixed framer
    tcp::LengthPrefixedFramer framer(tcp::LengthPrefixedFramer::LengthType::UInt32);
    
    // Frame a message
    std::vector<uint8_t> message = {'H', 'e', 'l', 'l', 'o'};
    auto framedMessage = framer.frame(message);
    
    // Send framed message
    tcp::TcpClient client;
    if (client.connect("127.0.0.1", 8080)) {
        client.send(framedMessage);
    }
    
    tcp::Library::cleanup();
    return 0;
}
```

### Connection Pooling

```cpp
#include <tcp/tcp.h>

int main() {
    tcp::Library::initialize();
    
    // Create connection pool
    tcp::ConnectionPool pool(10); // Max 10 connections
    
    // Set connection factory
    pool.setConnectionFactory([]() {
        auto client = std::make_shared<tcp::TcpClient>();
        client->connect("127.0.0.1", 8080);
        return std::static_pointer_cast<tcp::TcpConnection>(client);
    });
    
    // Use pool
    auto connection = pool.acquire();
    if (connection) {
        connection->send("Hello from pool!");
        pool.release(connection);
    }
    
    tcp::Library::cleanup();
    return 0;
}
```

## API Reference

### TcpClient

#### Connection Management
- `bool connect(const std::string& address, uint16_t port)`
- `void disconnect()`
- `bool isConnected() const`
- `std::future<bool> connectAsync(const std::string& address, uint16_t port)`

#### Data Transmission
- `bool send(const std::vector<uint8_t>& data)`
- `bool send(const std::string& data)`
- `std::vector<uint8_t> receive(size_t maxLength = 4096)`
- `std::string receiveString(size_t maxLength = 4096)`

#### Callbacks
- `void setOnConnected(std::function<void()> callback)`
- `void setOnDisconnected(std::function<void()> callback)`
- `void setOnDataReceived(std::function<void(const std::vector<uint8_t>&)> callback)`
- `void setOnError(std::function<void(ErrorCode, const std::string&)> callback)`

#### Advanced Features
- `void enableAutoReconnect(bool enable, std::chrono::milliseconds interval)`
- `void enableHeartbeat(bool enable, std::chrono::milliseconds interval)`
- `bool enableSsl(std::shared_ptr<SslContext> context)`

### TcpServer

#### Server Management
- `bool start(const std::string& address, uint16_t port, int backlog = 10)`
- `void stop()`
- `bool isRunning() const`

#### Connection Management
- `std::vector<std::shared_ptr<TcpConnection>> getConnections() const`
- `size_t getConnectionCount() const`
- `void closeAllConnections()`

#### Broadcasting
- `void broadcast(const std::vector<uint8_t>& data)`
- `void broadcast(const std::string& data)`

#### Callbacks
- `void setOnConnected(OnConnectedCallback callback)`
- `void setOnDisconnected(OnDisconnectedCallback callback)`
- `void setOnDataReceived(OnDataReceivedCallback callback)`
- `void setOnError(OnErrorCallback callback)`

### TcpConnection

#### Connection Info
- `bool isConnected() const`
- `ConnectionState getState() const`
- `ConnectionInfo getInfo() const`

#### Data Transmission
- `bool send(const std::vector<uint8_t>& data)`
- `bool send(const std::string& data)`
- `std::vector<uint8_t> receive(size_t maxLength = 4096)`

## Examples

The library includes comprehensive examples in the `examples/` directory:

- **tcp_client_example.cpp**: Basic client usage
- **tcp_server_example.cpp**: Basic server usage  
- **echo_server.cpp**: Echo server with line-based protocol
- **echo_client.cpp**: Echo client
- **chat_server.cpp**: Multi-client chat server
- **chat_client.cpp**: Chat client with threading
- **file_transfer_server.cpp**: File transfer server
- **file_transfer_client.cpp**: File transfer client
- **http_server.cpp**: Simple HTTP server
- **ssl_client_example.cpp**: SSL/TLS client (requires OpenSSL)
- **ssl_server_example.cpp**: SSL/TLS server (requires OpenSSL)

### Building Examples

```bash
mkdir build
cd build
cmake -DBUILD_EXAMPLES=ON ..
make -j4

# Run examples
./examples/tcp_server_example
./examples/tcp_client_example
```

## Error Handling

The library provides comprehensive error handling through the `ErrorCode` enum:

```cpp
enum class ErrorCode {
    Success,
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
```

Error callbacks provide detailed error information:

```cpp
client.setOnError([](tcp::ErrorCode error, const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
    // Handle specific error types
    switch (error) {
        case tcp::ErrorCode::ConnectionFailed:
            // Handle connection failure
            break;
        case tcp::ErrorCode::Timeout:
            // Handle timeout
            break;
        // ... other error types
    }
});
```

## Performance Considerations

### Buffer Management
- Use appropriate buffer sizes for your use case
- Consider using circular buffers for streaming data
- Implement proper memory management for large data transfers

### Threading
- The library is thread-safe but consider your threading model
- Use connection pooling for high-concurrency scenarios
- Implement proper synchronization for shared resources

### Network Optimization
- Set appropriate socket options (buffer sizes, timeouts)
- Use non-blocking I/O for better performance
- Consider message batching for high-frequency communications

## Testing

Run the test suite to ensure everything works correctly:

```bash
mkdir build
cd build
cmake -DBUILD_TESTS=ON ..
make -j4
make test
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## License

This library is released under the MIT License. See LICENSE file for details.

## Support

For questions, bug reports, or feature requests, please open an issue on the GitHub repository.

## Changelog

### Version 1.0.0
- Initial release
- Core TCP client/server functionality
- SSL/TLS support
- Message framing
- Connection pooling
- Rate limiting
- Comprehensive examples and documentation
