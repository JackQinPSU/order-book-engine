#include "book_side.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>

BookSide::BookSide(Side side) : side_(side) {}

void BookSide::addOrder(std::shared_ptr<Order> order) {
    if (!order) {
        throw std::invalid_argument("Null order");
    }
    if (order -> getSide() != side_) {
        throw std::invalid_argument("Order side doesn't match BookSide");
    }

    double price = order->getPrice();

    price_levels_[price].push_back(order);  // Add to the back of the deque (FIFO)
    order_to_price_[order->getOrderId()] = price;   // Map order_id to price, for quick lookup
}

std::optional<double> BookSide::getBestPrice() const {
    if (price_levels_.empty()) {
        return std::nullopt;
    }

    if (side_ == Side::BUY) {
        return price_levels_.rbegin()->first;  // Highest price
    } else {
        return price_levels_.begin()->first;   // Lowest price
    }
}

// skips filled orders but does not remove them
std::shared_ptr<Order> BookSide::getBestOrder() const {
    auto best_price = getBestPrice();
    if (!best_price) return nullptr;

    const auto& level = price_levels_.at(*best_price);  // get copy of deque, read w/o modifying

    for (const auto& order : level) {
        if (order && !order->isFilled()) {
            return order;
        }
    }
    return nullptr;  // If all orders at this level are filled
}

bool BookSide::removeOrder(int64_t order_id) {
    auto it = order_to_price_.find(order_id);
    if (it == order_to_price_.end()) return false;  // Find order_id w/o having to search all levels

    double price = it->second;

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

int BookSide::getVolumeAtPrice(double price) const {
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
    auto side_str = (side_ == Side::BUY) ? "BUY" : "SELL";
    std::cout << side_str << " SIDE:\n";

    int count = 0;

    if (side_ == Side::BUY) {
        for (auto it = price_levels_.rbegin();
             it != price_levels_.rend() && count < levels;  // reverse iterator for BUY side， print whole list if length less than levels
             ++it, ++count) {
            std::cout << "  $" << it->first << " - "
                      << getVolumeAtPrice(it->first) << " shares\n";
        }
    } else {
        for (auto it = price_levels_.begin();
             it != price_levels_.end() && count < levels;
             ++it, ++count) {
            std::cout << "  $" << it->first << " - "
                      << getVolumeAtPrice(it->first) << " shares\n";
        }
    }
}

void BookSide::cleanPriceLevel(double price) {
    auto it = price_levels_.find(price);
    if (it == price_levels_.end()) return;

    auto& level = it->second;

    // remove filled orders from front (FIFO cleanup)
    while (!level.empty() && level.front() && level.front()->isFilled()) {
        order_to_price_.erase(level.front()->getOrderId());
        level.pop_front();
    }

    if (level.empty()) {
        price_levels_.erase(it);
    }
}