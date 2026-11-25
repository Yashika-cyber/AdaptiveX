#include "order_book.hpp"
#include <cstring>

// ─────────────────────────── SymbolBook ───────────────────────
std::vector<Fill> SymbolBook::add(Order order) {
    if (order.side == Side::BUY)
        bids_[order.price].push(order);
    else
        asks_[order.price].push(order);

    return try_match();
}

std::vector<Fill> SymbolBook::try_match() {
    std::vector<Fill> fills;

    while (!bids_.empty() && !asks_.empty()) {
        auto& [best_bid_price, bid_q] = *bids_.begin();
        auto& [best_ask_price, ask_q] = *asks_.begin();

        if (best_bid_price < best_ask_price) break;  // no cross

        // Aggressive match: execute at the resting order's price.
        Order& bid = bid_q.front();
        Order& ask = ask_q.front();

        uint32_t fill_qty = std::min(bid.quantity, ask.quantity);
        double   fill_px  = ask.price;  // resting order's price

        Fill f{};
        f.buy_order_id  = bid.id;
        f.sell_order_id = ask.id;
        std::strncpy(f.symbol, bid.symbol, 8);
        f.price    = fill_px;
        f.quantity = fill_qty;
        f.fill_time = std::chrono::steady_clock::now();
        fills.push_back(f);

        bid.quantity -= fill_qty;
        ask.quantity -= fill_qty;

        if (bid.quantity == 0) {
            bid_q.pop();
            if (bid_q.empty()) bids_.erase(best_bid_price);
        }
        if (ask.quantity == 0) {
            ask_q.pop();
            if (ask_q.empty()) asks_.erase(best_ask_price);
        }
    }
    return fills;
}

std::vector<SymbolBook::Level> SymbolBook::bid_levels(int n) const {
    std::vector<Level> out;
    for (auto it = bids_.begin(); it != bids_.end() && static_cast<int>(out.size()) < n; ++it) {
        uint32_t total = 0;
        auto orders = it->second;
        while (!orders.empty()) {
            total += orders.front().quantity;
            orders.pop();
        }
        out.push_back({it->first, total, it->second.size()});
    }
    return out;
}

std::vector<SymbolBook::Level> SymbolBook::ask_levels(int n) const {
    std::vector<Level> out;
    for (auto it = asks_.begin(); it != asks_.end() && static_cast<int>(out.size()) < n; ++it) {
        uint32_t total = 0;
        auto orders = it->second;
        while (!orders.empty()) {
            total += orders.front().quantity;
            orders.pop();
        }
        out.push_back({it->first, total, it->second.size()});
    }
    return out;
}

size_t SymbolBook::bid_count() const { return bids_.size(); }
size_t SymbolBook::ask_count() const { return asks_.size(); }

// ─────────────────────────── OrderBook ────────────────────────
OrderBook::BookEntry& OrderBook::get_or_create(const std::string& sym) {
    std::lock_guard<std::mutex> lk(registry_mu_);
    auto& ptr = books_[sym];
    if (!ptr) ptr = std::make_unique<BookEntry>();
    return *ptr;
}

std::vector<Fill> OrderBook::match(Order order) {
    std::string sym(order.symbol);
    auto& entry = get_or_create(sym);
    std::lock_guard<std::mutex> lk(entry.mu);
    return entry.book.add(order);
}

std::vector<SymbolBook::Level> OrderBook::bid_levels(const std::string& sym, int n) const {
    std::lock_guard<std::mutex> lk(registry_mu_);
    auto it = books_.find(sym);
    if (it == books_.end()) return {};
    std::lock_guard<std::mutex> blk(it->second->mu);
    return it->second->book.bid_levels(n);
}

std::vector<SymbolBook::Level> OrderBook::ask_levels(const std::string& sym, int n) const {
    std::lock_guard<std::mutex> lk(registry_mu_);
    auto it = books_.find(sym);
    if (it == books_.end()) return {};
    std::lock_guard<std::mutex> blk(it->second->mu);
    return it->second->book.ask_levels(n);
}

SymbolBook::Level OrderBook::best_bid(const std::string& sym) const {
    const auto levels = bid_levels(sym, 1);
    return levels.empty() ? SymbolBook::Level{} : levels.front();
}

SymbolBook::Level OrderBook::best_ask(const std::string& sym) const {
    const auto levels = ask_levels(sym, 1);
    return levels.empty() ? SymbolBook::Level{} : levels.front();
}

std::vector<std::string> OrderBook::active_symbols() const {
    std::lock_guard<std::mutex> lk(registry_mu_);
    std::vector<std::string> out;
    for (auto& kv : books_) out.push_back(kv.first);
    return out;
}
