#include "trade.h"
#include <iostream>

void Trade::print() const {
    std::cout << "TRADE " << trade_id << ": "
              << symbol << " " << quantity << " @ $" << price
              << " (taker=" << taker_order_id
              << ", maker=" << maker_order_id
              << ", taker_side=" << to_string(taker_side)
              << ")"
              << std::endl;
}