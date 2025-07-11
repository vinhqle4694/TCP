#include "../tcp.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "TCP Server Example" << std::endl;
    std::cout << "==================" << std::endl;
    
    // Initialize library
    if (!tcp::Library::initialize()) {
        std::cerr << "Failed to initialize TCP library" << std::endl;
        return 1;
    }
    
    // Create server
    tcp::TcpServer server;
    
    // Set up callbacks
    server.setOnConnected([](std::shared_ptr<tcp::TcpConnection> connection) {
        auto info = connection->getInfo();
        std::cout << "Client connected from " << info.remoteAddress << ":" << info.remotePort << std::endl;
    });
    
    server.setOnDisconnected([](std::shared_ptr<tcp::TcpConnection> connection) {
        auto info = connection->getInfo();
        std::cout << "Client disconnected: " << info.remoteAddress << ":" << info.remotePort << std::endl;
    });
    
    server.setOnDataReceived([](std::shared_ptr<tcp::TcpConnection> connection, const std::vector<uint8_t>& data) {
        std::string message(data.begin(), data.end());
        auto info = connection->getInfo();
        std::cout << "Received from " << info.remoteAddress << ":" << info.remotePort << ": " << message << std::endl;
        
        // Echo the message back
        std::string response = "Echo: " + message;
        connection->send(response);
    });
    
    server.setOnError([](std::shared_ptr<tcp::TcpConnection> connection, tcp::ErrorCode error, const std::string& message) {
        auto info = connection->getInfo();
        std::cerr << "Connection error for " << info.remoteAddress << ":" << info.remotePort 
                  << " (Error " << static_cast<int>(error) << "): " << message << std::endl;
    });
    
    // Start server
    std::cout << "Starting server on localhost:8080..." << std::endl;
    if (!server.start("127.0.0.1", 8080)) {
        std::cerr << "Failed to start server" << std::endl;
        tcp::Library::cleanup();
        return 1;
    }
    
    std::cout << "Server started successfully!" << std::endl;
    std::cout << "Listening on " << server.getLocalAddress() << ":" << server.getLocalPort() << std::endl;
    
    // Print server statistics periodically
    std::thread statsThread([&server]() {
        while (server.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
            auto stats = server.getStatistics();
            std::cout << "Server Statistics:" << std::endl;
            std::cout << "  Active connections: " << stats.activeConnections << std::endl;
            std::cout << "  Total connections: " << stats.totalConnections << std::endl;
            std::cout << "  Bytes sent: " << stats.totalBytesSent << std::endl;
            std::cout << "  Bytes received: " << stats.totalBytesReceived << std::endl;
            std::cout << "  Uptime: " << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - stats.startTime).count() << " seconds" << std::endl;
        }
    });
    
    // Keep server running
    std::cout << "Press Enter to stop server..." << std::endl;
    std::cin.get();
    
    // Stop server
    std::cout << "Stopping server..." << std::endl;
    server.stop();
    
    // Wait for statistics thread to finish
    if (statsThread.joinable()) {
        statsThread.join();
    }
    
    // Print final statistics
    auto stats = server.getStatistics();
    std::cout << "Final Server Statistics:" << std::endl;
    std::cout << "  Total connections: " << stats.totalConnections << std::endl;
    std::cout << "  Total bytes sent: " << stats.totalBytesSent << std::endl;
    std::cout << "  Total bytes received: " << stats.totalBytesReceived << std::endl;
    
    // Cleanup
    tcp::Library::cleanup();
    
    std::cout << "Server example completed." << std::endl;
    return 0;
}
