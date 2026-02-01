#ifndef BOOK_SIDE_H
#define BOOK_SIDE_H

#include "order.h"
#include <map>
#include <deque>
#include <memory>
#include <optional>

// One side of the order book (either BUY or SELL)
class BookSide {
public:
    explicit BookSide(Side side); // "BUY" or "SELL"

    // Add order to this side
    void addOrder(std::shared_ptr<Order> order);

    // Get best price level
    std::optional<double> getBestPrice() const;

    // Get best order (front of the best price level)
    std::shared_ptr<Order> getBestOrder() const;

    // Remove order from book
    bool removeOrder(int64_t order_id);

    // Get total volumn at a price level
    int getVolumeAtPrice(double price) const;

    // Print book state 
    void print(int levels = 5) const;

    // Check if empty
    bool isEmpty() const { return price_levels_.empty(); }

private:
    Side side_;  // "BUY" or "SELL"

    // Price -> Queue of orders at that price (FIFO)
    // For BUY, use reverse iteration (highest price first)
    // For SELL, use normal iteration (lowest price first)
    std::map<double, std::deque<std::shared_ptr<Order>>> price_levels_;
    
    // Quick lookup: order_id -> (price, position in deque)
    std::map<int64_t, double> order_to_price_;
    
    // Helper: remove filled orders from front of level
    void cleanPriceLevel(double price);
};

#endif // BOOK_SIDE_H