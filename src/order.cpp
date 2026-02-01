#include "order.h"
#include "types.h"
#include <iostream>

Order::Order(int64_t order_id, 
             std::string symbol,
             Side side, 
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

void Order::fill(int qty) {
    filled_ += qty;
}


void Order::print() const {
    std::cout << "Order " << order_id_ << ": " << symbol_ << " "
              << to_string(side_) << " " << quantity_ << " @ $" << price_
              << " (filled: " << filled_ << ")" << std::endl;
}