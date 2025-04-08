#include "config_loader.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

// Use the real nlohmann/json library
#include "config/json.hpp"

namespace mini2 {

ConfigLoader& ConfigLoader::getInstance() {
    static ConfigLoader instance;
    return instance;
}

bool ConfigLoader::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return false;
    }
    
    try {
        // Read the file and parse JSON
        nlohmann::json config;
        file >> config;
        
        // Clear existing data
        processes_.clear();
        overlay_.clear();
        
        // Load processes
        const auto& processes = config["processes"];
        for (auto it = processes.begin(); it != processes.end(); ++it) {
            const std::string& process_id = it.key();
            const auto& process_data = it.value();
            
            ProcessInfo info;
            info.process_id = process_id;
            info.host = process_data["host"].get<std::string>();
            info.port = process_data["port"].get<int>();
            
            // Load connections
            if (process_data.contains("connections")) {
                for (const auto& conn : process_data["connections"]) {
                    info.connections.push_back(conn.get<std::string>());
                }
            }
            
            info.data_subset = process_data["data_subset"].get<std::string>();
            
            processes_[process_id] = info;
        }
        
        // Load overlay
        const auto& overlay = config["overlay"];
        for (const auto& conn : overlay) {
            overlay_.push_back(conn.get<std::string>());
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;
        return false;
    }
}

ProcessInfo ConfigLoader::getProcessInfo(const std::string& process_id) const {
    auto it = processes_.find(process_id);
    if (it != processes_.end()) {
        return it->second;
    }
    throw std::runtime_error("Process ID not found in configuration: " + process_id);
}

std::unordered_map<std::string, ProcessInfo> ConfigLoader::getAllProcessInfo() const {
    return processes_;
}

std::vector<std::string> ConfigLoader::getOverlayConnections() const {
    return overlay_;
}

} // namespace mini2
