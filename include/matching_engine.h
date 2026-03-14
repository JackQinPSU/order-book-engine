#ifndef MATCHING_ENGINE_H
#define MATCHING_ENGINE_H

#include "order_book.h"
#include "event.h"
#include "trade.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

class MatchingEngine {
public:
    MatchingEngine() = default;

    // Process a single event. Returns any trades produced.
    std::vector<Trade> process(const Event& event);

    // Convenience overloads
    std::vector<Trade> process(const NewOrderEvent& e);
    std::vector<Trade> process(const CancelOrderEvent& e);

    // Register a callback fired for every trade (e.g. for logging/market data)
    using TradeCallback = std::function<void(const Trade&)>;
    void onTrade(TradeCallback cb) { trade_cb_ = std::move(cb); }

    // Access a specific book (returns nullptr if symbol not found)
    OrderBook* getBook(const std::string& symbol);

    // Print all books
    void printAll(int levels = 5) const;

    // Stats
    size_t numSymbols() const { return books_.size(); }
    int64_t totalTrades() const { return total_trades_; }

private:
    // Lazily creates a book if symbol is new
    OrderBook& getOrCreateBook(const std::string& symbol);

    std::unordered_map<std::string, std::unique_ptr<OrderBook>> books_;
    std::unordered_map<int64_t, std::string> order_to_symbol_;  // O(1) cancel routing
    TradeCallback trade_cb_;
    int64_t total_trades_ = 0;
};

#endif // MATCHING_ENGINE_H