#include "data_structures.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <iomanip>

namespace mini2 {

// Static members initialization
std::unordered_map<std::string, std::unique_ptr<DataStore>> DataStore::instances_;
std::mutex DataStore::instances_mutex_;

// CrashData implementation
std::string CrashData::toString() const {
    std::ostringstream oss;
    oss << "Date: " << crash_date << ", Time: " << crash_time
        << ", Borough: " << borough << ", ZIP: " << zip_code
        << ", Location: " << location
        << ", Street: " << on_street_name
        << ", Cross: " << cross_street_name
        << ", Off: " << off_street_name
        << ", Injured: " << persons_injured
        << ", Killed: " << persons_killed
        << ", Pedestrians: " << pedestrians;
    return oss.str();
}

CrashData CrashData::fromCSVRow(const std::vector<std::string>& row) {
    CrashData data;
    if (row.size() >= 13) {
        data.crash_date = row[0];
        data.crash_time = row[1];
        data.borough = row[2];
        data.zip_code = row[3];
        data.latitude = row[4];
        data.longitude = row[5];
        data.location = row[6];
        data.on_street_name = row[7];
        data.cross_street_name = row[8];
        data.off_street_name = row[9];
        
        // Convert numeric fields, handling empty strings
        try {
            data.persons_injured = row[10].empty() ? 0 : std::stoi(row[10]);
        } catch (...) {
            data.persons_injured = 0;
        }
        
        try {
            data.persons_killed = row[11].empty() ? 0 : std::stoi(row[11]);
        } catch (...) {
            data.persons_killed = 0;
        }
        
        try {
            data.pedestrians = row[12].empty() ? 0 : std::stoi(row[12]);
        } catch (...) {
            data.pedestrians = 0;
        }
    }
    return data;
}

// DataStore implementation
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
    
    // Handle different query types
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
    else if (query.query_string == "get_by_borough") {
        // Return data for specific borough
        if (query.parameters.empty()) {
            return QueryResult::createFailure(query.id, "No borough specified");
        }
        
        return getByBorough(query.parameters[0]);
    }
    else if (query.query_string == "get_by_street") {
        // Return data for specific street
        if (query.parameters.empty()) {
            return QueryResult::createFailure(query.id, "No street specified");
        }
        
        return getByStreet(query.parameters[0]);
    }
    else if (query.query_string == "get_by_date_range") {
        // Return data for date range
        if (query.parameters.size() < 2) {
            return QueryResult::createFailure(query.id, "Date range requires start and end dates");
        }
        
        return getByDateRange(query.parameters[0], query.parameters[1]);
    }
    else if (query.query_string == "get_crashes_with_injuries") {
        // Return crashes with injuries
        int min_injuries = 1;
        if (!query.parameters.empty()) {
            try {
                min_injuries = std::stoi(query.parameters[0]);
            } catch (...) {
                // Use default
            }
        }
        
        return getCrashesWithInjuries(min_injuries);
    }
    else if (query.query_string == "get_crashes_with_fatalities") {
        // Return crashes with fatalities
        int min_fatalities = 1;
        if (!query.parameters.empty()) {
            try {
                min_fatalities = std::stoi(query.parameters[0]);
            } catch (...) {
                // Use default
            }
        }
        
        return getCrashesWithFatalities(min_fatalities);
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

bool DataStore::loadCrashDataFromCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open crash data file: " << filename << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Read header line
    std::string header;
    std::getline(file, header);
    
    // Process each line
    std::string line;
    int count = 0;
    
    while (std::getline(file, line)) {
        std::vector<std::string> row;
        std::string field;
        std::istringstream iss(line);
        
        // Parse CSV row
        while (std::getline(iss, field, ',')) {
            // Remove any quotes
            if (!field.empty() && field.front() == '"' && field.back() == '"') {
                field = field.substr(1, field.size() - 2);
            }
            row.push_back(field);
        }
        
        // Ensure we have enough fields
        while (row.size() < 13) {
            row.push_back("");
        }
        
        // Create crash data entry
        CrashData crash = CrashData::fromCSVRow(row);
        
        // Generate a unique key for this crash
        std::string key = "crash_" + std::to_string(count);
        
        // Store in data store
        data_[key] = DataEntry::createCrashData(key, crash);
        count++;
    }
    
    std::cout << "Loaded " << count << " crash records from " << filename << std::endl;
    return true;
}

QueryResult DataStore::getByBorough(const std::string& borough) {
    std::vector<DataEntry> results;
    std::string borough_upper = borough;
    std::transform(borough_upper.begin(), borough_upper.end(), borough_upper.begin(), ::toupper);
    
    for (const auto& pair : data_) {
        if (std::holds_alternative<CrashData>(pair.second.value)) {
            const CrashData& crash = std::get<CrashData>(pair.second.value);
            std::string crash_borough = crash.borough;
            std::transform(crash_borough.begin(), crash_borough.end(), crash_borough.begin(), ::toupper);
            
            if (crash_borough == borough_upper) {
                results.push_back(pair.second);
            }
        }
    }
    
    return QueryResult::createSuccess("borough_query", results, 
                                     "Found " + std::to_string(results.size()) + 
                                     " crashes in " + borough);
}

QueryResult DataStore::getByStreet(const std::string& street) {
    std::vector<DataEntry> results;
    std::string street_upper = street;
    std::transform(street_upper.begin(), street_upper.end(), street_upper.begin(), ::toupper);
    
    for (const auto& pair : data_) {
        if (std::holds_alternative<CrashData>(pair.second.value)) {
            const CrashData& crash = std::get<CrashData>(pair.second.value);
            
            // Check if street appears in any of the street fields
            std::string on_street = crash.on_street_name;
            std::string cross_street = crash.cross_street_name;
            std::string off_street = crash.off_street_name;
            
            std::transform(on_street.begin(), on_street.end(), on_street.begin(), ::toupper);
            std::transform(cross_street.begin(), cross_street.end(), cross_street.begin(), ::toupper);
            std::transform(off_street.begin(), off_street.end(), off_street.begin(), ::toupper);
            
            if (on_street.find(street_upper) != std::string::npos ||
                cross_street.find(street_upper) != std::string::npos ||
                off_street.find(street_upper) != std::string::npos) {
                results.push_back(pair.second);
            }
        }
    }
    
    return QueryResult::createSuccess("street_query", results, 
                                     "Found " + std::to_string(results.size()) + 
                                     " crashes on street containing '" + street + "'");
}

// Helper to convert MM/DD/YYYY to a comparable format
int dateToComparable(const std::string& date_str) {
    std::regex date_regex(R"((\d{1,2})/(\d{1,2})/(\d{4}))");
    std::smatch matches;
    
    if (std::regex_match(date_str, matches, date_regex)) {
        int month = std::stoi(matches[1]);
        int day = std::stoi(matches[2]);
        int year = std::stoi(matches[3]);
        
        return year * 10000 + month * 100 + day;
    }
    
    return 0;  // Invalid date
}

QueryResult DataStore::getByDateRange(const std::string& start_date, const std::string& end_date) {
    std::vector<DataEntry> results;
    
    int start = dateToComparable(start_date);
    int end = dateToComparable(end_date);
    
    if (start == 0 || end == 0) {
        return QueryResult::createFailure("date_range_query", "Invalid date format. Use MM/DD/YYYY");
    }
    
    for (const auto& pair : data_) {
        if (std::holds_alternative<CrashData>(pair.second.value)) {
            const CrashData& crash = std::get<CrashData>(pair.second.value);
            int crash_date = dateToComparable(crash.crash_date);
            
            if (crash_date >= start && crash_date <= end) {
                results.push_back(pair.second);
            }
        }
    }
    
    return QueryResult::createSuccess("date_range_query", results, 
                                     "Found " + std::to_string(results.size()) + 
                                     " crashes between " + start_date + " and " + end_date);
}

QueryResult DataStore::getCrashesWithInjuries(int min_injuries) {
    std::vector<DataEntry> results;
    
    for (const auto& pair : data_) {
        if (std::holds_alternative<CrashData>(pair.second.value)) {
            const CrashData& crash = std::get<CrashData>(pair.second.value);
            
            if (crash.persons_injured >= min_injuries) {
                results.push_back(pair.second);
            }
        }
    }
    
    return QueryResult::createSuccess("injuries_query", results, 
                                     "Found " + std::to_string(results.size()) + 
                                     " crashes with at least " + std::to_string(min_injuries) + " injuries");
}

QueryResult DataStore::getCrashesWithFatalities(int min_fatalities) {
    std::vector<DataEntry> results;
    
    for (const auto& pair : data_) {
        if (std::holds_alternative<CrashData>(pair.second.value)) {
            const CrashData& crash = std::get<CrashData>(pair.second.value);
            
            if (crash.persons_killed >= min_fatalities) {
                results.push_back(pair.second);
            }
        }
    }
    
    return QueryResult::createSuccess("fatalities_query", results, 
                                     "Found " + std::to_string(results.size()) + 
                                     " crashes with at least " + std::to_string(min_fatalities) + " fatalities");
}

} // namespace mini2
