#include "shared_memory.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <cerrno>

namespace mini2 {

SharedMemoryManager& SharedMemoryManager::getInstance() {
    static SharedMemoryManager instance;
    return instance;
}

std::shared_ptr<SharedMemorySegment> SharedMemoryManager::createSegment(
    const std::string& name, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if segment already exists
    auto it = segments_.find(name);
    if (it != segments_.end()) {
        return it->second;
    }
    
    // Create new segment
    try {
        auto segment = std::make_shared<SharedMemorySegment>(name, size, true);
        segments_[name] = segment;
        return segment;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create shared memory segment: " << e.what() << std::endl;
        return nullptr;
    }
}

bool SharedMemoryManager::removeSegment(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = segments_.find(name);
    if (it != segments_.end()) {
        segments_.erase(it);
        return true;
    }
    return false;
}

SharedMemoryManager::~SharedMemoryManager() {
    // Clean up all segments
    segments_.clear();
}

SharedMemorySegment::SharedMemorySegment(const std::string& name, size_t size, bool create)
    : name_("/mini2_" + name), size_(size), fd_(-1), ptr_(nullptr), header_(nullptr) {
    
    // Adjust size to include header
    size_t total_size = size + sizeof(SharedHeader);
    
    // Open or create shared memory object
    int flags = O_RDWR;
    if (create) {
        flags |= O_CREAT;
    }
    
    fd_ = shm_open(name_.c_str(), flags, 0666);
    if (fd_ == -1) {
        throw std::runtime_error("Failed to open shared memory: " + std::string(strerror(errno)));
    }
    
    // Set size
    if (create && ftruncate(fd_, total_size) == -1) {
        close(fd_);
        throw std::runtime_error("Failed to set shared memory size: " + std::string(strerror(errno)));
    }
    
    // Map to memory
    ptr_ = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr_ == MAP_FAILED) {
        close(fd_);
        throw std::runtime_error("Failed to map shared memory: " + std::string(strerror(errno)));
    }
    
    // Set up header (mutex) if creating
    header_ = static_cast<SharedHeader*>(ptr_);
    if (create) {
        new (&header_->mutex) std::mutex();
    }
}

SharedMemorySegment::~SharedMemorySegment() {
    if (ptr_ != nullptr && ptr_ != MAP_FAILED) {
        // Destroy mutex
        header_->mutex.~mutex();
        
        // Unmap memory
        munmap(ptr_, size_ + sizeof(SharedHeader));
    }
    
    if (fd_ != -1) {
        close(fd_);
    }
}

bool SharedMemorySegment::write(const void* data, size_t size, size_t offset) {
    if (offset + size > size_) {
        return false;
    }
    
    // Calculate pointer to data area (after header)
    void* target = static_cast<char*>(ptr_) + sizeof(SharedHeader) + offset;
    
    // Copy data
    memcpy(target, data, size);
    return true;
}

bool SharedMemorySegment::read(void* data, size_t size, size_t offset) const {
    if (offset + size > size_) {
        return false;
    }
    
    // Calculate pointer to data area (after header)
    const void* source = static_cast<const char*>(ptr_) + sizeof(SharedHeader) + offset;
    
    // Copy data
    memcpy(data, source, size);
    return true;
}

void SharedMemorySegment::lock() {
    header_->mutex.lock();
}

void SharedMemorySegment::unlock() {
    header_->mutex.unlock();
}

// SharedCache implementation
std::shared_ptr<SharedCache> SharedCache::create(const std::string& name, size_t max_size) {
    auto segment = SharedMemoryManager::getInstance().createSegment(
        "cache_" + name, max_size);
    if (!segment) {
        return nullptr;
    }
    
    return std::shared_ptr<SharedCache>(new SharedCache(segment, max_size));
}

SharedCache::SharedCache(std::shared_ptr<SharedMemorySegment> segment, size_t max_size)
    : segment_(segment), max_size_(max_size) {
    deserializeFromMemory();
}

bool SharedCache::get(const std::string& key, std::vector<uint8_t>& data) const {
    std::lock_guard<std::mutex> lock(local_mutex_);
    segment_->lock();
    
    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        const auto& entry = it->second;
        
        // Check if entry has expired
        if (entry.ttl > 0) {
            auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            if (now > entry.timestamp + entry.ttl) {
                segment_->unlock();
                return false;
            }
        }
        
        data = entry.data;
        segment_->unlock();
        return true;
    }
    
    segment_->unlock();
    return false;
}

bool SharedCache::put(const std::string& key, const std::vector<uint8_t>& data, int32_t ttl_ms) {
    std::lock_guard<std::mutex> lock(local_mutex_);
    segment_->lock();
    
    // Add or update entry
    CacheEntry entry;
    entry.data = data;
    entry.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    entry.ttl = ttl_ms;
    
    cache_map_[key] = entry;
    
    // Serialize to shared memory
    serializeToMemory();
    
    segment_->unlock();
    return true;
}

bool SharedCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(local_mutex_);
    segment_->lock();
    
    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        cache_map_.erase(it);
        serializeToMemory();
        segment_->unlock();
        return true;
    }
    
    segment_->unlock();
    return false;
}

void SharedCache::clear() {
    std::lock_guard<std::mutex> lock(local_mutex_);
    segment_->lock();
    
    cache_map_.clear();
    serializeToMemory();
    
    segment_->unlock();
}

void SharedCache::serializeToMemory() {
    // Simple serialization format:
    // [number of entries (4 bytes)][key length (4 bytes)][key data][value length (4 bytes)][value data][timestamp (8 bytes)][ttl (4 bytes)]...
    
    std::vector<uint8_t> buffer;
    uint32_t num_entries = static_cast<uint32_t>(cache_map_.size());
    
    // Reserve space for number of entries
    buffer.resize(sizeof(num_entries));
    memcpy(buffer.data(), &num_entries, sizeof(num_entries));
    
    // Add each entry
    for (const auto& pair : cache_map_) {
        const std::string& key = pair.first;
        const CacheEntry& entry = pair.second;
        
        // Key length and data
        uint32_t key_length = static_cast<uint32_t>(key.size());
        size_t pos = buffer.size();
        buffer.resize(pos + sizeof(key_length));
        memcpy(buffer.data() + pos, &key_length, sizeof(key_length));
        
        pos = buffer.size();
        buffer.resize(pos + key_length);
        memcpy(buffer.data() + pos, key.data(), key_length);
        
        // Value length and data
        uint32_t value_length = static_cast<uint32_t>(entry.data.size());
        pos = buffer.size();
        buffer.resize(pos + sizeof(value_length));
        memcpy(buffer.data() + pos, &value_length, sizeof(value_length));
        
        pos = buffer.size();
        buffer.resize(pos + value_length);
        memcpy(buffer.data() + pos, entry.data.data(), value_length);
        
        // Timestamp and TTL
        pos = buffer.size();
        buffer.resize(pos + sizeof(entry.timestamp) + sizeof(entry.ttl));
        memcpy(buffer.data() + pos, &entry.timestamp, sizeof(entry.timestamp));
        memcpy(buffer.data() + pos + sizeof(entry.timestamp), &entry.ttl, sizeof(entry.ttl));
    }
    
    // Write to shared memory
    if (buffer.size() <= max_size_) {
        segment_->write(buffer.data(), buffer.size(), 0);
    } else {
        std::cerr << "Cache data too large for shared memory segment" << std::endl;
    }
}

void SharedCache::deserializeFromMemory() {
    // Read from shared memory
    std::vector<uint8_t> buffer(max_size_);
    segment_->read(buffer.data(), max_size_, 0);
    
    // Clear existing map
    cache_map_.clear();
    
    // Check if there's valid data
    if (buffer.size() < sizeof(uint32_t)) {
        return;
    }
    
    // Read number of entries
    uint32_t num_entries;
    memcpy(&num_entries, buffer.data(), sizeof(num_entries));
    
    // Parse entries
    size_t pos = sizeof(num_entries);
    for (uint32_t i = 0; i < num_entries; ++i) {
        if (pos + sizeof(uint32_t) > buffer.size()) break;
        
        // Read key length and data
        uint32_t key_length;
        memcpy(&key_length, buffer.data() + pos, sizeof(key_length));
        pos += sizeof(key_length);
        
        if (pos + key_length > buffer.size()) break;
        std::string key(reinterpret_cast<char*>(buffer.data() + pos), key_length);
        pos += key_length;
        
        // Read value length and data
        if (pos + sizeof(uint32_t) > buffer.size()) break;
        uint32_t value_length;
        memcpy(&value_length, buffer.data() + pos, sizeof(value_length));
        pos += sizeof(value_length);
        
        if (pos + value_length > buffer.size()) break;
        std::vector<uint8_t> value_data(buffer.data() + pos, buffer.data() + pos + value_length);
        pos += value_length;
        
        // Read timestamp and TTL
        if (pos + sizeof(int64_t) + sizeof(int32_t) > buffer.size()) break;
        int64_t timestamp;
        int32_t ttl;
        memcpy(&timestamp, buffer.data() + pos, sizeof(timestamp));
        memcpy(&ttl, buffer.data() + pos + sizeof(timestamp), sizeof(ttl));
        pos += sizeof(timestamp) + sizeof(ttl);
        
        // Create entry
        CacheEntry entry;
        entry.data = std::move(value_data);
        entry.timestamp = timestamp;
        entry.ttl = ttl;
        
        cache_map_[key] = std::move(entry);
    }
}

} // namespace mini2