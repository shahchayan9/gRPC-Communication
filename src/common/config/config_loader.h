#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace mini2 {

// Process information structure
struct ProcessInfo {
    std::string process_id;
    std::string host;
    int port;
    std::vector<std::string> connections;
    std::string data_subset;
};

// Configuration loader class
class ConfigLoader {
public:
    static ConfigLoader& getInstance();
    
    // Load configuration from file
    bool loadFromFile(const std::string& filename);
    
    // Get process information by ID
    ProcessInfo getProcessInfo(const std::string& process_id) const;
    
    // Get all process information
    std::unordered_map<std::string, ProcessInfo> getAllProcessInfo() const;
    
    // Get overlay connections
    std::vector<std::string> getOverlayConnections() const;
    
private:
    ConfigLoader() = default;
    
    std::unordered_map<std::string, ProcessInfo> processes_;
    std::vector<std::string> overlay_;
};

} // namespace mini2