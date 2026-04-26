#include "order.h"
#include "types.h"
#include <iostream>

Order::Order(int64_t order_id,
             std::string symbol,
             Side side,
             Price price,
             int quantity,
             int64_t timestamp,
             OrderType order_type)
    : order_id_(order_id),
      symbol_(std::move(symbol)),
      side_(side),
      price_(price),
      quantity_(quantity),
      filled_(0),
      timestamp_(timestamp),
      order_type_(order_type) {}

void Order::fill(int qty) {
    filled_ += qty;
}

bool Order::resize(int new_qty) {
    if (new_qty <= filled_) return false;
    quantity_ = new_qty;
    return true;
}


void Order::print() const {
    std::cout << "Order " << order_id_ << ": " << symbol_ << " "
              << to_string(side_) << " " << quantity_ << " @ $" << price_
              << " (filled: " << filled_ << ")" << std::endl;
}