#include "../tcp.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main() {
    std::cout << "Echo Client Example" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Initialize library
    if (!tcp::Library::initialize()) {
        std::cerr << "Failed to initialize TCP library" << std::endl;
        return 1;
    }
    
    // Create client
    tcp::TcpClient client;
    
    // Set up message framer for line-based protocol
    auto framer = std::make_shared<tcp::DelimiterFramer>("\r\n", false);
    
    // Set up callbacks
    client.setOnConnected([]() {
        std::cout << "Connected to echo server!" << std::endl;
        std::cout << "Type messages to send (empty line to quit):" << std::endl;
    });
    
    client.setOnDisconnected([]() {
        std::cout << "Disconnected from echo server!" << std::endl;
    });
    
    client.setOnDataReceived([framer](const std::vector<uint8_t>& data) {
        // Use framer to extract complete messages
        auto messages = framer->unframe(data);
        
        for (const auto& messageData : messages) {
            std::string message(messageData.begin(), messageData.end());
            std::cout << "Server: " << message << std::endl;
        }
    });
    
    client.setOnError([](tcp::ErrorCode error, const std::string& message) {
        std::cerr << "Error (" << static_cast<int>(error) << "): " << message << std::endl;
    });
    
    // Connect to server
    std::cout << "Connecting to localhost:7777..." << std::endl;
    if (!client.connect("127.0.0.1", 7777)) {
        std::cerr << "Failed to connect to server" << std::endl;
        tcp::Library::cleanup();
        return 1;
    }
    
    // Wait for welcome message
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Interactive message loop
    std::string input;
    while (client.isConnected()) {
        std::cout << "You: ";
        std::getline(std::cin, input);
        
        if (input.empty()) {
            break;
        }
        
        // Send message with line ending
        client.send(input + "\r\n");
        
        // Brief pause to see response
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Send quit command
    if (client.isConnected()) {
        client.send("quit\r\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Disconnect
    client.disconnect();
    
    // Print statistics
    auto stats = client.getStatistics();
    std::cout << "Connection Statistics:" << std::endl;
    std::cout << "  Total connections: " << stats.totalConnections << std::endl;
    std::cout << "  Bytes sent: " << stats.bytesSent << std::endl;
    std::cout << "  Bytes received: " << stats.bytesReceived << std::endl;
    
    // Cleanup
    tcp::Library::cleanup();
    
    std::cout << "Echo client example completed." << std::endl;
    return 0;
}
