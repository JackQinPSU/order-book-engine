#include "matching_engine.h"
#include <iostream>
#include <stdexcept>

std::vector<Trade> MatchingEngine::process(const Event& event) {
    switch (event.getType()) {
        case EventType::NEW_ORDER:
            return process(static_cast<const NewOrderEvent&>(event));
        case EventType::CANCEL_ORDER:
            return process(static_cast<const CancelOrderEvent&>(event));
        default:
            throw std::runtime_error("Unhandled event type in MatchingEngine::process");
    }
}

std::vector<Trade> MatchingEngine::process(const NewOrderEvent& e) {
    auto order = std::make_shared<Order>(
        e.getOrderId(),
        e.getSymbol(),
        e.getSide(),
        e.getPrice(),      // already fixed-point Price from event
        e.getQuantity(),
        e.getTimestamp()
    );

    OrderBook& book = getOrCreateBook(e.getSymbol());
    auto trades = book.addOrder(order);

    // Fire callback and accumulate stats
    for (const auto& t : trades) {
        ++total_trades_;
        if (trade_cb_) trade_cb_(t);
    }

    return trades;
}

std::vector<Trade> MatchingEngine::process(const CancelOrderEvent& e) {
    // We need to find which book owns this order_id.
    // Linear scan across books is acceptable here since cancel
    // volume is low relative to new orders. If this becomes a bottleneck,
    // add a global order_id -> symbol index.
    for (auto& [sym, book] : books_) {
        if (book->cancelOrder(e.getOrderId())) return {};
    }
    // Not found — silently ignore (order may have already been filled)
    return {};
}

OrderBook* MatchingEngine::getBook(const std::string& symbol) {
    auto it = books_.find(symbol);
    return (it != books_.end()) ? it->second.get() : nullptr;
}

void MatchingEngine::printAll(int levels) const {
    for (const auto& [sym, book] : books_) {
        book->print(levels);
        std::cout << "\n";
    }
}

OrderBook& MatchingEngine::getOrCreateBook(const std::string& symbol) {
    auto it = books_.find(symbol);
    if (it != books_.end()) return *it->second;

    auto [ins, ok] = books_.emplace(symbol, std::make_unique<OrderBook>(symbol));
    return *ins->second;
}