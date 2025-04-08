#include "data_structures.h"
#include <fstream>
#include <mutex>
#include <sstream>

namespace mini2 {

// Static members initialization
std::unordered_map<std::string, std::unique_ptr<DataStore>> DataStore::instances_;
std::mutex DataStore::instances_mutex_;

DataStore& DataStore::getInstance(const std::string& store_name) {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    
    auto it = instances_.find(store_name);
    if (it != instances_.end()) {
        return *it->second;
    }
    
    // Create new instance
    auto store = std::unique_ptr<DataStore>(new DataStore(store_name));
    auto& result = *store;
    instances_[store_name] = std::move(store);
    return result;
}

DataStore::DataStore(const std::string& name) : name_(name) {
    // Initialize with empty data
}

void DataStore::store(const DataEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[entry.key] = entry;
}

bool DataStore::get(const std::string& key, DataEntry& entry) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = data_.find(key);
    if (it != data_.end()) {
        entry = it->second;
        return true;
    }
    return false;
}

bool DataStore::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = data_.find(key);
    if (it != data_.end()) {
        data_.erase(it);
        return true;
    }
    return false;
}

QueryResult DataStore::processQuery(const Query& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Simple query processing logic
    // In a real implementation, you would parse the query string and execute it
    
    if (query.query_string == "get_all") {
        // Return all data
        std::vector<DataEntry> results;
        for (const auto& pair : data_) {
            results.push_back(pair.second);
        }
        return QueryResult::createSuccess(query.id, results);
    }
    else if (query.query_string == "get_by_key") {
        // Return data for specific keys
        std::vector<DataEntry> results;
        for (const auto& key : query.parameters) {
            auto it = data_.find(key);
            if (it != data_.end()) {
                results.push_back(it->second);
            }
        }
        return QueryResult::createSuccess(query.id, results);
    }
    else if (query.query_string == "get_by_prefix") {
        // Return data with keys starting with prefix
        if (query.parameters.empty()) {
            return QueryResult::createFailure(query.id, "No prefix provided");
        }
        
        const std::string& prefix = query.parameters[0];
        std::vector<DataEntry> results;
        
        for (const auto& pair : data_) {
            if (pair.first.compare(0, prefix.length(), prefix) == 0) {
                results.push_back(pair.second);
            }
        }
        
        return QueryResult::createSuccess(query.id, results);
    }
    
    return QueryResult::createFailure(query.id, "Unknown query: " + query.query_string);
}

bool DataStore::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open data file: " << filename << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
    
    std::string line;
    while (std::getline(file, line)) {
        // Simple format: key,type,value
        std::istringstream iss(line);
        std::string key, type, value;
        
        if (std::getline(iss, key, ',') && 
            std::getline(iss, type, ',') && 
            std::getline(iss, value)) {
            
            DataEntry entry;
            entry.key = key;
            entry.timestamp = DataEntry::getCurrentTimestamp();
            
            if (type == "int") {
                entry.value = std::stoi(value);
            }
            else if (type == "double") {
                entry.value = std::stod(value);
            }
            else if (type == "bool") {
                entry.value = (value == "true" || value == "1");
            }
            else if (type == "string") {
                entry.value = value;
            }
            else {
                continue; // Skip unknown type
            }
            
            data_[key] = entry;
        }
    }
    
    return true;
}

} // namespace mini2