#ifndef BOOK_SIDE_H
#define BOOK_SIDE_H

#include "order.h"
#include <map>
#include <list>
#include <unordered_map>
#include <memory>
#include <optional>

// One side of the order book (either BUY or SELL)
class BookSide {
public:
    explicit BookSide(Side side); // "BUY" or "SELL"

    // Add order to this side
    void addOrder(std::shared_ptr<Order> order);

    // Get best price level
    std::optional<Price> getBestPrice() const;

    // Get best order (front of the best price level), or nullptr if empty.
    // Filled orders are removed as soon as they fill (see popBestOrder), so the
    // front is always live and this is O(1) with no cleanup.
    std::shared_ptr<Order> getBestOrder();

    // Remove the best order (front of the best price level). The matching loop calls
    // this as soon as a maker is fully filled. Drops the level if it becomes empty.
    void popBestOrder();

    // Remove order from book. O(1) average: hash lookup + list erase
    // (+ amortized O(1) map erase-by-iterator if the level empties).
    bool removeOrder(int64_t order_id);

    // Get total volumn at a price level
    int getVolumeAtPrice(Price price) const;

    // Print book state 
    void print(int levels = 5) const;

    // Check if empty
    bool isEmpty() const { return price_levels_.empty(); }

    // Find Order by ID (returns nullptr if not found)
    std::shared_ptr<Order> findOrder(int64_t order_id) const;

private:
    Side side_;  // "BUY" or "SELL"

    // FIFO queue of orders at one price. std::list (not deque) so iterators stay
    // valid across inserts/erases elsewhere -> we can store a handle per order and
    // erase from the middle in O(1) on cancel.
    using OrderQueue = std::list<std::shared_ptr<Order>>;

    // Price -> queue of orders at that price.
    // For BUY, use reverse iteration (highest price first)
    // For SELL, use normal iteration (lowest price first)
    using LevelMap = std::map<Price, OrderQueue>;

    // Direct handle to a resting order: its level and its node in that level's queue.
    // Both iterators are stable (std::map and std::list never invalidate other elements).
    struct OrderHandle {
        LevelMap::iterator level;
        OrderQueue::iterator node;
    };

    LevelMap price_levels_;

    // order_id -> handle. O(1) average lookup for cancel / find.
    std::unordered_map<int64_t, OrderHandle> orders_;
};

#endif // BOOK_SIDE_H