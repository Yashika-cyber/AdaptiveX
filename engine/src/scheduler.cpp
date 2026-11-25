#include "scheduler.hpp"
#include <chrono>

// ─────────────────────────── PriorityScheduler ────────────────
void PriorityScheduler::push(Order order) {
    std::unique_lock<std::mutex> lk(mu_);
    pq_.push(std::move(order));
    lk.unlock();
    cv_.notify_one();
}

std::optional<Order> PriorityScheduler::pop() {
    std::lock_guard<std::mutex> lk(mu_);
    if (pq_.empty()) return std::nullopt;
    Order o = pq_.top();
    pq_.pop();
    return o;
}

std::optional<Order> PriorityScheduler::pop_wait() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait_for(lk, std::chrono::milliseconds(1), [this]{ return !pq_.empty(); });
    if (pq_.empty()) return std::nullopt;
    Order o = pq_.top();
    pq_.pop();
    return o;
}

size_t PriorityScheduler::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return pq_.size();
}

void PriorityScheduler::drain_into(std::vector<Order>& out) {
    std::lock_guard<std::mutex> lk(mu_);
    while (!pq_.empty()) {
        out.push_back(pq_.top());
        pq_.pop();
    }
}

// ─────────────────────────── SJFScheduler ─────────────────────
void SJFScheduler::push(Order order) {
    std::unique_lock<std::mutex> lk(mu_);
    pq_.push(std::move(order));
    lk.unlock();
    cv_.notify_one();
}

std::optional<Order> SJFScheduler::pop() {
    std::lock_guard<std::mutex> lk(mu_);
    if (pq_.empty()) return std::nullopt;
    Order o = pq_.top();
    pq_.pop();
    return o;
}

std::optional<Order> SJFScheduler::pop_wait() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait_for(lk, std::chrono::milliseconds(1), [this]{ return !pq_.empty(); });
    if (pq_.empty()) return std::nullopt;
    Order o = pq_.top();
    pq_.pop();
    return o;
}

size_t SJFScheduler::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return pq_.size();
}

void SJFScheduler::drain_into(std::vector<Order>& out) {
    std::lock_guard<std::mutex> lk(mu_);
    while (!pq_.empty()) {
        out.push_back(pq_.top());
        pq_.pop();
    }
}

// ─────────────────────────── RoundRobinScheduler ──────────────
void RoundRobinScheduler::push(Order order) {
    std::unique_lock<std::mutex> lk(mu_);
    q_.push_back(std::move(order));
    lk.unlock();
    cv_.notify_one();
}

std::optional<Order> RoundRobinScheduler::pop() {
    std::lock_guard<std::mutex> lk(mu_);
    if (q_.empty()) return std::nullopt;
    Order o = std::move(q_.front());
    q_.pop_front();
    return o;
}

std::optional<Order> RoundRobinScheduler::pop_wait() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait_for(lk, std::chrono::milliseconds(1), [this]{ return !q_.empty(); });
    if (q_.empty()) return std::nullopt;
    Order o = std::move(q_.front());
    q_.pop_front();
    return o;
}

size_t RoundRobinScheduler::size() const {
    std::lock_guard<std::mutex> lk(mu_);
    return q_.size();
}

void RoundRobinScheduler::drain_into(std::vector<Order>& out) {
    std::lock_guard<std::mutex> lk(mu_);
    while (!q_.empty()) {
        out.push_back(std::move(q_.front()));
        q_.pop_front();
    }
}

// ─────────────────────────── Factory ──────────────────────────
std::unique_ptr<Scheduler> make_scheduler(PolicyType p) {
    switch (p) {
        case PolicyType::PRIORITY:    return std::make_unique<PriorityScheduler>();
        case PolicyType::SJF:         return std::make_unique<SJFScheduler>();
        case PolicyType::ROUND_ROBIN: return std::make_unique<RoundRobinScheduler>();
        default:                      return std::make_unique<RoundRobinScheduler>();
    }
}
