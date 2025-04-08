#pragma once

#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <iostream>
#include <chrono>

namespace mini2 {

// Value types supported
using DataValue = std::variant<int, double, bool, std::string, std::vector<uint8_t>>;

// Data entry structure
struct DataEntry {
    std::string key;
    DataValue value;
    int64_t timestamp;
    
    // Helper methods for creating entries
    static DataEntry createInt(const std::string& key, int value) {
        return {key, value, getCurrentTimestamp()};
    }
    
    static DataEntry createDouble(const std::string& key, double value) {
        return {key, value, getCurrentTimestamp()};
    }
    
    static DataEntry createBool(const std::string& key, bool value) {
        return {key, value, getCurrentTimestamp()};
    }
    
    static DataEntry createString(const std::string& key, const std::string& value) {
        return {key, value, getCurrentTimestamp()};
    }
    
    static DataEntry createBinary(const std::string& key, const std::vector<uint8_t>& value) {
        return {key, value, getCurrentTimestamp()};
    }
    
    static int64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

// Query structure
struct Query {
    std::string id;
    std::string query_string;
    std::vector<std::string> parameters;
    
    // Create a query with a generated ID
    static Query create(const std::string& query_string, 
                       const std::vector<std::string>& parameters = {}) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        return {std::to_string(now), query_string, parameters};
    }
};

// Query result structure
struct QueryResult {
    std::string query_id;
    bool success;
    std::string message;
    std::vector<DataEntry> results;
    
    // Create a success result
    static QueryResult createSuccess(const std::string& query_id,
                                    const std::vector<DataEntry>& results,
                                    const std::string& message = "Success") {
        return {query_id, true, message, results};
    }
    
    // Create a failure result
    static QueryResult createFailure(const std::string& query_id,
                                    const std::string& error_message) {
        return {query_id, false, error_message, {}};
    }
};

// Data store for each process
class DataStore {
public:
    // Singleton access
    static DataStore& getInstance(const std::string& store_name = "default");
    
    // Store a data entry
    void store(const DataEntry& entry);
    
    // Get a data entry by key
    bool get(const std::string& key, DataEntry& entry) const;
    
    // Remove a data entry
    bool remove(const std::string& key);
    
    // Process a query
    QueryResult processQuery(const Query& query);
    
    // Load data from a file
    bool loadFromFile(const std::string& filename);
    
private:
    DataStore(const std::string& name);
    
    std::unordered_map<std::string, DataEntry> data_;
    std::string name_;
    mutable std::mutex mutex_;
    
    static std::unordered_map<std::string, std::unique_ptr<DataStore>> instances_;
    static std::mutex instances_mutex_;
};

} // namespace mini2