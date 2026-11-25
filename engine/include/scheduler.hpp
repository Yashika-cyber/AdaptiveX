#pragma once

#include "order.hpp"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <vector>

enum class PolicyType { PRIORITY, SJF, ROUND_ROBIN };

const char* policy_name(PolicyType policy);

class Scheduler {
public:
    virtual ~Scheduler() = default;
    virtual void push(Order order) = 0;
    virtual std::optional<Order> pop() = 0;
    virtual std::optional<Order> pop_wait() = 0;
    virtual size_t size() const = 0;
    virtual void drain_into(std::vector<Order>& out) = 0;
};

class PriorityScheduler final : public Scheduler {
public:
    void push(Order order) override;
    std::optional<Order> pop() override;
    std::optional<Order> pop_wait() override;
    size_t size() const override;
    void drain_into(std::vector<Order>& out) override;

private:
    struct LowerPriority {
        bool operator()(const Order& lhs, const Order& rhs) const {
            if (lhs.priority != rhs.priority) return lhs.priority < rhs.priority;
            return lhs.sequence > rhs.sequence;
        }
    };
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::priority_queue<Order, std::vector<Order>, LowerPriority> pq_;
};

class SJFScheduler final : public Scheduler {
public:
    void push(Order order) override;
    std::optional<Order> pop() override;
    std::optional<Order> pop_wait() override;
    size_t size() const override;
    void drain_into(std::vector<Order>& out) override;

private:
    struct LongerService {
        bool operator()(const Order& lhs, const Order& rhs) const {
            if (lhs.service_time_us != rhs.service_time_us)
                return lhs.service_time_us > rhs.service_time_us;
            return lhs.sequence > rhs.sequence;
        }
    };
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::priority_queue<Order, std::vector<Order>, LongerService> pq_;
};

class RoundRobinScheduler final : public Scheduler {
public:
    void push(Order order) override;
    std::optional<Order> pop() override;
    std::optional<Order> pop_wait() override;
    size_t size() const override;
    void drain_into(std::vector<Order>& out) override;

private:
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Order> q_;
};

std::unique_ptr<Scheduler> make_scheduler(PolicyType policy);
