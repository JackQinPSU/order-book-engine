#include "order.h"
#include <iostream>

Order::Order(int id, std::string side, double price, int qty)
    : order_id(id), side(side), price(price), quantity(qty), filled(0) {
}

int Order::getRemaining() const {
    return quantity - filled;
}

void Order::fill(int qty) {
    filled += qty;
}

bool Order::isFilled() const {
    return filled >= quantity;
}

void Order::print() const {
    std::cout << "Order " << order_id << ": " 
              << side << " " << quantity << " @ $" << price 
              << " (filled: " << filled << ")" << std::endl;
}