#include "order.h"
#include <iostream>

Order::Order(int64_t order_id, 
             std::string symbol,
             std::string side, 
             double price, 
             int quantity, 
             int64_t timestamp)
    : order_id_(order_id), 
      symbol_(symbol),
      side_(side), 
      price_(price), 
      quantity_(quantity), 
      filled_(0),
      timestamp_(timestamp)  {}

int Order::getRemaining() const {
    return quantity_ - filled_;
}

void Order::fill(int qty) {
    filled_ += qty;
}

bool Order::isFilled() const {
    return filled_ >= quantity_;
}

void Order::print() const {
    std::cout << "Order " << order_id_ << ": " << symbol_ << " "
              << side_ << " " << quantity_ << " @ $" << price_
              << " (filled: " << filled_ << ")" << std::endl;
}