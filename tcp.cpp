#include "tcp.h"
#include <mutex>
#include <sstream>

namespace tcp {

// Library static members
bool Library::initialized_ = false;
std::mutex Library::mutex_;

bool Library::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return true;
    }
    
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return false;
    }
#endif
    
    initialized_ = true;
    return true;
}

void Library::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    initialized_ = false;
}

bool Library::isInitialized() {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

std::string Library::getBuildInfo() {
    std::ostringstream oss;
    oss << "TCP Library v" << Version::getString() << "\n";
    oss << "Build date: " << __DATE__ << " " << __TIME__ << "\n";
    oss << "Compiler: " << 
#ifdef __GNUC__
        "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__
#elif defined(_MSC_VER)
        "MSVC " << _MSC_VER
#elif defined(__clang__)
        "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__
#else
        "Unknown"
#endif
        << "\n";
    
    oss << "Platform: " << 
#ifdef _WIN32
        "Windows"
#elif defined(__linux__)
        "Linux"
#elif defined(__APPLE__)
        "macOS"
#else
        "Unknown"
#endif
        << "\n";
    
    return oss.str();
}

std::vector<std::string> Library::getSupportedFeatures() {
    std::vector<std::string> features;
    
    features.push_back("TCP Client");
    features.push_back("TCP Server");
    features.push_back("Async I/O");
    features.push_back("Threading");
    features.push_back("Connection Management");
    features.push_back("Message Framing");
    features.push_back("Rate Limiting");
    features.push_back("Connection Pooling");
    features.push_back("Logging");
    features.push_back("Statistics");
    
#ifdef TCP_SSL_SUPPORT
    features.push_back("SSL/TLS");
#endif
    
    return features;
}

// Global namespace functions
namespace global {

static std::function<void(Logger::Level, const std::string&)> globalLogOutput = nullptr;
static Logger::Level globalLogLevel = Logger::Level::Info;
static std::string lastErrorMessage;
static SocketOptions defaultSocketOptions;

void setLogOutput(std::function<void(Logger::Level, const std::string&)> output) {
    globalLogOutput = output;
    Logger::setOutput(output);
}

void setLogLevel(Logger::Level level) {
    globalLogLevel = level;
    Logger::setLevel(level);
}

std::string getLastError() {
    return lastErrorMessage;
}

void setDefaultSocketOptions(const SocketOptions& options) {
    defaultSocketOptions = options;
}

SocketOptions getDefaultSocketOptions() {
    return defaultSocketOptions;
}

} // namespace global

} // namespace tcp
