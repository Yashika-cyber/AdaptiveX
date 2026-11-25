#pragma once

#include "order.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

template <size_t Capacity>
class BoundedLockFreeQueue {
    static_assert(Capacity > 1 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two greater than one");

    struct Cell {
        std::atomic<size_t> sequence{0};
        Order value{};
    };

public:
    BoundedLockFreeQueue() {
        for (size_t i = 0; i < Capacity; ++i)
            cells_[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool try_push(const Order& value) {
        size_t pos = enqueue_.load(std::memory_order_relaxed);
        Cell* cell;
        for (;;) {
            cell = &cells_[pos & (Capacity - 1)];
            const size_t seq = cell->sequence.load(std::memory_order_acquire);
            const auto diff = static_cast<std::intptr_t>(seq) -
                              static_cast<std::intptr_t>(pos);
            if (diff == 0) {
                if (enqueue_.compare_exchange_weak(pos, pos + 1,
                                                   std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                return false;
            } else {
                pos = enqueue_.load(std::memory_order_relaxed);
            }
        }
        cell->value = value;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    std::optional<Order> try_pop() {
        size_t pos = dequeue_.load(std::memory_order_relaxed);
        Cell* cell;
        for (;;) {
            cell = &cells_[pos & (Capacity - 1)];
            const size_t seq = cell->sequence.load(std::memory_order_acquire);
            const auto diff = static_cast<std::intptr_t>(seq) -
                              static_cast<std::intptr_t>(pos + 1);
            if (diff == 0) {
                if (dequeue_.compare_exchange_weak(pos, pos + 1,
                                                   std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                return std::nullopt;
            } else {
                pos = dequeue_.load(std::memory_order_relaxed);
            }
        }
        Order value = cell->value;
        cell->sequence.store(pos + Capacity, std::memory_order_release);
        return value;
    }

private:
    std::array<Cell, Capacity> cells_{};
    alignas(64) std::atomic<size_t> enqueue_{0};
    alignas(64) std::atomic<size_t> dequeue_{0};
};

using LockFreeQueue = BoundedLockFreeQueue<65536>;
