#pragma once

#include "order.hpp"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

enum class SyncMode { LOCKED, LOCK_FREE };

class LockedQueue {
public:
    void push(Order order) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            queue_.push(std::move(order));
        }
        cv_.notify_one();
    }

    std::optional<Order> try_pop() {
        std::lock_guard<std::mutex> lock(mu_);
        if (queue_.empty()) return std::nullopt;
        Order order = std::move(queue_.front());
        queue_.pop();
        return order;
    }

private:
    std::mutex mu_;
    std::condition_variable cv_;
    std::queue<Order> queue_;
};
