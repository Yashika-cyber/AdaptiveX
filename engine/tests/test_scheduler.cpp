// Unit tests for all three schedulers.
// Build with test_engine target. Run: ./test_engine
#include "scheduler.hpp"
#include "fairness.hpp"
#include "metrics.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

static int passed = 0, failed = 0;
#define CHECK(cond) do { \
    if (cond) { ++passed; } \
    else { ++failed; std::cerr << "FAIL: " << #cond << " at line " << __LINE__ << "\n"; } \
} while(0)

static Order make(uint64_t id, int prio, uint32_t svc, uint64_t seq) {
    Order o{};
    o.id = id;
    o.priority = prio;
    o.service_time_us = svc;
    o.sequence = seq;
    o.arrival_time = std::chrono::steady_clock::now();
    return o;
}

void test_priority_order() {
    PriorityScheduler s;
    s.push(make(1, 1, 50, 0));  // low prio
    s.push(make(2, 3, 50, 1));  // high prio
    s.push(make(3, 2, 50, 2));  // medium prio

    auto a = s.pop(); CHECK(a && a->id == 2);
    auto b = s.pop(); CHECK(b && b->id == 3);
    auto c = s.pop(); CHECK(c && c->id == 1);
    auto d = s.pop(); CHECK(!d);  // empty
    std::cout << "PriorityScheduler order: OK\n";
}

void test_sjf_order() {
    SJFScheduler s;
    s.push(make(1, 0, 100, 0));  // slowest
    s.push(make(2, 0,  10, 1));  // fastest
    s.push(make(3, 0,  50, 2));  // medium

    auto a = s.pop(); CHECK(a && a->id == 2);  // svc=10
    auto b = s.pop(); CHECK(b && b->id == 3);  // svc=50
    auto c = s.pop(); CHECK(c && c->id == 1);  // svc=100
    std::cout << "SJFScheduler order: OK\n";
}

void test_rr_order() {
    RoundRobinScheduler s;
    for (int i = 1; i <= 5; i++) s.push(make(i, 0, 10, i));
    for (int i = 1; i <= 5; i++) {
        auto o = s.pop();
        CHECK(o && o->id == static_cast<uint64_t>(i));
    }
    CHECK(!s.pop());
    std::cout << "RoundRobinScheduler FIFO: OK\n";
}

void test_concurrent_push_pop() {
    // 4 producers, 2 consumers; count must balance.
    RoundRobinScheduler s;
    std::atomic<int> produced{0}, consumed{0};
    std::atomic<bool> done{false};

    std::vector<std::thread> prod, cons;
    for (int i = 0; i < 4; i++) {
        prod.emplace_back([&, i](){
            for (int j = 0; j < 500; j++) {
                s.push(make(i * 1000 + j, 0, 1, 0));
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (int i = 0; i < 2; i++) {
        cons.emplace_back([&](){
            while (!done.load() || s.size() > 0) {
                if (s.pop()) consumed.fetch_add(1, std::memory_order_relaxed);
                else std::this_thread::yield();
            }
        });
    }
    for (auto& t : prod) t.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    done.store(true);
    for (auto& t : cons) t.join();

    CHECK(produced.load() == 2000);
    CHECK(consumed.load() == 2000);
    std::cout << "Concurrent push/pop: produced=" << produced.load()
              << " consumed=" << consumed.load() << "\n";
}

void test_drain() {
    SJFScheduler s;
    for (int i = 0; i < 100; i++) s.push(make(i, 0, i, i));
    CHECK(s.size() == 100);
    std::vector<Order> out;
    s.drain_into(out);
    CHECK(out.size() == 100);
    CHECK(s.size() == 0);
    std::cout << "drain_into: OK\n";
}

int main() {
    std::cout << "=== AdaptiveX Scheduler Tests ===\n";
    test_priority_order();
    test_sjf_order();
    test_rr_order();
    test_concurrent_push_pop();
    test_drain();

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
