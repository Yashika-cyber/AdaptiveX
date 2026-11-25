#pragma once
#include "order.hpp"
#include <map>
#include <queue>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

// Single-symbol order book with price-time priority matching.
class SymbolBook
{
public:
    // Returns list of fills produced.
    std::vector<Fill> add(Order order);

    // Snapshot for display: top N levels each side.
    struct Level
    {
        double price;
        uint32_t total_qty;
        size_t order_count;
    };
    std::vector<Level> bid_levels(int n = 5) const;
    std::vector<Level> ask_levels(int n = 5) const;

    size_t bid_count() const;
    size_t ask_count() const;

private:
    // Bid side: descending price (highest first).
    std::map<double, std::queue<Order>, std::greater<double>> bids_;
    // Ask side: ascending price (lowest first).
    std::map<double, std::queue<Order>> asks_;

    std::vector<Fill> try_match();
};

// Multi-symbol book with per-symbol mutex.
class OrderBook
{
public:
    // Returns fills; thread-safe per symbol.
    std::vector<Fill> match(Order order);

    // Snapshot
    SymbolBook::Level best_bid(const std::string &sym) const;
    SymbolBook::Level best_ask(const std::string &sym) const;
    std::vector<SymbolBook::Level> bid_levels(const std::string &sym, int n = 5) const;
    std::vector<SymbolBook::Level> ask_levels(const std::string &sym, int n = 5) const;

    std::vector<std::string> active_symbols() const;

private:
    struct BookEntry
    {
        mutable std::mutex mu;
        SymbolBook book;
    };
    mutable std::mutex registry_mu_;
    std::unordered_map<std::string, std::unique_ptr<BookEntry>> books_;

    BookEntry &get_or_create(const std::string &sym);
};
