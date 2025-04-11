#include "shared_memory.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <cerrno>

namespace mini2 {

// Singleton access for shared memory manager
SharedMemoryManager& SharedMemoryManager::getInstance() {
    static SharedMemoryManager instance;
    return instance;
}

std::shared_ptr<SharedMemorySegment> SharedMemoryManager::createSegment(const std::string& name, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (segments_.count(name)) {
        return segments_[name];
    }

    try {
        auto segment = std::make_shared<SharedMemorySegment>(name, size, true);
        segments_[name] = segment;
        return segment;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create shared memory segment: " << e.what() << std::endl;

        auto segment = std::make_shared<SharedMemorySegment>();
        segment->initializeWithRegularMemory(name, size);
        segments_[name] = segment;
        return segment;
    }
}

bool SharedMemoryManager::removeSegment(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return segments_.erase(name) > 0;
}

SharedMemoryManager::~SharedMemoryManager() {
    segments_.clear();
}

// ========== SharedMemorySegment Implementation ==========

SharedMemorySegment::SharedMemorySegment()
    : name_(""), size_(0), fd_(-1), ptr_(nullptr), header_(nullptr), using_regular_memory_(false) {}

void SharedMemorySegment::initializeWithRegularMemory(const std::string& name, size_t size) {
    name_ = name;
    size_ = size;
    using_regular_memory_ = true;

    size_t total_size = size + sizeof(SharedHeader);
    ptr_ = malloc(total_size);
    if (!ptr_) throw std::runtime_error("Failed to allocate regular memory");

    header_ = static_cast<SharedHeader*>(ptr_);
    new (&header_->mutex) std::mutex();
}

SharedMemorySegment::SharedMemorySegment(const std::string& name, size_t size, bool create)
    : name_("/mini2_" + name), size_(size), fd_(-1), ptr_(nullptr), header_(nullptr), using_regular_memory_(false) {
    
    size_t total_size = size + sizeof(SharedHeader);
    int flags = O_RDWR | (create ? O_CREAT : 0);

    fd_ = shm_open(name_.c_str(), flags, 0666);
    if (fd_ == -1)
        throw std::runtime_error("Failed to open shared memory: " + std::string(strerror(errno)));

    if (create && ftruncate(fd_, total_size) == -1) {
        total_size = 1024 * 1024;
        size_ = total_size - sizeof(SharedHeader);
        if (ftruncate(fd_, total_size) == -1) {
            close(fd_);
            throw std::runtime_error("Failed to set shared memory size: " + std::string(strerror(errno)));
        }
    }

    ptr_ = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr_ == MAP_FAILED) {
        close(fd_);
        throw std::runtime_error("Failed to map shared memory: " + std::string(strerror(errno)));
    }

    header_ = static_cast<SharedHeader*>(ptr_);
    if (create) {
        new (&header_->mutex) std::mutex();
    }
}

SharedMemorySegment::~SharedMemorySegment() {
    if (ptr_) {
        header_->mutex.~mutex();

        if (using_regular_memory_) {
            free(ptr_);
        } else {
            munmap(ptr_, size_ + sizeof(SharedHeader));
        }
    }

    if (fd_ != -1) {
        close(fd_);
    }
}

bool SharedMemorySegment::write(const void* data, size_t size, size_t offset) {
    if (offset + size > size_) return false;
    void* target = static_cast<char*>(ptr_) + sizeof(SharedHeader) + offset;
    memcpy(target, data, size);
    return true;
}

bool SharedMemorySegment::read(void* data, size_t size, size_t offset) const {
    if (offset + size > size_) return false;
    const void* source = static_cast<const char*>(ptr_) + sizeof(SharedHeader) + offset;
    memcpy(data, source, size);
    return true;
}

void SharedMemorySegment::lock() { header_->mutex.lock(); }
void SharedMemorySegment::unlock() { header_->mutex.unlock(); }

// ========== SharedCache Implementation ==========

std::shared_ptr<SharedCache> SharedCache::create(const std::string& name, size_t max_size) {
    auto segment = SharedMemoryManager::getInstance().createSegment("cache_" + name, max_size);
    return segment ? std::make_shared<SharedCache>(segment, max_size) : nullptr;
}

SharedCache::SharedCache(std::shared_ptr<SharedMemorySegment> segment, size_t max_size)
    : segment_(std::move(segment)), max_size_(max_size) {
    deserializeFromMemory();
}

bool SharedCache::get(const std::string& key, std::vector<uint8_t>& data) const {
    std::lock_guard<std::mutex> lock(local_mutex_);
    segment_->lock();

    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        const auto& entry = it->second;

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

    CacheEntry entry{data, std::chrono::steady_clock::now().time_since_epoch().count(), ttl_ms};
    cache_map_[key] = std::move(entry);

    serializeToMemory();
    segment_->unlock();
    return true;
}

bool SharedCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(local_mutex_);
    segment_->lock();

    if (cache_map_.erase(key)) {
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
    std::vector<uint8_t> buffer;
    uint32_t num_entries = static_cast<uint32_t>(cache_map_.size());

    buffer.resize(sizeof(num_entries));
    memcpy(buffer.data(), &num_entries, sizeof(num_entries));

    for (const auto& [key, entry] : cache_map_) {
        uint32_t key_len = key.size();
        uint32_t val_len = entry.data.size();

        auto write_chunk = [&](const void* src, size_t len) {
            size_t pos = buffer.size();
            buffer.resize(pos + len);
            memcpy(buffer.data() + pos, src, len);
        };

        write_chunk(&key_len, sizeof(key_len));
        write_chunk(key.data(), key_len);
        write_chunk(&val_len, sizeof(val_len));
        write_chunk(entry.data.data(), val_len);
        write_chunk(&entry.timestamp, sizeof(entry.timestamp));
        write_chunk(&entry.ttl, sizeof(entry.ttl));
    }

    if (buffer.size() <= max_size_) {
        segment_->write(buffer.data(), buffer.size(), 0);
    } else {
        std::cerr << "Cache too large for shared memory segment" << std::endl;
        segment_->write(buffer.data(), max_size_, 0);
    }
}

void SharedCache::deserializeFromMemory() {
    std::vector<uint8_t> buffer(max_size_);
    segment_->read(buffer.data(), max_size_, 0);

    cache_map_.clear();
    if (buffer.size() < sizeof(uint32_t)) return;

    uint32_t num_entries;
    memcpy(&num_entries, buffer.data(), sizeof(num_entries));

    size_t pos = sizeof(num_entries);
    for (uint32_t i = 0; i < num_entries; ++i) {
        if (pos + sizeof(uint32_t) > buffer.size()) break;

        uint32_t key_len;
        memcpy(&key_len, buffer.data() + pos, sizeof(key_len));
        pos += sizeof(key_len);

        if (pos + key_len > buffer.size()) break;
        std::string key(reinterpret_cast<char*>(buffer.data() + pos), key_len);
        pos += key_len;

        if (pos + sizeof(uint32_t) > buffer.size()) break;
        uint32_t val_len;
        memcpy(&val_len, buffer.data() + pos, sizeof(val_len));
        pos += sizeof(val_len);

        if (pos + val_len > buffer.size()) break;
        std::vector<uint8_t> val(buffer.data() + pos, buffer.data() + pos + val_len);
        pos += val_len;

        if (pos + sizeof(int64_t) + sizeof(int32_t) > buffer.size()) break;
        int64_t timestamp;
        int32_t ttl;
        memcpy(&timestamp, buffer.data() + pos, sizeof(timestamp));
        memcpy(&ttl, buffer.data() + pos + sizeof(timestamp), sizeof(ttl));
        pos += sizeof(timestamp) + sizeof(ttl);

        cache_map_[key] = {std::move(val), timestamp, ttl};
    }
}

} // namespace mini2
