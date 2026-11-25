#include "controller.hpp"
#include <cstdio>

AdaptiveController::AdaptiveController(const ControllerConfig& cfg)
    : cfg_(cfg)
    , active_(make_scheduler(cfg.initial_policy))
    , current_policy_(cfg.initial_policy)
    , last_switch_(std::chrono::steady_clock::now())
{}

void AdaptiveController::push(Order order) {
    std::lock_guard<std::mutex> lock(active_mu_);
    active_->push(std::move(order));
}

std::optional<Order> AdaptiveController::pop_wait() {
    std::shared_ptr<Scheduler> scheduler;
    {
        std::lock_guard<std::mutex> lock(active_mu_);
        scheduler = active_;
    }
    return scheduler->pop_wait();
}

size_t AdaptiveController::queue_size() const {
    std::lock_guard<std::mutex> lock(active_mu_);
    return active_->size();
}

void AdaptiveController::tick(size_t current_queue_len) {
    std::lock_guard<std::mutex> lock(active_mu_);
    // Update EWMA of queue depth (use as proxy for arrival rate)
    ewma_rate_ = cfg_.ewma_alpha * static_cast<double>(current_queue_len)
               + (1.0 - cfg_.ewma_alpha) * ewma_rate_;

    PolicyType cur = current_policy_.load();

    if (ewma_rate_ > static_cast<double>(cfg_.high_water)) {
        ++ticks_above_;
        ticks_below_ = 0;
        if (ticks_above_ >= cfg_.hold_ticks && cur != PolicyType::SJF) {
            fprintf(stderr, "[ctrl] queue=%.0f → switching to SJF\n", ewma_rate_);
            switch_to(PolicyType::SJF);
        }
    } else if (ewma_rate_ < static_cast<double>(cfg_.low_water)) {
        ++ticks_below_;
        ticks_above_ = 0;
        if (ticks_below_ >= cfg_.hold_ticks && cur == PolicyType::SJF) {
            fprintf(stderr, "[ctrl] queue=%.0f → switching to RR\n", ewma_rate_);
            switch_to(PolicyType::ROUND_ROBIN);
        }
    } else {
        ticks_above_ = ticks_below_ = 0;
    }

}

void AdaptiveController::switch_to(PolicyType p) {
    // Drain existing queue into new scheduler to avoid losing orders.
    auto new_sched = std::shared_ptr<Scheduler>(make_scheduler(p));
    std::vector<Order> pending;
    active_->drain_into(pending);
    for (auto& o : pending) new_sched->push(std::move(o));

    active_ = std::move(new_sched);
    current_policy_.store(p);
    ++switch_count_;
    last_switch_ = std::chrono::steady_clock::now();
}

int64_t AdaptiveController::ms_since_switch() const {
    std::lock_guard<std::mutex> lock(active_mu_);
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_switch_).count();
}
