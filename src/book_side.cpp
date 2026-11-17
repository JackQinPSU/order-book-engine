#include "book_side.h"
#include <iostream>
#include <algorithm>
#include <iterator>
#include <stdexcept>

BookSide::BookSide(Side side) : side_(side) {}

void BookSide::addOrder(std::shared_ptr<Order> order) {
    if (!order) throw std::invalid_argument("Null order");
    if (order -> getSide() != side_) throw std::invalid_argument("Order side doesn't match BookSide");

    Price price = order->getPrice();

    price_levels_[price].push_back(order);  // Add to the back of the deque (FIFO)
    order_to_price_[order->getOrderId()] = price;   // Map order_id to price, for quick lookup
}

std::optional<Price> BookSide::getBestPrice() const {  // Optional: returns nullopt if empty
    if (price_levels_.empty()) return std::nullopt;
    
    if (side_ == Side::BUY) {
        return price_levels_.rbegin()->first;  // Highest price
    } else {
        return price_levels_.begin()->first;   // Lowest price
    }
}

// Levels are never empty (empty ones are erased) and never hold filled orders
// (makers are popped the moment they fill), so the best order is just the front
// of the best level. O(1): map begin()/rbegin() are cached by std::map.
std::shared_ptr<Order> BookSide::getBestOrder() {
    if (price_levels_.empty()) return nullptr;

    const auto& level = (side_ == Side::BUY) ? price_levels_.rbegin()->second
                                             : price_levels_.begin()->second;
    return level.front();
}

void BookSide::popBestOrder() {
    if (price_levels_.empty()) return;

    // Best level: highest price for bids, lowest for asks.
    auto lvl_it = (side_ == Side::BUY) ? std::prev(price_levels_.end())
                                       : price_levels_.begin();
    auto& level = lvl_it->second;

    order_to_price_.erase(level.front()->getOrderId());
    level.pop_front();

    if (level.empty()) {
        price_levels_.erase(lvl_it);    // erase by iterator: no second lookup
    }
}

bool BookSide::removeOrder(int64_t order_id) {
    auto it = order_to_price_.find(order_id);
    if (it == order_to_price_.end()) return false;  // Find order_id w/o having to search all levels

    Price price = it->second;

    auto lvl_it = price_levels_.find(price);
    if (lvl_it == price_levels_.end()) {
        order_to_price_.erase(order_id);
        return false;
    }

    auto& level = lvl_it->second;

    auto order_it = std::find_if(level.begin(), level.end(),
        [order_id](const std::shared_ptr<Order>& o) {   // Scan only this price level
            return o && o->getOrderId() == order_id;
        });

    if (order_it == level.end()) return false;

    level.erase(order_it);
    order_to_price_.erase(order_id);

    if (level.empty()) {
        price_levels_.erase(lvl_it);    // Remove empty price level if deque is empty
    }

    return true;
}

int BookSide::getVolumeAtPrice(Price price) const {
    auto it = price_levels_.find(price);
    if (it == price_levels_.end()) return 0;

    int total = 0;
    for (const auto& order : it->second) {
        if (order && !order->isFilled()) {
            total += order->getRemaining();
        }
    }
    return total;
}

// Print top N levels of the book side
void BookSide::print(int levels) const {
    std::cout << ((side_ == Side::BUY) ? "BUY" : "SELL") << " SIDE:\n";
    int count = 0;

    auto print_level = [&](Price px) {
        std::cout << "  $" << to_double(px) << " - "
                  << getVolumeAtPrice(px) << " shares\n";
    };

    if (side_ == Side::BUY) {
        for (auto it = price_levels_.rbegin();
             it != price_levels_.rend() && count < levels; ++it, ++count)
            print_level(it->first);
    } else {
        for (auto it = price_levels_.begin();
             it != price_levels_.end() && count < levels; ++it, ++count)
            print_level(it->first);
    }
}

// New method to find an order by ID, returns nullptr if not found or already filled
std::shared_ptr<Order> BookSide::findOrder(int64_t order_id) const {
    auto it = order_to_price_.find(order_id);                   
    if (it == order_to_price_.end()) return nullptr;                        // O(1) lookup to find the price level of the order

    Price price = it->second;
    auto lvl_it = price_levels_.find(price);                                
    if (lvl_it == price_levels_.end()) return nullptr;

    const auto& level = lvl_it->second;

    auto order_it = std::find_if(level.begin(), level.end(),                // Scan only this price level for the order ID, and check if it's not filled
        [order_id](const std::shared_ptr<Order>& o) {
            return o && o->getOrderId() == order_id && !o->isFilled();
        });

    if (order_it == level.end()) return nullptr;

    return *order_it;
}