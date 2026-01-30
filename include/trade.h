#ifndef TRADE_H
#define TRADE_H

#include <cstdint>
#include <string>
#include "order.h"

struct Trade {
    int64_t trade_id;
    std::string symbol;
    int64_t taker_order_id; //Keep immutability for orders
    int64_t maker_order_id;
    Side taker_side;
    double price;
    int quantity;
    int64_t timestamp;
    
    void print() const;
};

#endif // TRADE_H