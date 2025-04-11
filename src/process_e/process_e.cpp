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

// Leaf node Process E: handles data loading, caching, and query processing.
class ProcessE {
public:
    ProcessE(const std::string& config_file, const std::string& data_file)
        : running_(false) {

        ConfigLoader::getInstance().loadFromFile(config_file);
        process_info_ = ConfigLoader::getInstance().getProcessInfo("E");

        std::string server_address = process_info_.host + ":" + std::to_string(process_info_.port);
        server_ = std::make_unique<DataServiceServer>("E", server_address);

        data_store_ = &DataStore::getInstance("process_e");
        bool loaded_files = false;

        if (!data_file.empty()) {
            data_store_->loadCrashDataFromCSV(data_file);
            loaded_files = true;
        } else {
            std::vector<std::string> paths = {
                "data/process_e/process4.csv",
                "data/process_e/other_crashes.csv"
            };

            for (const auto& path : paths) {
                std::ifstream test_file(path);
                if (test_file.good()) {
                    test_file.close();
                    data_store_->loadCrashDataFromCSV(path);
                    loaded_files = true;
                }
            }
        }

        if (!loaded_files) {
            std::cout << "No default data files found. Process E started with empty data store." << std::endl;
        }

        cache_ = SharedCache::create("process_e_cache", 1024 * 1024);
    }

    ~ProcessE() { stop(); }

    bool start() {
        if (running_) return true;

        server_->setQueryHandler([this](const Query& query) { return handleQuery(query); });
        server_->setDataHandler([this](const std::string& source, const std::string& destination, const std::vector<uint8_t>& data) {
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

    QueryResult handleQuery(const Query& query) {
        std::cout << "Process E received query: " << query.query_string;
        if (!query.parameters.empty()) {
            std::cout << " with parameters: ";
            for (size_t i = 0; i < query.parameters.size(); ++i) {
                std::cout << query.parameters[i];
                if (i < query.parameters.size() - 1) std::cout << ", ";
            }
        }
        std::cout << std::endl;

        QueryTimer::getInstance().startTiming(query.id, "E");

        std::string cache_key = "query_" + query.query_string;
        for (const auto& param : query.parameters) cache_key += "_" + param;

        std::vector<uint8_t> cached_data;
        if (cache_->get(cache_key, cached_data)) {
            QueryTimer::getInstance().endTiming(query.id, "Cache_Access");
            QueryResult cached_result;
            cached_result.query_id = query.id;
            cached_result.success = true;
            cached_result.message = "From cache";

            std::istringstream iss(std::string(cached_data.begin(), cached_data.end()));
            std::string line;
            while (std::getline(iss, line)) {
                std::istringstream line_iss(line);
                std::string key, type_str, value_str;
                if (std::getline(line_iss, key, ',') && std::getline(line_iss, type_str, ',') && std::getline(line_iss, value_str)) {
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

            QueryTimer::getInstance().endTiming(query.id, "Total_Processing");
            cached_result.timing_data = QueryTimer::getInstance().serializeTimingData(query.id);
            return cached_result;
        }

        QueryResult local_result;
        QueryTimer::getInstance().startTiming(query.id, "Local_Processing");

        if (query.query_string == "get_by_borough") {
            const std::string& borough = query.parameters.empty() ? "" : query.parameters[0];
            if (borough == "STATEN ISLAND") {
                local_result = data_store_->getByBorough(borough);
            } else if (borough != "BRONX" && borough != "BROOKLYN" && borough != "QUEENS") {
                local_result = data_store_->getByBorough(borough);
            } else {
                local_result = QueryResult::createSuccess(query.id, {}, "No matching borough data requested");
            }
        } else {
            local_result = data_store_->processQuery(query);
        }

        QueryTimer::getInstance().endTiming(query.id, "Local_Processing");

        QueryTimer::getInstance().startTiming(query.id, "Cache_Storage");
        if (local_result.success) {
            std::ostringstream oss;
            for (const auto& entry : local_result.results) {
                oss << entry.key << ",";
                if (std::holds_alternative<int>(entry.value)) oss << "int," << std::get<int>(entry.value);
                else if (std::holds_alternative<double>(entry.value)) oss << "double," << std::get<double>(entry.value);
                else if (std::holds_alternative<bool>(entry.value)) oss << "bool," << (std::get<bool>(entry.value) ? "true" : "false");
                else if (std::holds_alternative<std::string>(entry.value)) oss << "string," << std::get<std::string>(entry.value);
                else if (std::holds_alternative<CrashData>(entry.value)) oss << "string," << "CrashData:" << entry.key;
                oss << std::endl;
            }
            std::vector<uint8_t> cache_data(oss.str().begin(), oss.str().end());
            cache_->put(cache_key, cache_data, 5000);
        }
        QueryTimer::getInstance().endTiming(query.id, "Cache_Storage");
        QueryTimer::getInstance().endTiming(query.id, "Total_Processing");

        local_result.timing_data = QueryTimer::getInstance().serializeTimingData(query.id);
        return local_result;
    }

    void handleData(const std::string& source, const std::string& destination, const std::vector<uint8_t>& data) {
        std::cout << "Process E received data from " << source << " to " << destination << std::endl;
        if (destination == "E") {
            processData(source, data);
        } else {
            std::cerr << "Process E (leaf node) received forwarding request to " << destination
                      << " but cannot forward messages" << std::endl;
        }
    }

    void processData(const std::string& source, const std::vector<uint8_t>& data) {
        std::cout << "Processing data from " << source << ": ";
        size_t bytes_to_print = std::min(data.size(), size_t(16));
        for (size_t i = 0; i < bytes_to_print; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }

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
