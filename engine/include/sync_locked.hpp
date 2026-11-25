#pragma once

#include "order.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <atomic>
#include <chrono>

enum class SyncMode { LOCKED, LOCK_FREE };

class LockedQueue {
public:
    void push(Order order) {
        const auto started = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(mu_);
            record_wait(started);
            queue_.push(std::move(order));
        }
        cv_.notify_one();
    }

    std::optional<Order> try_pop() {
        const auto started = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mu_);
        record_wait(started);
        if (queue_.empty()) return std::nullopt;
        Order order = std::move(queue_.front());
        queue_.pop();
        return order;
    }
    struct LockStats { uint64_t contentions; double avg_wait_us; };
    LockStats take_lock_stats() { auto c = contentions_.exchange(0); auto w = wait_us_.exchange(0); return {c, c ? static_cast<double>(w) / c : 0.0}; }

private:
    std::mutex mu_;
    std::condition_variable cv_;
    std::queue<Order> queue_;
    std::atomic<uint64_t> contentions_{0}, wait_us_{0};
    void record_wait(std::chrono::steady_clock::time_point started) { auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count(); if (us > 0) { contentions_.fetch_add(1); wait_us_.fetch_add(us); } }
};
