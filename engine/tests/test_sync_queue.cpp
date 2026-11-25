#include "sync_lockfree.hpp"

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    LockFreeQueue queue;
    constexpr uint64_t producer_count = 2;
    constexpr uint64_t items_per_producer = 20000;
    constexpr uint64_t expected = producer_count * items_per_producer;
    std::atomic<uint64_t> consumed{0};
    std::atomic<uint64_t> sum{0};

    std::thread consumer([&] {
        while (consumed.load(std::memory_order_relaxed) < expected) {
            if (auto order = queue.try_pop()) {
                sum.fetch_add(order->id, std::memory_order_relaxed);
                consumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });

    std::vector<std::thread> producers;
    for (uint64_t producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([&, producer] {
            for (uint64_t i = 0; i < items_per_producer; ++i) {
                Order order{};
                order.id = producer * items_per_producer + i + 1;
                while (!queue.try_push(order)) std::this_thread::yield();
            }
        });
    }

    for (auto& producer : producers) producer.join();
    consumer.join();

    const uint64_t expected_sum = expected * (expected + 1) / 2;
    if (consumed.load() != expected || sum.load() != expected_sum) {
        std::cerr << "Lock-free queue lost or duplicated entries\n";
        return 1;
    }
    std::cout << "Lock-free queue concurrency: OK\n";
}
