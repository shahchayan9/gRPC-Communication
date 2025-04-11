#include "timing.h"
#include <sstream>
#include <iomanip>

namespace mini2 {

QueryTimer& QueryTimer::getInstance() {
    static QueryTimer instance;
    return instance;
}

void QueryTimer::startTiming(const std::string& query_id, const std::string& process_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create or update timing info
    TimingInfo& info = timings_[query_id];
    info.process_id = process_id;
    info.start_time = std::chrono::high_resolution_clock::now();
}

void QueryTimer::endTiming(const std::string& query_id, const std::string& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = timings_.find(query_id);
    if (it != timings_.end()) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - it->second.start_time).count();
        
        it->second.operation_times[operation] = static_cast<double>(duration) / 1000.0; // Convert to seconds
    }
}

std::string QueryTimer::getTimingReport(const std::string& query_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    
    auto it = timings_.find(query_id);
    if (it != timings_.end()) {
        const TimingInfo& info = it->second;
        
        oss << "Timing Report for Query " << query_id << " (Process " << info.process_id << "):\n";
        
        // Local operation times
        oss << "Local Operations:\n";
        for (const auto& [operation, time] : info.operation_times) {
            oss << "  " << std::left << std::setw(20) << operation << ": " 
                << std::fixed << std::setprecision(6) << time << " seconds\n";
        }
        
        // Downstream timings
        if (!info.downstream_timings.empty()) {
            oss << "\nDownstream Processes:\n";
            for (const auto& downstream : info.downstream_timings) {
                oss << downstream;
            }
        }
    } else {
        oss << "No timing data available for query " << query_id;
    }
    
    return oss.str();
}

void QueryTimer::addDownstreamTiming(const std::string& query_id, const std::string& timing_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = timings_.find(query_id);
    if (it != timings_.end()) {
        it->second.downstream_timings.push_back(timing_data);
    }
}

std::string QueryTimer::serializeTimingData(const std::string& query_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    
    auto it = timings_.find(query_id);
    if (it != timings_.end()) {
        const TimingInfo& info = it->second;
        
        oss << "  [Process " << info.process_id << "]\n";
        
        // Operation times
        for (const auto& [operation, time] : info.operation_times) {
            oss << "    " << std::left << std::setw(20) << operation << ": " 
                << std::fixed << std::setprecision(6) << time << " seconds\n";
        }
        
        // Downstream timings
        for (const auto& downstream : info.downstream_timings) {
            oss << downstream;
        }
    }
    
    return oss.str();
}

void QueryTimer::clearTiming(const std::string& query_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    timings_.erase(query_id);
}

} // namespace mini2
