#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

struct MetricsSnapshot {
    struct BookLevel { double price = 0.0; uint32_t quantity = 0; };
    int64_t t = 0;
    std::string policy;
    bool in_burst = false;
    int64_t p50_us = 0;
    int64_t p90_us = 0;
    int64_t p95_us = 0;
    int64_t p99_us = 0;
    double throughput = 0.0;
    double fairness = 1.0;
    double service_ratio[3]{};
    uint64_t starvation_count = 0;
    double cache_hit_pred = 0.0;
    double cache_hit_lru = 0.0;
    double wal_crit_lat_ms = 0.0;
    double wal_rout_lat_ms = 0.0;
    double wal_lift = 0.0;
    uint64_t wal_flush_count = 0;
    int policy_switches = 0;
    size_t queue_len = 0;
    std::string book_symbol;
    std::vector<BookLevel> bids;
    std::vector<BookLevel> asks;
};

class LatencyHistogram {
public:
    void record(int64_t latency_us);
    std::array<int64_t, 4> compute() const;

private:
    mutable std::mutex mu_;
    std::vector<int64_t> values_;
    size_t next_ = 0;
    static constexpr size_t CAPACITY = 10000;
};

class ThroughputCounter {
public:
    void record_completion() { completed_.fetch_add(1, std::memory_order_relaxed); }
    double take(double seconds);

private:
    std::atomic<uint64_t> completed_{0};
};

std::string to_json(const MetricsSnapshot& snapshot);
void print_json(const MetricsSnapshot& snapshot);
