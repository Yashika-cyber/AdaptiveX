#pragma once

#include "order.hpp"

#include <array>
#include <atomic>
#include <cstdint>

double jains_index(const double* values, size_t count);

class FairnessTracker {
public:
    static constexpr int N = NUM_TRADER_TYPES;

    struct Report {
        uint64_t submissions[N]{};
        uint64_t completions[N]{};
        double service_ratio[N]{};
        double jain_index = 1.0;
        uint64_t starvation_count = 0;
    };

    FairnessTracker();
    void record_submission(TraderType type);
    void record_completion(TraderType type);
    void set_median_wait_us(int64_t median);
    void record_wait(TraderType type, int64_t wait_us);
    Report compute() const;
    void reset_counters();

private:
    std::array<std::atomic<uint64_t>, N> submitted_{};
    std::array<std::atomic<uint64_t>, N> completed_{};
    std::atomic<uint64_t> starved_{0};
    std::atomic<int64_t> median_us_{1000};
};
