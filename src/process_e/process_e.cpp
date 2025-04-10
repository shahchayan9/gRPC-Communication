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

using namespace mini2;

// Process E implementation
class ProcessE {
public:
   ProcessE(const std::string& config_file, const std::string& data_file)
       : running_(false) {
       
       // Load configuration
       ConfigLoader::getInstance().loadFromFile(config_file);
       
       // Get process info
       process_info_ = ConfigLoader::getInstance().getProcessInfo("E");
       
       // Create gRPC server
       std::string server_address = process_info_.host + ":" + std::to_string(process_info_.port);
       server_ = std::make_unique<DataServiceServer>("E", server_address);
       
       // Load data
       data_store_ = &DataStore::getInstance("process_e");
       if (!data_file.empty()) {
           // Load crash data from CSV
           data_store_->loadCrashDataFromCSV(data_file);
       } else {
           // Load default data paths if they exist
           std::string staten_island_path = "data/process_e/staten_island_crashes.csv";
           std::string other_path = "data/process_e/other_crashes.csv";
           
           bool loaded_files = false;
           
           std::ifstream test_file(staten_island_path);
           if (test_file.good()) {
               test_file.close();
               data_store_->loadCrashDataFromCSV(staten_island_path);
               loaded_files = true;
           }
           
           test_file = std::ifstream(other_path);
           if (test_file.good()) {
               test_file.close();
               data_store_->loadCrashDataFromCSV(other_path);
               loaded_files = true;
           }
           
           if (!loaded_files) {
               // Load some demo data
               loadDemoData();
           }
       }
       
       // Create shared cache
       cache_ = SharedCache::create("process_e_cache", 1024 * 1024); // 1MB cache
   }
   
   ~ProcessE() {
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
       // Create demo crash data for STATEN ISLAND
       for (int i = 0; i < 5; ++i) {
           CrashData crash;
           crash.crash_date = "12/13/2021";
           crash.crash_time = "11:" + std::to_string(i) + "0";
           crash.borough = "STATEN ISLAND";
           crash.zip_code = "10301";
           crash.latitude = "40.6423";
           crash.longitude = "-74.0841";
           crash.location = "(40.6423, -74.0841)";
           crash.on_street_name = "VICTORY BOULEVARD";
           crash.cross_street_name = "BAY STREET";
           crash.off_street_name = "";
           crash.persons_injured = i % 3;
           crash.persons_killed = (i % 4 == 0) ? 1 : 0;
           crash.pedestrians = i % 2;
           
           // Generate a unique key for this crash
           std::string key = "staten_island_crash_" + std::to_string(i);
           
           // Store in data store
           data_store_->store(DataEntry::createCrashData(key, crash));
       }
       
       // Create demo crash data for other boroughs and unspecified
       for (int i = 0; i < 5; ++i) {
           CrashData crash;
           crash.crash_date = "12/10/2021";
           crash.crash_time = "12:" + std::to_string(i) + "0";
           crash.borough = ""; // Unspecified borough
           crash.zip_code = "10000";
           crash.latitude = "40.7500";
           crash.longitude = "-73.9500";
           crash.location = "(40.7500, -73.9500)";
           crash.on_street_name = "UNKNOWN STREET";
           crash.cross_street_name = "SOMEWHERE AVE";
           crash.off_street_name = "";
           crash.persons_injured = i;
           crash.persons_killed = 0;
           crash.pedestrians = i % 2;
           
           // Generate a unique key for this crash
           std::string key = "other_crash_" + std::to_string(i);
           
           // Store in data store
           data_store_->store(DataEntry::createCrashData(key, crash));
       }
       
       std::cout << "Created 10 demo crash records (5 for STATEN ISLAND and 5 for other)" << std::endl;
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
       std::cout << "Process E received query: " << query.query_string;
       if (!query.parameters.empty()) {
           std::cout << " with parameters: ";
           for (const auto& param : query.parameters) {
               std::cout << param << " ";
           }
       }
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
       
       // Process specific queries for crash data
       if (query.query_string == "get_by_borough") {
           if (!query.parameters.empty() && query.parameters[0] == "STATEN ISLAND") {
               // This is our borough, process locally
               local_result = data_store_->getByBorough("STATEN ISLAND");
           } else if (!query.parameters.empty() && 
                     (query.parameters[0] != "BROOKLYN" && 
                      query.parameters[0] != "QUEENS" && 
                      query.parameters[0] != "BRONX")) {
               // This might be an "other" or unspecified borough, process locally
               local_result = data_store_->getByBorough(query.parameters[0]);
           } else {
               // Not our borough, return empty result
               local_result = QueryResult::createSuccess(query.id, {}, "No matching borough data requested");
           }
       }
       else if (query.query_string == "get_by_street" || 
               query.query_string == "get_by_date_range" ||
               query.query_string == "get_crashes_with_injuries" ||
               query.query_string == "get_crashes_with_fatalities" ||
               query.query_string == "get_by_time" ) {
           // Process these special queries locally
           local_result = data_store_->processQuery(query);
       }
       else {
           // Standard queries like get_all, get_by_key, get_by_prefix
           local_result = data_store_->processQuery(query);
       }
       
       // Process E is a leaf node, so we don't forward queries
       
       // Cache the result
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
       
       return local_result;
   }
   
   void handleData(const std::string& source, 
                  const std::string& destination,
                  const std::vector<uint8_t>& data) {
       std::cout << "Process E received data from " << source << " to " << destination << std::endl;
       
       if (destination == "E") {
           // Process data meant for this process
           processData(source, data);
       } else {
           // We are a leaf node, so this is unexpected
           std::cerr << "Process E (leaf node) received forwarding request to " << destination 
                     << " but cannot forward messages" << std::endl;
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
       ProcessE process(config_file, data_file);
       
       if (!process.start()) {
           std::cerr << "Failed to start Process E" << std::endl;
           return 1;
       }
       
       std::cout << "Process E started. Press Enter to exit." << std::endl;
       std::cin.get();
       
   } catch (const std::exception& e) {
       std::cerr << "Exception: " << e.what() << std::endl;
       return 1;
   }
   
   return 0;
}
