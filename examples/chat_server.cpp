#include "../tcp.h"
#include <iostream>
#include <string>
#include <set>
#include <sstream>

class ChatServer {
private:
    tcp::TcpServer server_;
    std::set<std::shared_ptr<tcp::TcpConnection>> clients_;
    std::mutex clientsMutex_;
    
    void broadcastMessage(const std::string& message, std::shared_ptr<tcp::TcpConnection> sender = nullptr) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        
        std::string formattedMessage = message + "\r\n";
        for (auto& client : clients_) {
            if (client && client != sender && client->isConnected()) {
                client->send(formattedMessage);
            }
        }
    }
    
    void removeClient(std::shared_ptr<tcp::TcpConnection> client) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.erase(client);
    }
    
public:
    ChatServer() {
        setupCallbacks();
    }
    
    void setupCallbacks() {
        server_.setOnConnected([this](std::shared_ptr<tcp::TcpConnection> connection) {
            auto info = connection->getInfo();
            std::cout << "Client connected from " << info.remoteAddress << ":" << info.remotePort << std::endl;
            
            {
                std::lock_guard<std::mutex> lock(clientsMutex_);
                clients_.insert(connection);
            }
            
            // Send welcome message
            connection->send("Welcome to Chat Server! Type '/help' for commands.\r\n");
            
            // Notify other clients
            std::stringstream ss;
            ss << "User " << info.remoteAddress << ":" << info.remotePort << " joined the chat";
            broadcastMessage(ss.str(), connection);
        });
        
        server_.setOnDisconnected([this](std::shared_ptr<tcp::TcpConnection> connection) {
            auto info = connection->getInfo();
            std::cout << "Client disconnected: " << info.remoteAddress << ":" << info.remotePort << std::endl;
            
            // Notify other clients
            std::stringstream ss;
            ss << "User " << info.remoteAddress << ":" << info.remotePort << " left the chat";
            broadcastMessage(ss.str(), connection);
            
            removeClient(connection);
        });
        
        server_.setOnDataReceived([this](std::shared_ptr<tcp::TcpConnection> connection, const std::vector<uint8_t>& data) {
            std::string message(data.begin(), data.end());
            
            // Remove trailing whitespace
            message.erase(message.find_last_not_of(" \t\n\r\f\v") + 1);
            
            if (message.empty()) {
                return;
            }
            
            auto info = connection->getInfo();
            std::cout << "Message from " << info.remoteAddress << ":" << info.remotePort << ": " << message << std::endl;
            
            // Handle commands
            if (message[0] == '/') {
                handleCommand(connection, message);
            } else {
                // Broadcast message to all clients
                std::stringstream ss;
                ss << "[" << info.remoteAddress << ":" << info.remotePort << "] " << message;
                broadcastMessage(ss.str(), connection);
            }
        });
        
        server_.setOnError([](std::shared_ptr<tcp::TcpConnection> connection, tcp::ErrorCode error, const std::string& message) {
            auto info = connection->getInfo();
            std::cerr << "Connection error for " << info.remoteAddress << ":" << info.remotePort 
                      << " (Error " << static_cast<int>(error) << "): " << message << std::endl;
        });
    }
    
    void handleCommand(std::shared_ptr<tcp::TcpConnection> connection, const std::string& command) {
        if (command == "/help") {
            connection->send("Available commands:\r\n");
            connection->send("  /help - Show this help message\r\n");
            connection->send("  /users - List connected users\r\n");
            connection->send("  /stats - Show server statistics\r\n");
            connection->send("  /quit - Disconnect from server\r\n");
        } else if (command == "/users") {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            connection->send("Connected users:\r\n");
            for (const auto& client : clients_) {
                if (client && client->isConnected()) {
                    auto info = client->getInfo();
                    std::string userInfo = "  " + info.remoteAddress + ":" + std::to_string(info.remotePort) + "\r\n";
                    connection->send(userInfo);
                }
            }
        } else if (command == "/stats") {
            auto stats = server_.getStatistics();
            std::stringstream ss;
            ss << "Server Statistics:\r\n";
            ss << "  Active connections: " << stats.activeConnections << "\r\n";
            ss << "  Total connections: " << stats.totalConnections << "\r\n";
            ss << "  Total bytes sent: " << stats.totalBytesSent << "\r\n";
            ss << "  Total bytes received: " << stats.totalBytesReceived << "\r\n";
            connection->send(ss.str());
        } else if (command == "/quit") {
            connection->send("Goodbye!\r\n");
            connection->close();
        } else {
            connection->send("Unknown command. Type '/help' for available commands.\r\n");
        }
    }
    
    bool start(const std::string& address, uint16_t port) {
        return server_.start(address, port);
    }
    
    void stop() {
        server_.stop();
    }
    
    bool isRunning() const {
        return server_.isRunning();
    }
    
    tcp::TcpServer::Statistics getStatistics() const {
        return server_.getStatistics();
    }
};

int main() {
    std::cout << "Chat Server Example" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Initialize library
    if (!tcp::Library::initialize()) {
        std::cerr << "Failed to initialize TCP library" << std::endl;
        return 1;
    }
    
    // Create chat server
    ChatServer chatServer;
    
    // Start server
    std::cout << "Starting chat server on localhost:9999..." << std::endl;
    if (!chatServer.start("127.0.0.1", 9999)) {
        std::cerr << "Failed to start chat server" << std::endl;
        tcp::Library::cleanup();
        return 1;
    }
    
    std::cout << "Chat server started successfully!" << std::endl;
    std::cout << "Connect with: telnet localhost 9999" << std::endl;
    
    // Keep server running
    std::cout << "Press Enter to stop server..." << std::endl;
    std::cin.get();
    
    // Stop server
    std::cout << "Stopping chat server..." << std::endl;
    chatServer.stop();
    
    // Print final statistics
    auto stats = chatServer.getStatistics();
    std::cout << "Final Server Statistics:" << std::endl;
    std::cout << "  Total connections: " << stats.totalConnections << std::endl;
    std::cout << "  Total bytes sent: " << stats.totalBytesSent << std::endl;
    std::cout << "  Total bytes received: " << stats.totalBytesReceived << std::endl;
    
    // Cleanup
    tcp::Library::cleanup();
    
    std::cout << "Chat server example completed." << std::endl;
    return 0;
}
