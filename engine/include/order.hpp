#pragma once

#include <chrono>
#include <cstdint>

enum class Side : uint8_t { BUY = 0, SELL = 1 };
enum class TraderType : uint8_t { HFT = 0, RETAIL = 1, MARKET_MAKER = 2 };

constexpr int NUM_TRADER_TYPES = 3;

struct Order {
    uint64_t id = 0;
    uint64_t sequence = 0;
    char symbol[8]{};
    Side side = Side::BUY;
    TraderType trader_type = TraderType::HFT;
    double price = 0.0;
    uint32_t quantity = 0;
    uint32_t service_time_us = 0;
    int priority = 0;
    std::chrono::steady_clock::time_point arrival_time{};
};

struct Fill {
    uint64_t buy_order_id = 0;
    uint64_t sell_order_id = 0;
    char symbol[8]{};
    double price = 0.0;
    uint32_t quantity = 0;
    bool partial = false;
    std::chrono::steady_clock::time_point fill_time{};
};

inline int64_t latency_us(const Order& order) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - order.arrival_time).count();
}
