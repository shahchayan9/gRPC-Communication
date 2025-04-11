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


//  Process B handles crash data queries, local data access, and upstream coordination.

class ProcessB {
public:
    ProcessB(const std::string& config_file, const std::string& data_file)
        : running_(false) {
        
        ConfigLoader::getInstance().loadFromFile(config_file);
        process_info_ = ConfigLoader::getInstance().getProcessInfo("B");

        std::string server_address = process_info_.host + ":" + std::to_string(process_info_.port);
        server_ = std::make_unique<DataServiceServer>("B", server_address);

        data_store_ = &DataStore::getInstance("process_b");

        // Load dataset (from CSV or demo)
        if (!data_file.empty()) {
            data_store_->loadCrashDataFromCSV(data_file);
        } else {
            std::string default_path = "data/process_b/process1.csv";
            if (std::ifstream(default_path).good()) {
                data_store_->loadCrashDataFromCSV(default_path);
            } else {
                std::cout << "Default data file not found at: " << default_path << ". No data loaded for Process C." << std::endl;
            }
        }

        cache_ = SharedCache::create("process_b_cache", 1024 * 1024);  // 1MB shared memory cache
    }

    ~ProcessB() {
        stop();
    }

    bool start() {
        if (running_) return true;

        server_->setQueryHandler([this](const Query& query) {
            return handleQuery(query);
        });

        server_->setDataHandler([this](const std::string& source,
                                       const std::string& destination,
                                       const std::vector<uint8_t>& data) {
            handleData(source, destination, data);
        });

        connectToDownstreamServers();

        if (!server_->start()) return false;

        running_ = true;
        return true;
    }

    void stop() {
        if (!running_) return;
        running_ = false;
        if (server_) server_->stop();
        clients_.clear();
    }

private:
    bool running_;
    ProcessInfo process_info_;
    std::unique_ptr<DataServiceServer> server_;
    std::unordered_map<std::string, std::unique_ptr<DataServiceClient>> clients_;
    DataStore* data_store_;
    std::shared_ptr<SharedCache> cache_;

    void connectToDownstreamServers() {
        for (const auto& conn_id : process_info_.connections) {
            try {
                auto conn_info = ConfigLoader::getInstance().getProcessInfo(conn_id);
                std::string target = conn_info.host + ":" + std::to_string(conn_info.port);

                std::cout << "Connecting to " << conn_id << " at " << target << std::endl;
                clients_[conn_id] = std::make_unique<DataServiceClient>(target);
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
                std::cout << query.parameters[i] << (i < query.parameters.size() - 1 ? ", " : "");
            }
        }
        std::cout << std::endl;

        QueryTimer::getInstance().startTiming(query.id, "B");

        std::string cache_key = "query_" + query.query_string;
        for (const auto& param : query.parameters) {
            cache_key += "_" + param;
        }

        std::vector<uint8_t> cached_data;
        if (cache_->get(cache_key, cached_data)) {
            QueryResult cached_result;
            cached_result.query_id = query.id;
            cached_result.success = true;
            cached_result.message = "From cache";

            std::istringstream iss(std::string(cached_data.begin(), cached_data.end()));
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

                    if (type_str == "int") entry.value = std::stoi(value_str);
                    else if (type_str == "double") entry.value = std::stod(value_str);
                    else if (type_str == "bool") entry.value = (value_str == "true" || value_str == "1");
                    else entry.value = value_str;

                    cached_result.results.push_back(entry);
                }
            }

            QueryTimer::getInstance().endTiming(query.id, "Cache_Access");
            QueryTimer::getInstance().endTiming(query.id, "Total_Processing");
            cached_result.timing_data = QueryTimer::getInstance().serializeTimingData(query.id);
            std::cout << "Cache hit for query " << cache_key << std::endl;
            return cached_result;
        }

        // Local query processing
        QueryTimer::getInstance().startTiming(query.id, "Local_Processing");
        QueryResult local_result;

        if (query.query_string == "get_by_borough" && !query.parameters.empty() && query.parameters[0] == "BROOKLYN") {
            local_result = data_store_->getByBorough("BROOKLYN");
        } else {
            local_result = data_store_->processQuery(query);
        }

        QueryTimer::getInstance().endTiming(query.id, "Local_Processing");

        // Forwarding logic
        if (query.query_string != "get_by_borough" &&
            (query.query_string == "get_all" || shouldForwardQuery(query))) {

            QueryTimer::getInstance().startTiming(query.id, "Downstream_Queries");
            std::vector<QueryResult> downstream_results;

            for (const auto& [conn_id, client] : clients_) {
                if (client->isConnected()) {
                    QueryTimer::getInstance().startTiming(query.id, "Query_To_" + conn_id);
                    auto result = client->queryData(query);
                    QueryTimer::getInstance().endTiming(query.id, "Query_To_" + conn_id);

                    if (result.success) {
                        downstream_results.push_back(result);
                    }
                }
            }

            for (const auto& result : downstream_results) {
                local_result.results.insert(local_result.results.end(),
                                            result.results.begin(),
                                            result.results.end());

                if (!result.timing_data.empty()) {
                    QueryTimer::getInstance().addDownstreamTiming(query.id, result.timing_data);
                }
            }

            local_result.message = "Combined results from Process B and " +
                                   std::to_string(downstream_results.size()) + " downstream processes";
            QueryTimer::getInstance().endTiming(query.id, "Downstream_Queries");
        }

        // Cache results if successful
        QueryTimer::getInstance().startTiming(query.id, "Cache_Storage");

        if (local_result.success) {
            std::ostringstream oss;
            for (const auto& entry : local_result.results) {
                oss << entry.key << ",";
                if (std::holds_alternative<int>(entry.value)) {
                    oss << "int," << std::get<int>(entry.value);
                } else if (std::holds_alternative<double>(entry.value)) {
                    oss << "double," << std::get<double>(entry.value);
                } else if (std::holds_alternative<bool>(entry.value)) {
                    oss << "bool," << (std::get<bool>(entry.value) ? "true" : "false");
                } else if (std::holds_alternative<std::string>(entry.value)) {
                    oss << "string," << std::get<std::string>(entry.value);
                } else {
                    oss << "string," << "CrashData:" << entry.key;
                }
                oss << std::endl;
            }

            std::string cache_str = oss.str();
            std::vector<uint8_t> cache_data(cache_str.begin(), cache_str.end());
            cache_->put(cache_key, cache_data, 5000);  // TTL = 5s
        }

        QueryTimer::getInstance().endTiming(query.id, "Cache_Storage");
        QueryTimer::getInstance().endTiming(query.id, "Total_Processing");

        local_result.timing_data = QueryTimer::getInstance().serializeTimingData(query.id);
        return local_result;
    }

    bool shouldForwardQuery(const Query& query) {
        return query.query_string == "get_by_street" ||
               query.query_string == "get_by_key" ||
               query.query_string == "get_by_prefix" ||
               query.query_string == "get_by_date_range" ||
               query.query_string == "get_crashes_with_injuries" ||
               query.query_string == "get_crashes_with_fatalities" ||
               query.query_string == "get_by_time";
    }

    void handleData(const std::string& source,
                    const std::string& destination,
                    const std::vector<uint8_t>& data) {
        std::cout << "Process B received data from " << source << " to " << destination << std::endl;

        if (destination == "B") {
            processData(source, data);
        } else {
            auto dest_it = clients_.find(destination);
            if (dest_it != clients_.end() && dest_it->second->isConnected()) {
                dest_it->second->sendData(source, destination, data);
            } else {
                std::cerr << "Cannot forward message to " << destination << ": client not connected" << std::endl;
            }
        }
    }

    void processData(const std::string& source, const std::vector<uint8_t>& data) {
        std::cout << "Processing data from " << source << ": ";
        size_t bytes_to_print = std::min(data.size(), size_t(16));
        for (size_t i = 0; i < bytes_to_print; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(data[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }
};


// Main

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
