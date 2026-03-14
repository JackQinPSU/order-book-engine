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

    // Track resting orders so cancel can find the right book in O(1).
    // Orders that were fully matched immediately are never rested, so skip them.
    if (!order->isFilled()) {
        order_to_symbol_[order->getOrderId()] = e.getSymbol();
    }

    // Fire callback and accumulate stats
    for (const auto& t : trades) {
        ++total_trades_;
        if (trade_cb_) trade_cb_(t);
    }

    return trades;
}

std::vector<Trade> MatchingEngine::process(const CancelOrderEvent& e) {
    auto it = order_to_symbol_.find(e.getOrderId());
    if (it == order_to_symbol_.end()) return {};  // unknown or already filled

    const std::string& sym = it->second;
    order_to_symbol_.erase(it);  // remove regardless — order is leaving the book

    if (auto* book = getBook(sym)) {
        book->cancelOrder(e.getOrderId());
    }
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