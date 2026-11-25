#pragma once
#include "order.hpp"
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <cstdint>
#include <vector>

enum class LogPriority : uint8_t { CRITICAL = 0, ROUTINE = 1 };

struct LogRecord {
    LogPriority  priority;
    std::string  payload;       // serialised as JSON line
    std::chrono::steady_clock::time_point enqueue_time;
};

struct WALConfig {
    int    batch_ms     = 10;   // routine flush interval
    size_t batch_max    = 256;  // max routine records per flush
    std::function<void(const std::vector<LogRecord>&)> flush_fn = nullptr;
};

// Priority Write-Ahead Log.
// Critical records (fills, cancels) are flushed immediately.
// Routine records (arrivals) are batched and flushed every batch_ms.
class WAL {
public:
    using Config = WALConfig;

    explicit WAL(const WALConfig& cfg = WALConfig{});
    ~WAL();

    void write(LogRecord rec);

    struct Stats {
        uint64_t critical_written;
        uint64_t routine_written;
        uint64_t flush_count;
        double   avg_critical_latency_ms;
        double   avg_routine_latency_ms;
        double   priority_lift;   // routine_latency / critical_latency
    };
    Stats stats() const;

    void stop();

private:
    WALConfig cfg_;
    std::atomic<bool> running_ {true};

    // Critical queue — tiny, flushed immediately
    std::mutex crit_mu_;
    std::condition_variable crit_cv_;
    std::queue<LogRecord> crit_q_;

    // Routine queue — batched
    std::mutex rout_mu_;
    std::queue<LogRecord> rout_q_;

    // Stats
    std::atomic<uint64_t> crit_written_  {0};
    std::atomic<uint64_t> rout_written_  {0};
    std::atomic<uint64_t> flush_count_   {0};
    std::atomic<int64_t>  crit_lat_sum_  {0};  // µs
    std::atomic<int64_t>  rout_lat_sum_  {0};

    std::thread worker_;
    void run();
};
