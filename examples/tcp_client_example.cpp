#include "../tcp.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "TCP Client Example" << std::endl;
    std::cout << "==================" << std::endl;
    
    // Initialize library
    if (!tcp::Library::initialize()) {
        std::cerr << "Failed to initialize TCP library" << std::endl;
        return 1;
    }
    
    // Create client
    tcp::TcpClient client;
    
    // Set up callbacks
    client.setOnConnected([]() {
        std::cout << "Connected to server!" << std::endl;
    });
    
    client.setOnDisconnected([]() {
        std::cout << "Disconnected from server!" << std::endl;
    });
    
    client.setOnDataReceived([](const std::vector<uint8_t>& data) {
        std::string message(data.begin(), data.end());
        std::cout << "Received: " << message << std::endl;
    });
    
    client.setOnError([](tcp::ErrorCode error, const std::string& message) {
        std::cerr << "Error (" << static_cast<int>(error) << "): " << message << std::endl;
    });
    
    // Connect to server
    std::cout << "Connecting to localhost:8080..." << std::endl;
    if (!client.connect("127.0.0.1", 8080)) {
        std::cerr << "Failed to connect to server" << std::endl;
        tcp::Library::cleanup();
        return 1;
    }
    
    // Send some messages
    std::cout << "Sending messages..." << std::endl;
    client.send("Hello, Server!");
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay between messages
    
    client.send("How are you?");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    client.send("This is a test message.");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Wait for responses
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Send async message
    client.sendAsync("Async message", [](bool success) {
        if (success) {
            std::cout << "Async message sent successfully" << std::endl;
        } else {
            std::cerr << "Failed to send async message" << std::endl;
        }
    });
    
    // Wait for async message to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Keep client running for a while
    std::cout << "Press Enter to disconnect..." << std::endl;
    std::cin.get();
    
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
    
    std::cout << "Client example completed." << std::endl;
    return 0;
}
