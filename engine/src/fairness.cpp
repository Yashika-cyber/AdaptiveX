#include "fairness.hpp"
#include <cstring>
#include <cmath>

double jains_index(const double* values, size_t count) {
    if (count == 0) return 1.0;
    double sum = 0.0;
    double squares = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum += values[i];
        squares += values[i] * values[i];
    }
    return squares == 0.0
        ? 1.0
        : (sum * sum) / (static_cast<double>(count) * squares);
}

FairnessTracker::FairnessTracker() {
    for (int i = 0; i < N; i++) {
        submitted_[i].store(0);
        completed_[i].store(0);
    }
    starved_.store(0);
    median_us_.store(1000);  // default 1 ms
}

void FairnessTracker::record_submission(TraderType t) {
    submitted_[static_cast<int>(t)].fetch_add(1, std::memory_order_relaxed);
}

void FairnessTracker::record_completion(TraderType t) {
    completed_[static_cast<int>(t)].fetch_add(1, std::memory_order_relaxed);
}

void FairnessTracker::set_median_wait_us(int64_t median) {
    median_us_.store(median, std::memory_order_relaxed);
}

void FairnessTracker::record_wait(TraderType t, int64_t wait_us) {
    (void)t;
    int64_t median = median_us_.load(std::memory_order_relaxed);
    if (wait_us > 10 * median)
        starved_.fetch_add(1, std::memory_order_relaxed);
}

FairnessTracker::Report FairnessTracker::compute() const {
    Report r{};
    double x[N];
    for (int i = 0; i < N; i++) {
        r.submissions[i] = submitted_[i].load(std::memory_order_relaxed);
        r.completions[i] = completed_[i].load(std::memory_order_relaxed);
        x[i] = (r.submissions[i] > 0)
            ? static_cast<double>(r.completions[i]) / r.submissions[i]
            : 1.0;
        r.service_ratio[i] = x[i];
    }
    r.jain_index       = jains_index(x, N);
    r.starvation_count = starved_.load(std::memory_order_relaxed);
    return r;
}

void FairnessTracker::reset_counters() {
    for (int i = 0; i < N; i++) {
        submitted_[i].store(0);
        completed_[i].store(0);
    }
    starved_.store(0);
}
