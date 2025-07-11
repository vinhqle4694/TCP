#include "../tcp.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

class ChatClient {
private:
    tcp::TcpClient client_;
    std::atomic<bool> running_{true};
    std::thread inputThread_;
    
    void startInputThread() {
        inputThread_ = std::thread([this]() {
            std::string input;
            while (running_ && client_.isConnected()) {
                std::cout << "You: ";
                std::getline(std::cin, input);
                
                if (input.empty()) {
                    continue;
                }
                
                if (input == "/quit") {
                    client_.send("/quit\r\n");
                    running_ = false;
                    break;
                }
                
                client_.send(input + "\r\n");
            }
        });
    }
    
public:
    ChatClient() {
        setupCallbacks();
    }
    
    ~ChatClient() {
        disconnect();
    }
    
    void setupCallbacks() {
        client_.setOnConnected([this]() {
            std::cout << "Connected to chat server!" << std::endl;
            std::cout << "Type messages to send (/quit to exit):" << std::endl;
            startInputThread();
        });
        
        client_.setOnDisconnected([this]() {
            std::cout << "Disconnected from chat server!" << std::endl;
            running_ = false;
        });
        
        client_.setOnDataReceived([](const std::vector<uint8_t>& data) {
            std::string message(data.begin(), data.end());
            
            // Remove trailing whitespace
            message.erase(message.find_last_not_of(" \t\n\r\f\v") + 1);
            
            if (!message.empty()) {
                std::cout << "\r" << message << std::endl;
                std::cout << "You: " << std::flush;
            }
        });
        
        client_.setOnError([](tcp::ErrorCode error, const std::string& message) {
            std::cerr << "Error (" << static_cast<int>(error) << "): " << message << std::endl;
        });
    }
    
    bool connect(const std::string& address, uint16_t port) {
        return client_.connect(address, port);
    }
    
    void disconnect() {
        running_ = false;
        client_.disconnect();
        
        if (inputThread_.joinable()) {
            inputThread_.join();
        }
    }
    
    void waitForDisconnection() {
        while (running_ && client_.isConnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    tcp::TcpClient::Statistics getStatistics() const {
        return client_.getStatistics();
    }
};

int main() {
    std::cout << "Chat Client Example" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Initialize library
    if (!tcp::Library::initialize()) {
        std::cerr << "Failed to initialize TCP library" << std::endl;
        return 1;
    }
    
    // Create chat client
    ChatClient chatClient;
    
    // Connect to server
    std::cout << "Connecting to localhost:9999..." << std::endl;
    if (!chatClient.connect("127.0.0.1", 9999)) {
        std::cerr << "Failed to connect to chat server" << std::endl;
        tcp::Library::cleanup();
        return 1;
    }
    
    // Wait for disconnection
    chatClient.waitForDisconnection();
    
    // Print statistics
    auto stats = chatClient.getStatistics();
    std::cout << "Connection Statistics:" << std::endl;
    std::cout << "  Total connections: " << stats.totalConnections << std::endl;
    std::cout << "  Bytes sent: " << stats.bytesSent << std::endl;
    std::cout << "  Bytes received: " << stats.bytesReceived << std::endl;
    
    // Cleanup
    tcp::Library::cleanup();
    
    std::cout << "Chat client example completed." << std::endl;
    return 0;
}
