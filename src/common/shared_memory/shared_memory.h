#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

namespace mini2 {

// Forward declarations
class SharedMemorySegment;
class SharedCache;

// Class to manage shared memory objects
class SharedMemoryManager {
public:
    static SharedMemoryManager& getInstance();
    
    // Create or open a shared memory segment with the given name and size
    std::shared_ptr<SharedMemorySegment> createSegment(const std::string& name, size_t size);
    
    // Remove a shared memory segment
    bool removeSegment(const std::string& name);
    
private:
    SharedMemoryManager() = default;
    ~SharedMemoryManager();
    
    std::unordered_map<std::string, std::shared_ptr<SharedMemorySegment>> segments_;
    std::mutex mutex_;
};

// Shared memory segment class
class SharedMemorySegment {
public:
    SharedMemorySegment(const std::string& name, size_t size, bool create);
    ~SharedMemorySegment();
    
    // Get raw pointer to the shared memory
    void* getPtr() const { return ptr_; }
    
    // Get size of the shared memory segment
    size_t getSize() const { return size_; }
    
    // Get name of the shared memory segment
    const std::string& getName() const { return name_; }
    
    // Write data to shared memory at offset
    bool write(const void* data, size_t size, size_t offset = 0);
    
    // Read data from shared memory at offset
    bool read(void* data, size_t size, size_t offset = 0) const;
    
    // Lock the shared memory for exclusive access
    void lock();
    
    // Unlock the shared memory
    void unlock();
    
private:
    std::string name_;
    size_t size_;
    int fd_;
    void* ptr_;
    struct SharedHeader {
        std::mutex mutex;
    };
    SharedHeader* header_;
};

// Simple struct for cache entries
struct CacheEntry {
    std::vector<uint8_t> data;
    int64_t timestamp;
    int32_t ttl;  // Time to live in milliseconds
};

// Shared memory-based cache
class SharedCache {
public:
    // Create or connect to a named shared cache
    static std::shared_ptr<SharedCache> create(const std::string& name, size_t max_size);
    
    // Get an entry from the cache
    bool get(const std::string& key, std::vector<uint8_t>& data) const;
    
    // Put an entry into the cache
    bool put(const std::string& key, const std::vector<uint8_t>& data, int32_t ttl_ms = 0);
    
    // Remove an entry from the cache
    bool remove(const std::string& key);
    
    // Clear all entries
    void clear();
    
private:
    SharedCache(std::shared_ptr<SharedMemorySegment> segment, size_t max_size);
    
    // Serialize/deserialize the cache to/from shared memory
    void serializeToMemory();
    void deserializeFromMemory();
    
    std::shared_ptr<SharedMemorySegment> segment_;
    std::unordered_map<std::string, CacheEntry> cache_map_;
    size_t max_size_;
    mutable std::mutex local_mutex_;
};

} // namespace mini2