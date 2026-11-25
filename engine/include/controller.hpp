#pragma once

#include "scheduler.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>

struct ControllerConfig {
    PolicyType initial_policy = PolicyType::ROUND_ROBIN;
    size_t high_water = 500;
    size_t low_water = 100;
    double ewma_alpha = 0.3;
    int hold_ticks = 3;
};

class AdaptiveController {
public:
    explicit AdaptiveController(const ControllerConfig& cfg);

    void push(Order order);
    std::optional<Order> pop_wait();
    size_t queue_size() const;
    void tick(size_t current_queue_len);
    PolicyType current_policy() const { return current_policy_.load(); }
    int switch_count() const { return switch_count_.load(); }
    int64_t ms_since_switch() const;

private:
    void switch_to(PolicyType policy);

    ControllerConfig cfg_;
    mutable std::mutex active_mu_;
    std::shared_ptr<Scheduler> active_;
    std::atomic<PolicyType> current_policy_;
    std::atomic<int> switch_count_{0};
    double ewma_rate_ = 0.0;
    int ticks_above_ = 0;
    int ticks_below_ = 0;
    std::chrono::steady_clock::time_point last_switch_;
};
