#include "book_side.h"
#include <iostream>
#include <iterator>
#include <stdexcept>

BookSide::BookSide(Side side) : side_(side) {}

void BookSide::addOrder(std::shared_ptr<Order> order) {
    if (!order) throw std::invalid_argument("Null order");
    if (order -> getSide() != side_) throw std::invalid_argument("Order side doesn't match BookSide");

    Price price = order->getPrice();

    auto lvl_it = price_levels_.try_emplace(price).first;  // O(log L): find or create level
    auto& level = lvl_it->second;
    level.push_back(order);                                 // Back of the queue (FIFO)
    orders_[order->getOrderId()] = { lvl_it, std::prev(level.end()) };  // Store handle for O(1) cancel
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

    orders_.erase(level.front()->getOrderId());
    level.pop_front();

    if (level.empty()) {
        price_levels_.erase(lvl_it);    // erase by iterator: no second lookup
    }
}

bool BookSide::removeOrder(int64_t order_id) {
    auto it = orders_.find(order_id);       // O(1) avg: jump straight to the order
    if (it == orders_.end()) return false;

    auto lvl_it = it->second.level;
    lvl_it->second.erase(it->second.node);  // O(1): unlink node, no scan, no shifting
    orders_.erase(it);

    if (lvl_it->second.empty()) {
        price_levels_.erase(lvl_it);        // Amortized O(1): erase by iterator
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

// Find an order by ID, returns nullptr if not found. O(1) average.
// Filled orders are removed immediately (popBestOrder), so anything indexed is live.
std::shared_ptr<Order> BookSide::findOrder(int64_t order_id) const {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return nullptr;
    return *it->second.node;
}
