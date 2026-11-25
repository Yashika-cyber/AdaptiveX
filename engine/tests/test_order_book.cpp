#include "order_book.hpp"

#include <iostream>
#include <cstring>

static Order make_order(uint64_t id, Side side, double price, uint32_t quantity) {
    Order order{};
    order.id = id;
    std::strncpy(order.symbol, "AAPL", sizeof(order.symbol) - 1);
    order.side = side;
    order.price = price;
    order.quantity = quantity;
    order.arrival_time = std::chrono::steady_clock::now();
    return order;
}

int main() {
    OrderBook book;
    book.match(make_order(1, Side::BUY, 101.0, 10));

    const auto bid = book.best_bid("AAPL");
    if (bid.price != 101.0 || bid.total_qty != 10 || bid.order_count != 1) {
        std::cerr << "Best bid snapshot is incorrect\n";
        return 1;
    }

    const auto fills = book.match(make_order(2, Side::SELL, 100.0, 4));
    if (fills.size() != 1 || fills.front().quantity != 4 ||
        fills.front().buy_order_id != 1 || fills.front().sell_order_id != 2) {
        std::cerr << "Crossed orders did not produce the expected fill\n";
        return 1;
    }

    const auto remaining_bid = book.best_bid("AAPL");
    if (remaining_bid.total_qty != 6 || remaining_bid.order_count != 1) {
        std::cerr << "Partial fill was not reflected in the book snapshot\n";
        return 1;
    }

    std::cout << "Order-book matching and snapshot: OK\n";
}
