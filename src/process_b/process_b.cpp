#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <iomanip>

#include "config/config_loader.h"
#include "data/data_structures.h"
#include "grpc/data_service.h"
#include "shared_memory/shared_memory.h"
#include "timing/timing.h"

using namespace mini2;

// Process B implementation
class ProcessB {
public:
    ProcessB(const std::string& config_file, const std::string& data_file)
        : running_(false) {
        
        // Load configuration
        ConfigLoader::getInstance().loadFromFile(config_file);
        
        // Get process info
        process_info_ = ConfigLoader::getInstance().getProcessInfo("B");
        
        // Create gRPC server
        std::string server_address = process_info_.host + ":" + std::to_string(process_info_.port);
        server_ = std::make_unique<DataServiceServer>("B", server_address);
        
        // Load data
        data_store_ = &DataStore::getInstance("process_b");
        if (!data_file.empty()) {
            // Load crash data from CSV
            data_store_->loadCrashDataFromCSV(data_file);
        } else {
            // Load default data path if exists
            std::string default_path = "data/process_b/process1.csv";
            std::ifstream test_file(default_path);
            if (test_file.good()) {
                test_file.close();
                data_store_->loadCrashDataFromCSV(default_path);
            } else {
                // Load some demo data
                loadDemoData();
            }
        }
        
        // Create shared cache
        cache_ = SharedCache::create("process_b_cache", 1024 * 1024); // 1MB cache
    }
    
    ~ProcessB() {
        stop();
    }
    
    bool start() {
        if (running_) {
            return true; // Already running
        }
        
        // Set up handlers
        server_->setQueryHandler([this](const Query& query) {
            return handleQuery(query);
        });
        
        server_->setDataHandler([this](const std::string& source, 
                                      const std::string& destination,
                                      const std::vector<uint8_t>& data) {
            handleData(source, destination, data);
        });
        
        // Connect to downstream servers
        connectToDownstreamServers();
        
        // Start server
        if (!server_->start()) {
            return false;
        }
        
        running_ = true;
        return true;
    }
    
    void stop() {
        if (!running_) {
            return;
        }
        
        running_ = false;
        
        // Stop server
        if (server_) {
            server_->stop();
        }
        
        // Disconnect from downstream servers
        clients_.clear();
    }
    
private:
    bool running_;
    ProcessInfo process_info_;
    std::unique_ptr<DataServiceServer> server_;
    std::unordered_map<std::string, std::unique_ptr<DataServiceClient>> clients_;
    DataStore* data_store_;
    std::shared_ptr<SharedCache> cache_;
    
    void loadDemoData() {
        // Create demo crash data for BROOKLYN
        for (int i = 0; i < 10; ++i) {
            CrashData crash;
            crash.crash_date = "12/14/2021";
            crash.crash_time = "8:" + std::to_string(i) + "0";
            crash.borough = "BROOKLYN";
            crash.zip_code = "11211";
            crash.latitude = "40.7128";
            crash.longitude = "-73.9654";
            crash.location = "(40.7128, -73.9654)";
            crash.on_street_name = "BEDFORD AVENUE";
            crash.cross_street_name = "GRAND STREET";
            crash.off_street_name = "";
            crash.persons_injured = i % 3;
            crash.persons_killed = (i % 5 == 0) ? 1 : 0;
            crash.pedestrians = i % 2;
            
            // Generate a unique key for this crash
            std::string key = "processB_" + std::to_string(i);
            
            // Store in data store
            data_store_->store(DataEntry::createCrashData(key, crash));
        }
        
        std::cout << "Created 10 demo crash records for Process B" << std::endl;
    }
    
    void connectToDownstreamServers() {
        // Connect to each server in the connections list
        for (const auto& conn_id : process_info_.connections) {
            try {
                auto conn_info = ConfigLoader::getInstance().getProcessInfo(conn_id);
                std::string target = conn_info.host + ":" + std::to_string(conn_info.port);
                
                std::cout << "Connecting to " << conn_id << " at " << target << std::endl;
                
                auto client = std::make_unique<DataServiceClient>(target);
                clients_[conn_id] = std::move(client);
            } catch (const std::exception& e) {
                std::cerr << "Failed to connect to " << conn_id << ": " << e.what() << std::endl;
            }
        }
    }
    
    QueryResult handleQuery(const Query& query) {
        std::cout << "Process B received query: " << query.query_string;
        if (!query.parameters.empty()) {
            std::cout << " with parameters: ";
            for (size_t i = 0; i < query.parameters.size(); ++i) {
                std::cout << query.parameters[i];
                if (i < query.parameters.size() - 1) {
                    std::cout << ", ";
                }
            }
        }
        
        // Start timing for this query
        QueryTimer::getInstance().startTiming(query.id, "B");
        
        std::cout << std::endl;
        
        // Check cache first
        std::string cache_key = "query_" + query.query_string;
        for (const auto& param : query.parameters) {
            cache_key += "_" + param;
        }
        
        std::vector<uint8_t> cached_data;
        
        if (cache_->get(cache_key, cached_data)) {
            
            // Deserialize from cache
            std::string cache_str(cached_data.begin(), cached_data.end());
            QueryResult cached_result;
            
            // Record cache hit timing
            QueryTimer::getInstance().endTiming(query.id, "Cache_Access");
            QueryTimer::getInstance().endTiming(query.id, "Total_Processing");
            
            // Add timing data to cached result
            cached_result.timing_data = QueryTimer::getInstance().serializeTimingData(query.id);
            cached_result.query_id = query.id;
            cached_result.success = true;
            cached_result.message = "From cache";
            
            // Simple parsing of cached data
            std::istringstream iss(cache_str);
            std::string line;
            while (std::getline(iss, line)) {
                std::istringstream line_iss(line);
                std::string key, type_str, value_str;
                
                if (std::getline(line_iss, key, ',') && 
                    std::getline(line_iss, type_str, ',') && 
                    std::getline(line_iss, value_str)) {
                    
                    DataEntry entry;
                    entry.key = key;
                    entry.timestamp = DataEntry::getCurrentTimestamp();
                    
                    if (type_str == "int") {
                        entry.value = std::stoi(value_str);
                    }
                    else if (type_str == "double") {
                        entry.value = std::stod(value_str);
                    }
                    else if (type_str == "bool") {
                        entry.value = (value_str == "true" || value_str == "1");
                    }
                    else {
                        entry.value = value_str;
                    }
                    
                    cached_result.results.push_back(entry);
                }
            }
            
            std::cout << "Cache hit for query " << cache_key << std::endl;
            return cached_result;
        }
        
        // Process the query locally
        QueryResult local_result;

        // Start local processing timing
        QueryTimer::getInstance().startTiming(query.id, "Local_Processing");
        
        // Process specific queries for crash data
        if (query.query_string == "get_by_borough") {
            if (!query.parameters.empty() && query.parameters[0] == "BROOKLYN") {
                // This is our borough, process locally
                local_result = data_store_->getByBorough("BROOKLYN");
                // Record local processing time
                QueryTimer::getInstance().endTiming(query.id, "Local_Processing");
            } else {
                // Not our borough, return empty result
                local_result = QueryResult::createSuccess(query.id, {}, "No BROOKLYN data requested");
            }
        }
        else if (query.query_string == "get_by_street" || 
                query.query_string == "get_by_date_range" ||
                query.query_string == "get_crashes_with_injuries" ||
                query.query_string == "get_crashes_with_fatalities" ||
                query.query_string == "get_by_time") {
            // Process these special queries locally
            local_result = data_store_->processQuery(query);
            // Record local processing time
            QueryTimer::getInstance().endTiming(query.id, "Local_Processing");
        }
        else {
            // Standard queries like get_all, get_by_key, get_by_prefix
            local_result = data_store_->processQuery(query);
            // Record local processing time
            QueryTimer::getInstance().endTiming(query.id, "Local_Processing");
        }
        
        // Forward to other servers if needed (except for borough-specific queries)
        if (query.query_string != "get_by_borough" && 
            (query.query_string == "get_all" || shouldForwardQuery(query))) {
            
            // Start downstream query timing
            QueryTimer::getInstance().startTiming(query.id, "Downstream_Queries");

            std::vector<QueryResult> downstream_results;
            
            for (const auto& [conn_id, client] : clients_) {
                if (client->isConnected()) {
                    std::cout << "connected. ";
                    // Record timing for this specific downstream query
                    QueryTimer::getInstance().startTiming(query.id, "Query_To_" + conn_id);

                    auto downstream_result = client->queryData(query);
                    
                    QueryTimer::getInstance().endTiming(query.id, "Query_To_" + conn_id);
                    std::cout << "Got response. Success: " << (downstream_result.success ? "true" : "false")
                             << ", Results: " << downstream_result.results.size() << std::endl;
                    if (downstream_result.success) {
                        downstream_results.push_back(downstream_result);
                    }
                }
            }
            
            // Add downstream results to local results
            for (const auto& result : downstream_results) {
                local_result.results.insert(local_result.results.end(),
                                         result.results.begin(),
                                         result.results.end());
                 // Add downstream timing data
                if (!result.timing_data.empty()) {
                    QueryTimer::getInstance().addDownstreamTiming(query.id, result.timing_data);
                }
            }
            
            // Update message
            local_result.message = "Combined results from Process B and " + 
                                  std::to_string(downstream_results.size()) + " downstream processes";
            // End downstream query timing
            QueryTimer::getInstance().endTiming(query.id, "Downstream_Queries");
        }
        
        // Cache the result
        QueryTimer::getInstance().startTiming(query.id, "Cache_Storage");
        
        // End total processing timing
        QueryTimer::getInstance().endTiming(query.id, "Total_Processing");
       
        if (local_result.success) {
            // Serialize to cache
            std::ostringstream oss;
            for (const auto& entry : local_result.results) {
                oss << entry.key << ",";
                
                if (std::holds_alternative<int>(entry.value)) {
                    oss << "int," << std::get<int>(entry.value) << std::endl;
                }
                else if (std::holds_alternative<double>(entry.value)) {
                    oss << "double," << std::get<double>(entry.value) << std::endl;
                }
                else if (std::holds_alternative<bool>(entry.value)) {
                    oss << "bool," << (std::get<bool>(entry.value) ? "true" : "false") << std::endl;
                }
                else if (std::holds_alternative<std::string>(entry.value)) {
                    oss << "string," << std::get<std::string>(entry.value) << std::endl;
                }
                else if (std::holds_alternative<CrashData>(entry.value)) {
                    // For crash data, just store a placeholder in cache
                    oss << "string," << "CrashData:" << entry.key << std::endl;
                }
            }
            
            std::string cache_str = oss.str();
            std::vector<uint8_t> cache_data(cache_str.begin(), cache_str.end());
            
            // Cache for 5 seconds
            cache_->put(cache_key, cache_data, 5000);
        }
        QueryTimer::getInstance().endTiming(query.id, "Cache_Storage");
        
        // Add timing data to result
        local_result.timing_data = QueryTimer::getInstance().serializeTimingData(query.id);
        
      
        return local_result;
    }
    
    bool shouldForwardQuery(const Query& query) {
        // Determine if query should be forwarded based on content
        return query.query_string == "get_by_street" ||
               query.query_string == "get_by_key" ||
               query.query_string == "get_by_prefix" ||
               query.query_string == "get_by_date_range" ||
               query.query_string == "get_crashes_with_injuries" ||
               query.query_string == "get_crashes_with_fatalities"||
               query.query_string == "get_by_time";
    }
    
    void handleData(const std::string& source, 
                   const std::string& destination,
                   const std::vector<uint8_t>& data) {
        std::cout << "Process B received data from " << source << " to " << destination << std::endl;
        
        if (destination == "B") {
            // Process data meant for this process
            processData(source, data);
        } else {
            // Forward data to appropriate destination
            auto dest_it = clients_.find(destination);
            if (dest_it != clients_.end() && dest_it->second->isConnected()) {
                dest_it->second->sendData(source, destination, data);
            } else {
                std::cerr << "Cannot forward message to " << destination 
                          << ": client not connected" << std::endl;
            }
        }
    }
    
    void processData(const std::string& source, const std::vector<uint8_t>& data) {
        // Process incoming data
        // For demo, just print first few bytes
        std::cout << "Processing data from " << source << ": ";
        
        size_t bytes_to_print = std::min(data.size(), size_t(16));
        for (size_t i = 0; i < bytes_to_print; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                     << static_cast<int>(data[i]) << " ";
        }
        std::cout << std::dec << std::endl;
        
        // Additional processing would go here
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file> [data_file]" << std::endl;
        return 1;
    }
    
    std::string config_file = argv[1];
    std::string data_file = (argc > 2) ? argv[2] : "";
    
    try {
        ProcessB process(config_file, data_file);
        
        if (!process.start()) {
            std::cerr << "Failed to start Process B" << std::endl;
            return 1;
        }
        
        std::cout << "Process B started. Press Enter to exit." << std::endl;
        std::cin.get();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
