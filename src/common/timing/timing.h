#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <iostream>

namespace mini2 {

// Class for measuring and reporting timings
class QueryTimer {
public:
    // Get singleton instance
    static QueryTimer& getInstance();
    
    // Start timing for a specific query ID
    void startTiming(const std::string& query_id, const std::string& process_id);
    
    // End timing for a specific query ID and operation
    void endTiming(const std::string& query_id, const std::string& operation);
    
    // Get timing report for a query
    std::string getTimingReport(const std::string& query_id);
    
    // Add downstream timing from other processes
    void addDownstreamTiming(const std::string& query_id, const std::string& timing_data);
    
    // Serialize timing data for transmission
    std::string serializeTimingData(const std::string& query_id);
    
    // Clear timing data for a query
    void clearTiming(const std::string& query_id);
    
private:
    QueryTimer() = default;
    
    struct TimingInfo {
        std::string process_id;
        std::chrono::high_resolution_clock::time_point start_time;
        std::unordered_map<std::string, double> operation_times; // in milliseconds
        std::vector<std::string> downstream_timings;
    };
    
    std::unordered_map<std::string, TimingInfo> timings_;
    std::mutex mutex_;
};

} // namespace mini2
