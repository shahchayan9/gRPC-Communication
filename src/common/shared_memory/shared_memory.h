#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

namespace mini2 {

// Forward declarations
class SharedMemorySegment;
class SharedCache;


class SharedMemoryManager {
public:
    static SharedMemoryManager& getInstance();

    // Create or reuse a shared memory segment
    std::shared_ptr<SharedMemorySegment> createSegment(const std::string& name, size_t size);

    // Remove a previously created segment
    bool removeSegment(const std::string& name);

private:
    SharedMemoryManager() = default;
    ~SharedMemoryManager();

    std::unordered_map<std::string, std::shared_ptr<SharedMemorySegment>> segments_;
    std::mutex mutex_;
};



class SharedMemorySegment {
public:
    SharedMemorySegment(); // fallback constructor
    SharedMemorySegment(const std::string& name, size_t size, bool create);
    ~SharedMemorySegment();

    // For non-shared memory fallback usage
    void initializeWithRegularMemory(const std::string& name, size_t size);

    // Accessors
    void* getPtr() const { return ptr_; }
    size_t getSize() const { return size_; }
    const std::string& getName() const { return name_; }

    // Read/write operations at given offset
    bool write(const void* data, size_t size, size_t offset = 0);
    bool read(void* data, size_t size, size_t offset = 0) const;

    // Lock/unlock for thread-safe access
    void lock();
    void unlock();

private:
    std::string name_;
    size_t size_;
    int fd_;
    void* ptr_;
    bool using_regular_memory_;

    struct SharedHeader {
        std::mutex mutex;
    };
    SharedHeader* header_;
};



struct CacheEntry {
    std::vector<uint8_t> data;
    int64_t timestamp; 
    int32_t ttl;       
};



class SharedCache {
public:
    // Create or attach to a shared cache segment
    static std::shared_ptr<SharedCache> create(const std::string& name, size_t max_size);

    // Retrieve a cached value by key
    bool get(const std::string& key, std::vector<uint8_t>& data) const;

    // Insert or update a cached value with optional TTL
    bool put(const std::string& key, const std::vector<uint8_t>& data, int32_t ttl_ms = 0);

    // Remove an entry from the cache
    bool remove(const std::string& key);

    // Clear all cache entries
    void clear();

    // Constructor must be public to allow make_shared
    SharedCache(std::shared_ptr<SharedMemorySegment> segment, size_t max_size);

private:
    // Handles serialization and deserialization of cache_map_
    void serializeToMemory();
    void deserializeFromMemory();

    std::shared_ptr<SharedMemorySegment> segment_;
    std::unordered_map<std::string, CacheEntry> cache_map_;
    size_t max_size_;
    mutable std::mutex local_mutex_;
};

} // namespace mini2
