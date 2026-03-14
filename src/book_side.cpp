#include "book_side.h"
#include <iostream>
#include <algorithm>
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

// Walk price levels from best to worst, cleaning each one before inspecting it.
// Because cleanPriceLevel now removes ALL filled orders, if a level survives
// the clean its front() is guaranteed to be an unfilled order — no scan needed.
std::shared_ptr<Order> BookSide::getBestOrder() {
    while (true) {
        auto best_price = getBestPrice();
        if (!best_price) return nullptr;

        cleanPriceLevel(*best_price);

        auto after_clean = getBestPrice();
        if (!after_clean) return nullptr;

        if (*after_clean == *best_price) {
            // Level survived cleanup: front is guaranteed unfilled
            return price_levels_.at(*best_price).front();
        }
        // Level was entirely filled and erased; loop to examine the next best level
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

void BookSide::cleanPriceLevel(Price price) {
    auto it = price_levels_.find(price);
    if (it == price_levels_.end()) return;

    auto& level = it->second;

    // remove ALL filled orders, not just the front, so stale entries don't
    // accumulate in the deque or in order_to_price_ between matching rounds
    auto new_end = std::remove_if(level.begin(), level.end(),
        [this](const std::shared_ptr<Order>& o) {
            if (o && o->isFilled()) {
                order_to_price_.erase(o->getOrderId());
                return true;
            }
            return false;
        });
    level.erase(new_end, level.end());

    if (level.empty()) {
        price_levels_.erase(it);
    }
}