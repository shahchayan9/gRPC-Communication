#include "config_loader.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

// Using nlohmann/json for JSON parsing (header-only library)
#include "json.hpp"

namespace mini2 {

// Include the JSON library inline for simplicity
// This is the "json.hpp" header from nlohmann/json
// You can download it from: https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
// and place it in src/common/config/

// Create the json.hpp file (a simplified version for this project)
// Note: In a real project, you should download the full version
#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nlohmann {
    class json {
    public:
        using value_t = std::string;
        using object_t = std::unordered_map<std::string, json>;
        using array_t = std::vector<json>;
        
        json() = default;
        json(std::nullptr_t) {}
        json(const std::string& s) : string_value_(s) {}
        json(const char* s) : string_value_(s) {}
        json(int i) : int_value_(i), is_int_(true) {}
        json(const object_t& obj) : object_value_(obj), is_object_(true) {}
        json(const array_t& arr) : array_value_(arr), is_array_(true) {}
        
        bool is_object() const { return is_object_; }
        bool is_array() const { return is_array_; }
        bool is_string() const { return !is_object_ && !is_array_ && !is_int_; }
        bool is_number() const { return is_int_; }
        
        const object_t& get_ref() const {
            if (!is_object_) throw std::runtime_error("not an object");
            return object_value_;
        }
        
        const array_t& get_array() const {
            if (!is_array_) throw std::runtime_error("not an array");
            return array_value_;
        }
        
        std::string get() const {
            if (is_object_ || is_array_) throw std::runtime_error("not a value");
            return string_value_;
        }
        
        int get_int() const {
            if (!is_int_) throw std::runtime_error("not an int");
            return int_value_;
        }
        
        json& operator[](const std::string& key) {
            is_object_ = true;
            return object_value_[key];
        }
        
        const json& operator[](const std::string& key) const {
            if (!is_object_) throw std::runtime_error("not an object");
            return object_value_.at(key);
        }
        
        static json parse(const std::string& s) {
            // A very simplified JSON parser for demonstration purposes
            // In a real project, use the full nlohmann/json library
            if (s.empty()) return json();
            
            if (s[0] == '{') {
                // Parse object
                json result;
                result.is_object_ = true;
                // This parser is just a stub - in practice use the real library
                return result;
            } else if (s[0] == '[') {
                // Parse array
                json result;
                result.is_array_ = true;
                // This parser is just a stub - in practice use the real library
                return result;
            } else if (s[0] == '"') {
                // Parse string
                return json(s.substr(1, s.size() - 2));
            } else if (std::isdigit(s[0])) {
                // Parse number
                return json(std::stoi(s));
            }
            
            return json();
        }
        
    private:
        std::string string_value_;
        int int_value_ = 0;
        bool is_int_ = false;
        bool is_object_ = false;
        bool is_array_ = false;
        object_t object_value_;
        array_t array_value_;
    };
}

// Continue with the ConfigLoader implementation
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
        // Read the file into a string
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        
        // Parse JSON
        auto config = nlohmann::json::parse(content);
        
        // Clear existing data
        processes_.clear();
        overlay_.clear();
        
        // Load processes
        const auto& processes = config["processes"];
        for (const auto& item : processes.get_ref()) {
            const std::string& process_id = item.first;
            const auto& process_data = item.second;
            
            ProcessInfo info;
            info.process_id = process_id;
            info.host = process_data["host"].get();
            info.port = process_data["port"].get_int();
            
            // Load connections
            const auto& connections = process_data["connections"];
            for (const auto& conn : connections.get_array()) {
                info.connections.push_back(conn.get());
            }
            
            info.data_subset = process_data["data_subset"].get();
            
            processes_[process_id] = info;
        }
        
        // Load overlay
        const auto& overlay = config["overlay"];
        for (const auto& conn : overlay.get_array()) {
            overlay_.push_back(conn.get());
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