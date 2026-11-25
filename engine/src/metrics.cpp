#include "metrics.hpp"
#include "scheduler.hpp"
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <mutex>
#include <algorithm>

static std::mutex stdout_mu;

void LatencyHistogram::record(int64_t latency_us) {
    std::lock_guard<std::mutex> lock(mu_);
    if (values_.size() < CAPACITY) {
        values_.push_back(latency_us);
    } else {
        values_[next_] = latency_us;
        next_ = (next_ + 1) % CAPACITY;
    }
}

std::array<int64_t, 4> LatencyHistogram::compute() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (values_.empty()) return {0, 0, 0, 0};
    std::vector<int64_t> sorted(values_);
    std::sort(sorted.begin(), sorted.end());
    const auto percentile = [&sorted](double fraction) {
        const size_t index = static_cast<size_t>(
            fraction * static_cast<double>(sorted.size() - 1));
        return sorted[index];
    };
    return {percentile(0.50), percentile(0.90), percentile(0.95), percentile(0.99)};
}

double ThroughputCounter::take(double seconds) {
    if (seconds <= 0.0) return 0.0;
    return static_cast<double>(
        completed_.exchange(0, std::memory_order_relaxed)) / seconds;
}

const char* policy_name(PolicyType policy) {
    switch (policy) {
        case PolicyType::PRIORITY: return "PRIORITY";
        case PolicyType::SJF: return "SJF";
        case PolicyType::ROUND_ROBIN: return "RR";
    }
    return "UNKNOWN";
}

std::string to_json(const MetricsSnapshot& s) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(4);
    o << "{"
      << "\"t\":"          << s.t            << ","
      << "\"policy\":\""   << s.policy       << "\","
      << "\"burst\":"      << (s.in_burst ? "true" : "false") << ","
      << "\"p50_us\":"     << s.p50_us       << ","
      << "\"p90_us\":"     << s.p90_us       << ","
      << "\"p95_us\":"     << s.p95_us       << ","
      << "\"p99_us\":"     << s.p99_us       << ","
      << "\"tput\":"       << std::setprecision(1) << s.throughput << ","
      << "\"fairness\":"   << std::setprecision(4) << s.fairness   << ","
      << "\"sr_hft\":"     << s.service_ratio[0]   << ","
      << "\"sr_retail\":"  << s.service_ratio[1]   << ","
      << "\"sr_mm\":"      << s.service_ratio[2]   << ","
      << "\"starved\":"    << s.starvation_count    << ","
      << "\"cache_pred\":" << s.cache_hit_pred      << ","
      << "\"cache_lru\":"  << s.cache_hit_lru       << ","
      << "\"wal_crit_ms\":" << std::setprecision(3) << s.wal_crit_lat_ms << ","
      << "\"wal_rout_ms\":" << s.wal_rout_lat_ms    << ","
      << "\"wal_lift\":"   << std::setprecision(2)  << s.wal_lift  << ","
      << "\"wal_flushes\":" << s.wal_flush_count << ","
      << "\"switches\":"   << s.policy_switches      << ","
      << "\"q_len\":"      << s.queue_len << ","
      << "\"book\":{\"symbol\":\"" << s.book_symbol << "\",\"bids\":[";
    for (size_t i = 0; i < s.bids.size(); ++i) {
        if (i) o << ',';
        o << "[" << std::setprecision(2) << s.bids[i].price << ',' << s.bids[i].quantity << "]";
    }
    o << "],\"asks\":[";
    for (size_t i = 0; i < s.asks.size(); ++i) {
        if (i) o << ',';
        o << "[" << std::setprecision(2) << s.asks[i].price << ',' << s.asks[i].quantity << "]";
    }
    o << "]}"
      << "}";
    return o.str();
}

void print_json(const MetricsSnapshot& s) {
    std::string line = to_json(s) + "\n";
    std::lock_guard<std::mutex> lk(stdout_mu);
    std::fputs(line.c_str(), stdout);
    std::fflush(stdout);
}
