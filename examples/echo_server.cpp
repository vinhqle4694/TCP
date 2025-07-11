#include "../tcp.h"
#include <iostream>
#include <string>
#include <sstream>

int main() {
    std::cout << "Echo Server Example" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Initialize library
    if (!tcp::Library::initialize()) {
        std::cerr << "Failed to initialize TCP library" << std::endl;
        return 1;
    }
    
    // Create server
    tcp::TcpServer server;
    
    // Set up message framer for line-based protocol
    auto framer = std::make_shared<tcp::DelimiterFramer>("\r\n", false);
    
    // Set up callbacks
    server.setOnConnected([](std::shared_ptr<tcp::TcpConnection> connection) {
        auto info = connection->getInfo();
        std::cout << "Client connected from " << info.remoteAddress << ":" << info.remotePort << std::endl;
        
        // Send welcome message
        connection->send("Welcome to Echo Server! Type 'quit' to disconnect.\r\n");
    });
    
    server.setOnDisconnected([](std::shared_ptr<tcp::TcpConnection> connection) {
        auto info = connection->getInfo();
        std::cout << "Client disconnected: " << info.remoteAddress << ":" << info.remotePort << std::endl;
    });
    
    server.setOnDataReceived([framer](std::shared_ptr<tcp::TcpConnection> connection, const std::vector<uint8_t>& data) {
        auto info = connection->getInfo();
        
        // Use framer to extract complete messages
        auto messages = framer->unframe(data);
        
        for (const auto& messageData : messages) {
            std::string message(messageData.begin(), messageData.end());
            std::cout << "Received from " << info.remoteAddress << ":" << info.remotePort << ": " << message << std::endl;
            
            // Handle quit command
            if (message == "quit") {
                connection->send("Goodbye!\r\n");
                connection->close();
                return;
            }
            
            // Echo the message back
            std::string response = "Echo: " + message + "\r\n";
            connection->send(response);
        }
    });
    
    server.setOnError([](std::shared_ptr<tcp::TcpConnection> connection, tcp::ErrorCode error, const std::string& message) {
        auto info = connection->getInfo();
        std::cerr << "Connection error for " << info.remoteAddress << ":" << info.remotePort 
                  << " (Error " << static_cast<int>(error) << "): " << message << std::endl;
    });
    
    // Start server
    std::cout << "Starting echo server on localhost:7777..." << std::endl;
    if (!server.start("127.0.0.1", 7777)) {
        std::cerr << "Failed to start server" << std::endl;
        tcp::Library::cleanup();
        return 1;
    }
    
    std::cout << "Echo server started successfully!" << std::endl;
    std::cout << "Listening on " << server.getLocalAddress() << ":" << server.getLocalPort() << std::endl;
    std::cout << "Connect with: telnet localhost 7777" << std::endl;
    
    // Keep server running
    std::cout << "Press Enter to stop server..." << std::endl;
    std::cin.get();
    
    // Stop server
    std::cout << "Stopping server..." << std::endl;
    server.stop();
    
    // Print final statistics
    auto stats = server.getStatistics();
    std::cout << "Final Server Statistics:" << std::endl;
    std::cout << "  Total connections: " << stats.totalConnections << std::endl;
    std::cout << "  Total bytes sent: " << stats.totalBytesSent << std::endl;
    std::cout << "  Total bytes received: " << stats.totalBytesReceived << std::endl;
    
    // Cleanup
    tcp::Library::cleanup();
    
    std::cout << "Echo server example completed." << std::endl;
    return 0;
}
