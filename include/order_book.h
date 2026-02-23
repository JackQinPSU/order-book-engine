#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include "book_side.h"
#include "order.h"
#include "trade.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class OrderBook {
public:
    explicit OrderBook(std::string symbol);

    // Add a new order to this symbol's book. Matches first, rests remainder.
    std::vector<Trade> addOrder(const std::shared_ptr<Order>& order);

    // Cancel by id (only works for orders currently resting in this book)
    bool cancelOrder(int64_t order_id);

    const std::string& getSymbol() const { return symbol_; }

    // Best orders (may be nullptr if that side is empty, or needs cleaning)
    std::shared_ptr<Order> getBestBid();
    std::shared_ptr<Order> getBestAsk();

    // Spread (returns 0.0 if one side missing)
    double getSpread();

    // Print both sides
    void print(int levels = 5) const;

private:
    std::string symbol_;
    BookSide bids_;
    BookSide asks_;

    // For cancel routing inside this symbol book
    std::unordered_map<int64_t, Side> order_side_; // order_id -> Side::BUY or Side::SELL

    // Per-book trade counter (optional; you can also move this to MatchingEngine)
    int64_t trade_counter_;

    // Core matching loop: incoming vs opposite
    std::vector<Trade> matchIncoming(const std::shared_ptr<Order>& incoming, BookSide& opposite);

    // Trade record helper
    Trade makeTrade(const std::shared_ptr<Order>& buy,
                    const std::shared_ptr<Order>& sell,
                    double price,
                    int qty);
};

#endif