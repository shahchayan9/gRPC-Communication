#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <condition_variable>
#include <queue>

#include "config/config_loader.h"
#include "data/data_structures.h"
#include "grpc/data_service.h"
#include "timing/timing.h"
#include "shared_memory/shared_memory.h"

using namespace mini2;


// Simple shared memory-based cache for storing query results

class QueryCache {
public:
    explicit QueryCache(const std::string& cache_name, size_t max_size = 1024 * 1024)
        : cache_(SharedCache::create(cache_name, max_size)) {}

    bool get(const std::string& query_id, QueryResult& result) {
        std::vector<uint8_t> data;
        if (cache_->get(query_id, data)) {
            std::istringstream iss(std::string(data.begin(), data.end()));
            std::string success_str, message, count_str;

            std::getline(iss, success_str, ',');
            std::getline(iss, message, ',');
            std::getline(iss, count_str, ',');

            result.query_id = query_id;
            result.success = (success_str == "true");
            result.message = message;

            int count = std::stoi(count_str);
            result.results.resize(count);

            for (int i = 0; i < count; ++i) {
                std::string key, type, value;
                std::getline(iss, key, ',');
                std::getline(iss, type, ',');
                std::getline(iss, value, ',');

                result.results[i].key = key;
                result.results[i].timestamp = DataEntry::getCurrentTimestamp();

                if (type == "int") result.results[i].value = std::stoi(value);
                else if (type == "double") result.results[i].value = std::stod(value);
                else if (type == "bool") result.results[i].value = (value == "true");
                else result.results[i].value = value;
            }

            return true;
        }
        return false;
    }

    void put(const std::string& query_id, const QueryResult& result, int ttl_ms = 10000) {
        std::ostringstream oss;
        oss << (result.success ? "true" : "false") << ","
            << result.message << ","
            << result.results.size();

        for (const auto& entry : result.results) {
            oss << "," << entry.key << ",";
            if (std::holds_alternative<int>(entry.value)) {
                oss << "int," << std::get<int>(entry.value);
            } else if (std::holds_alternative<double>(entry.value)) {
                oss << "double," << std::get<double>(entry.value);
            } else if (std::holds_alternative<bool>(entry.value)) {
                oss << "bool," << (std::get<bool>(entry.value) ? "true" : "false");
            } else if (std::holds_alternative<std::string>(entry.value)) {
                oss << "string," << std::get<std::string>(entry.value);
            } else {
                oss << "unknown,";
            }
        }

        std::vector<uint8_t> data(oss.str().begin(), oss.str().end());
        cache_->put(query_id, data, ttl_ms);
    }

private:
    std::shared_ptr<SharedCache> cache_;
};

// ----------------------------------------------------------------------------
// Thread-safe message queue for async communication
// ----------------------------------------------------------------------------
class MessageQueue {
public:
    struct Message {
        std::string source;
        std::string destination;
        std::vector<uint8_t> data;
    };

    void push(Message msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(msg));
        cv_.notify_one();
    }

    bool pop(Message& msg, int timeout_ms = -1) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeout_ms < 0) {
            cv_.wait(lock, [this] { return !queue_.empty(); });
        } else {
            if (!cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return !queue_.empty(); })) {
                return false;
            }
        }

        msg = std::move(queue_.front());
        queue_.pop();
        return true;
    }

private:
    std::queue<Message> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};


// Process A: Coordinator handling client queries and dispatching downstream

class ProcessA {
public:
    explicit ProcessA(const std::string& config_file)
        : running_(false), query_cache_("process_a") {
        ConfigLoader::getInstance().loadFromFile(config_file);
        process_info_ = ConfigLoader::getInstance().getProcessInfo("A");

        std::string server_address = process_info_.host + ":" + std::to_string(process_info_.port);
        server_ = std::make_unique<DataServiceServer>("A", server_address);

        message_thread_ = std::thread(&ProcessA::processMessages, this);
    }

    ~ProcessA() {
        stop();
        if (message_thread_.joinable()) {
            message_thread_.join();
        }
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

        if (!server_->start()) {
            return false;
        }

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
    MessageQueue message_queue_;
    std::thread message_thread_;
    QueryCache query_cache_;

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
        std::cout << "Received query: " << query.query_string;
        if (!query.parameters.empty()) {
            std::cout << " with parameters: ";
            for (size_t i = 0; i < query.parameters.size(); ++i) {
                std::cout << query.parameters[i];
                if (i < query.parameters.size() - 1) std::cout << ", ";
            }
        }
        std::cout << std::endl;

        QueryTimer::getInstance().startTiming(query.id, "A");

        std::string cache_key = query.query_string;
        for (const auto& param : query.parameters) {
            cache_key += "_" + param;
        }

        QueryResult cached_result;
        if (query_cache_.get(cache_key, cached_result)) {
            std::cout << "Cache hit for query " << cache_key << std::endl;
            cached_result.message = "From cache: " + cached_result.message;

            QueryTimer::getInstance().endTiming(query.id, "Cache_Access");
            QueryTimer::getInstance().endTiming(query.id, "Total_Processing");
            cached_result.timing_data = QueryTimer::getInstance().serializeTimingData(query.id);
            return cached_result;
        }

        std::vector<QueryResult> downstream_results;
        for (const auto& [conn_id, client] : clients_) {
            if (client->isConnected()) {
                auto result = client->queryData(query);
                if (result.success) {
                    downstream_results.push_back(result);
                }
            }
        }

        QueryTimer::getInstance().endTiming(query.id, "Downstream_Queries");

        QueryResult final_result;
        final_result.query_id = query.id;
        final_result.success = true;

        for (const auto& result : downstream_results) {
            if (!result.timing_data.empty()) {
                QueryTimer::getInstance().addDownstreamTiming(query.id, result.timing_data);
            }
        }

        final_result.message = "Combined results from " + std::to_string(downstream_results.size()) + " sources";

        int total_entries = 0;
        for (const auto& result : downstream_results) {
            final_result.results.insert(final_result.results.end(), result.results.begin(), result.results.end());
            total_entries += result.results.size();
        }

        final_result.message += " (" + std::to_string(total_entries) + " total entries)";
        QueryTimer::getInstance().endTiming(query.id, "Total_Processing");
        final_result.timing_data = QueryTimer::getInstance().serializeTimingData(query.id);

        query_cache_.put(cache_key, final_result);

        std::cout << "\n===== Timing Report =====\n";
        std::cout << QueryTimer::getInstance().getTimingReport(query.id) << std::endl;

        return final_result;
    }

    void handleData(const std::string& source,
                    const std::string& destination,
                    const std::vector<uint8_t>& data) {
        std::cout << "Received data from " << source << " to " << destination << std::endl;

        if (destination == "A") {
            processData(source, data);
        } else {
            message_queue_.push({source, destination, data});
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

    void processMessages() {
        while (running_) {
            MessageQueue::Message msg;
            if (message_queue_.pop(msg, 100)) {
                auto it = clients_.find(msg.destination);
                if (it != clients_.end() && it->second->isConnected()) {
                    it->second->sendData(msg.source, msg.destination, msg.data);
                } else {
                    std::cerr << "Cannot forward message to " << msg.destination
                              << ": client not connected" << std::endl;
                }
            }
        }
    }
};


// Main

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    std::string config_file = argv[1];

    try {
        ProcessA process(config_file);
        if (!process.start()) {
            std::cerr << "Failed to start Process A" << std::endl;
            return 1;
        }

        std::cout << "Process A started. Press Enter to exit." << std::endl;
        std::cin.get();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
