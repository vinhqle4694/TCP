#include "tcp_utils.h"
#include "tcp_socket.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>

namespace tcp {

// LengthPrefixedFramer implementation
LengthPrefixedFramer::LengthPrefixedFramer(LengthType lengthType, bool bigEndian)
    : lengthType_(lengthType), bigEndian_(bigEndian), expectedLength_(0), lengthReceived_(false) {
}

std::vector<uint8_t> LengthPrefixedFramer::frame(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> framedData;
    framedData.reserve(data.size() + getLengthSize());
    
    // Add length prefix
    writeLength(framedData, data.size());
    
    // Add data
    framedData.insert(framedData.end(), data.begin(), data.end());
    
    return framedData;
}

std::vector<std::vector<uint8_t>> LengthPrefixedFramer::unframe(const std::vector<uint8_t>& data) {
    std::vector<std::vector<uint8_t>> messages;
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    
    while (buffer_.size() >= getLengthSize()) {
        if (!lengthReceived_) {
            if (buffer_.size() >= getLengthSize()) {
                expectedLength_ = readLength(buffer_, 0);
                lengthReceived_ = true;
                buffer_.erase(buffer_.begin(), buffer_.begin() + getLengthSize());
            } else {
                break;
            }
        }
        
        if (lengthReceived_ && buffer_.size() >= expectedLength_) {
            std::vector<uint8_t> message(buffer_.begin(), buffer_.begin() + expectedLength_);
            messages.push_back(message);
            buffer_.erase(buffer_.begin(), buffer_.begin() + expectedLength_);
            lengthReceived_ = false;
            expectedLength_ = 0;
        } else {
            break;
        }
    }
    
    return messages;
}

bool LengthPrefixedFramer::isComplete(const std::vector<uint8_t>& data) {
    if (data.size() < getLengthSize()) {
        return false;
    }
    
    size_t expectedLength = readLength(data, 0);
    return data.size() >= getLengthSize() + expectedLength;
}

void LengthPrefixedFramer::reset() {
    buffer_.clear();
    expectedLength_ = 0;
    lengthReceived_ = false;
}

size_t LengthPrefixedFramer::getLengthSize() const {
    switch (lengthType_) {
        case LengthType::UInt8: return 1;
        case LengthType::UInt16: return 2;
        case LengthType::UInt32: return 4;
        case LengthType::UInt64: return 8;
        default: return 4;
    }
}

void LengthPrefixedFramer::writeLength(std::vector<uint8_t>& data, size_t length) const {
    switch (lengthType_) {
        case LengthType::UInt8: {
            data.push_back(static_cast<uint8_t>(length));
            break;
        }
        case LengthType::UInt16: {
            uint16_t len = static_cast<uint16_t>(length);
            if (bigEndian_) {
                data.push_back((len >> 8) & 0xFF);
                data.push_back(len & 0xFF);
            } else {
                data.push_back(len & 0xFF);
                data.push_back((len >> 8) & 0xFF);
            }
            break;
        }
        case LengthType::UInt32: {
            uint32_t len = static_cast<uint32_t>(length);
            if (bigEndian_) {
                data.push_back((len >> 24) & 0xFF);
                data.push_back((len >> 16) & 0xFF);
                data.push_back((len >> 8) & 0xFF);
                data.push_back(len & 0xFF);
            } else {
                data.push_back(len & 0xFF);
                data.push_back((len >> 8) & 0xFF);
                data.push_back((len >> 16) & 0xFF);
                data.push_back((len >> 24) & 0xFF);
            }
            break;
        }
        case LengthType::UInt64: {
            uint64_t len = static_cast<uint64_t>(length);
            if (bigEndian_) {
                for (int i = 7; i >= 0; i--) {
                    data.push_back((len >> (i * 8)) & 0xFF);
                }
            } else {
                for (int i = 0; i < 8; i++) {
                    data.push_back((len >> (i * 8)) & 0xFF);
                }
            }
            break;
        }
    }
}

size_t LengthPrefixedFramer::readLength(const std::vector<uint8_t>& data, size_t offset) const {
    switch (lengthType_) {
        case LengthType::UInt8: {
            return data[offset];
        }
        case LengthType::UInt16: {
            if (bigEndian_) {
                return (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
            } else {
                return data[offset] | (static_cast<uint16_t>(data[offset + 1]) << 8);
            }
        }
        case LengthType::UInt32: {
            if (bigEndian_) {
                return (static_cast<uint32_t>(data[offset]) << 24) |
                       (static_cast<uint32_t>(data[offset + 1]) << 16) |
                       (static_cast<uint32_t>(data[offset + 2]) << 8) |
                       data[offset + 3];
            } else {
                return data[offset] |
                       (static_cast<uint32_t>(data[offset + 1]) << 8) |
                       (static_cast<uint32_t>(data[offset + 2]) << 16) |
                       (static_cast<uint32_t>(data[offset + 3]) << 24);
            }
        }
        case LengthType::UInt64: {
            uint64_t result = 0;
            if (bigEndian_) {
                for (int i = 0; i < 8; i++) {
                    result |= static_cast<uint64_t>(data[offset + i]) << ((7 - i) * 8);
                }
            } else {
                for (int i = 0; i < 8; i++) {
                    result |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
                }
            }
            return result;
        }
        default: return 0;
    }
}

// DelimiterFramer implementation
DelimiterFramer::DelimiterFramer(const std::vector<uint8_t>& delimiter, bool includeDelimiter)
    : delimiter_(delimiter), includeDelimiter_(includeDelimiter) {
}

DelimiterFramer::DelimiterFramer(const std::string& delimiter, bool includeDelimiter)
    : delimiter_(delimiter.begin(), delimiter.end()), includeDelimiter_(includeDelimiter) {
}

std::vector<uint8_t> DelimiterFramer::frame(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> framedData = data;
    framedData.insert(framedData.end(), delimiter_.begin(), delimiter_.end());
    return framedData;
}

std::vector<std::vector<uint8_t>> DelimiterFramer::unframe(const std::vector<uint8_t>& data) {
    std::vector<std::vector<uint8_t>> messages;
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    
    size_t pos = 0;
    while ((pos = findDelimiter(buffer_, pos)) != std::string::npos) {
        std::vector<uint8_t> message(buffer_.begin(), buffer_.begin() + pos);
        if (includeDelimiter_) {
            message.insert(message.end(), delimiter_.begin(), delimiter_.end());
        }
        messages.push_back(message);
        buffer_.erase(buffer_.begin(), buffer_.begin() + pos + delimiter_.size());
        pos = 0;
    }
    
    return messages;
}

bool DelimiterFramer::isComplete(const std::vector<uint8_t>& data) {
    return findDelimiter(data) != std::string::npos;
}

void DelimiterFramer::reset() {
    buffer_.clear();
}

size_t DelimiterFramer::findDelimiter(const std::vector<uint8_t>& data, size_t startPos) const {
    if (delimiter_.empty() || data.size() < delimiter_.size()) {
        return std::string::npos;
    }
    
    for (size_t i = startPos; i <= data.size() - delimiter_.size(); i++) {
        if (std::equal(delimiter_.begin(), delimiter_.end(), data.begin() + i)) {
            return i;
        }
    }
    
    return std::string::npos;
}

// ConnectionPool implementation
ConnectionPool::ConnectionPool(size_t maxConnections) : maxConnections_(maxConnections) {
}

ConnectionPool::~ConnectionPool() {
    clear();
}

std::shared_ptr<TcpConnection> ConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Try to get an idle connection
    if (!idleConnections_.empty()) {
        auto connection = idleConnections_.back();
        idleConnections_.pop_back();
        activeConnections_.push_back(connection);
        return connection;
    }
    
    // Create new connection if under limit
    if (activeConnections_.size() < maxConnections_) {
        if (connectionFactory_) {
            auto connection = connectionFactory_();
            if (connection) {
                activeConnections_.push_back(connection);
                return connection;
            }
        }
    }
    
    // Wait for a connection to become available
    condition_.wait(lock, [this] { return !idleConnections_.empty() || activeConnections_.size() < maxConnections_; });
    
    if (!idleConnections_.empty()) {
        auto connection = idleConnections_.back();
        idleConnections_.pop_back();
        activeConnections_.push_back(connection);
        return connection;
    }
    
    return nullptr;
}

void ConnectionPool::release(std::shared_ptr<TcpConnection> connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Remove from active connections
    auto it = std::find(activeConnections_.begin(), activeConnections_.end(), connection);
    if (it != activeConnections_.end()) {
        activeConnections_.erase(it);
        
        // Add to idle connections if connection is valid
        // Note: We can't check isConnected() here due to forward declaration
        // The connection factory should ensure connections are valid
        if (connection) {
            idleConnections_.push_back(connection);
        }
        
        condition_.notify_one();
    }
}

void ConnectionPool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Clear connections (can't call close() due to forward declaration)
    // The connections will be properly cleaned up when their destructors are called
    activeConnections_.clear();
    idleConnections_.clear();
    condition_.notify_all();
}

void ConnectionPool::setMaxConnections(size_t maxConnections) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxConnections_ = maxConnections;
}

size_t ConnectionPool::getActiveConnections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeConnections_.size();
}

size_t ConnectionPool::getIdleConnections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return idleConnections_.size();
}

void ConnectionPool::setConnectionFactory(std::function<std::shared_ptr<TcpConnection>()> factory) {
    connectionFactory_ = factory;
}

// RateLimiter implementation
RateLimiter::RateLimiter(size_t bytesPerSecond, size_t bucketSize)
    : bytesPerSecond_(bytesPerSecond), bucketSize_(bucketSize > 0 ? bucketSize : bytesPerSecond),
      availableBytes_(bucketSize_), lastRefill_(std::chrono::steady_clock::now()) {
}

bool RateLimiter::allowBytes(size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    refillBucket();
    
    if (availableBytes_ >= bytes) {
        availableBytes_ -= bytes;
        return true;
    }
    
    return false;
}

std::chrono::milliseconds RateLimiter::getDelay(size_t bytes) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (availableBytes_ >= bytes) {
        return std::chrono::milliseconds{0};
    }
    
    size_t deficit = bytes - availableBytes_;
    double secondsToWait = static_cast<double>(deficit) / bytesPerSecond_;
    return std::chrono::milliseconds{static_cast<long long>(secondsToWait * 1000)};
}

void RateLimiter::waitForBytes(size_t bytes) {
    while (!allowBytes(bytes)) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
}

void RateLimiter::setRate(size_t bytesPerSecond) {
    std::lock_guard<std::mutex> lock(mutex_);
    bytesPerSecond_ = bytesPerSecond;
}

void RateLimiter::setBucketSize(size_t bucketSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    bucketSize_ = bucketSize;
}

size_t RateLimiter::getAvailableBytes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return availableBytes_;
}

double RateLimiter::getUtilization() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return 1.0 - (static_cast<double>(availableBytes_) / bucketSize_);
}

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    availableBytes_ = bucketSize_;
    lastRefill_ = std::chrono::steady_clock::now();
}

void RateLimiter::refillBucket() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRefill_);
    
    if (elapsed.count() > 0) {
        size_t tokensToAdd = static_cast<size_t>((elapsed.count() / 1000.0) * bytesPerSecond_);
        availableBytes_ = std::min(bucketSize_, availableBytes_ + tokensToAdd);
        lastRefill_ = now;
    }
}

// BufferManager implementation
std::vector<uint8_t> BufferManager::allocateBuffer(size_t size) {
    return std::vector<uint8_t>(size);
}

void BufferManager::deallocateBuffer(std::vector<uint8_t>& buffer) {
    buffer.clear();
    buffer.shrink_to_fit();
}

std::vector<uint8_t> BufferManager::resizeBuffer(std::vector<uint8_t>& buffer, size_t newSize) {
    buffer.resize(newSize);
    return buffer;
}

void BufferManager::copyBuffer(const std::vector<uint8_t>& source, std::vector<uint8_t>& destination) {
    destination = source;
}

std::vector<uint8_t> BufferManager::concatenateBuffers(const std::vector<std::vector<uint8_t>>& buffers) {
    std::vector<uint8_t> result;
    size_t totalSize = 0;
    
    for (const auto& buffer : buffers) {
        totalSize += buffer.size();
    }
    
    result.reserve(totalSize);
    
    for (const auto& buffer : buffers) {
        result.insert(result.end(), buffer.begin(), buffer.end());
    }
    
    return result;
}

std::vector<std::vector<uint8_t>> BufferManager::splitBuffer(const std::vector<uint8_t>& buffer, size_t chunkSize) {
    std::vector<std::vector<uint8_t>> chunks;
    
    for (size_t i = 0; i < buffer.size(); i += chunkSize) {
        size_t end = std::min(i + chunkSize, buffer.size());
        chunks.emplace_back(buffer.begin() + i, buffer.begin() + end);
    }
    
    return chunks;
}

// CircularBuffer implementation
BufferManager::CircularBuffer::CircularBuffer(size_t capacity)
    : buffer_(capacity), capacity_(capacity), size_(0), head_(0), tail_(0) {
}

size_t BufferManager::CircularBuffer::write(const void* data, size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t availableSpace = capacity_ - size_;
    size_t writeLength = std::min(length, availableSpace);
    
    const uint8_t* sourceData = static_cast<const uint8_t*>(data);
    
    for (size_t i = 0; i < writeLength; i++) {
        buffer_[tail_] = sourceData[i];
        tail_ = (tail_ + 1) % capacity_;
        size_++;
    }
    
    return writeLength;
}

size_t BufferManager::CircularBuffer::read(void* data, size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t readLength = std::min(length, size_);
    uint8_t* destData = static_cast<uint8_t*>(data);
    
    for (size_t i = 0; i < readLength; i++) {
        destData[i] = buffer_[head_];
        head_ = (head_ + 1) % capacity_;
        size_--;
    }
    
    return readLength;
}

size_t BufferManager::CircularBuffer::peek(void* data, size_t length) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t peekLength = std::min(length, size_);
    uint8_t* destData = static_cast<uint8_t*>(data);
    
    size_t tempHead = head_;
    for (size_t i = 0; i < peekLength; i++) {
        destData[i] = buffer_[tempHead];
        tempHead = (tempHead + 1) % capacity_;
    }
    
    return peekLength;
}

void BufferManager::CircularBuffer::skip(size_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t skipLength = std::min(length, size_);
    head_ = (head_ + skipLength) % capacity_;
    size_ -= skipLength;
}

void BufferManager::CircularBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_ = 0;
    head_ = 0;
    tail_ = 0;
}

// Logger implementation
Logger::Level Logger::currentLevel_ = Logger::Level::Info;
std::function<void(Logger::Level, const std::string&)> Logger::output_ = nullptr;
std::mutex Logger::mutex_;

void Logger::setLevel(Level level) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
}

Logger::Level Logger::getLevel() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentLevel_;
}

void Logger::setOutput(std::function<void(Level, const std::string&)> output) {
    std::lock_guard<std::mutex> lock(mutex_);
    output_ = output;
}

void Logger::debug(const std::string& message) {
    log(Level::Debug, message);
}

void Logger::info(const std::string& message) {
    log(Level::Info, message);
}

void Logger::warning(const std::string& message) {
    log(Level::Warning, message);
}

void Logger::error(const std::string& message) {
    log(Level::Error, message);
}

void Logger::critical(const std::string& message) {
    log(Level::Critical, message);
}

void Logger::log(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (static_cast<int>(level) >= static_cast<int>(currentLevel_)) {
        std::string formattedMessage = formatMessage(level, message);
        
        if (output_) {
            output_(level, formattedMessage);
        } else {
            std::cout << formattedMessage << std::endl;
        }
    }
}

std::string Logger::formatMessage(Level level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << " [" << levelToString(level) << "] " << message;
    
    return ss.str();
}

std::string Logger::levelToString(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warning: return "WARNING";
        case Level::Error: return "ERROR";
        case Level::Critical: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

} // namespace tcp
