#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include "types.h"

struct PriceLevel {
    Price price;
    int   quantity;
    int   order_count;
};

struct L2Snapshot {
    std::string            symbol;
    std::vector<PriceLevel> bids;  // best first: highest price first
    std::vector<PriceLevel> asks;  // best first: lowest price first
    int64_t                timestamp_ns;

    void print() const {
        std::cout << "=== L2 SNAPSHOT: " << symbol << " ===\n";
        std::cout << "  ASKS:\n";
        for (auto it = asks.rbegin(); it != asks.rend(); ++it)
            std::cout << "    $" << to_double(it->price)
                      << "  qty=" << it->quantity
                      << "  orders=" << it->order_count << "\n";
        std::cout << "  ---- spread ----\n";
        std::cout << "  BIDS:\n";
        for (const auto& lvl : bids)
            std::cout << "    $" << to_double(lvl.price)
                      << "  qty=" << lvl.quantity
                      << "  orders=" << lvl.order_count << "\n";
    }
};
