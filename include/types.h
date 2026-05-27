#pragma once
#include <cstdint>

enum class Side : uint8_t {
    BUY,
    SELL
};

inline const char* to_string(Side s) {
    return (s == Side::BUY) ? "BUY" : "SELL";
}

enum class OrderType : uint8_t {
    LIMIT,   // Rest unfilled remainder on the book
    MARKET,  // Match at any price; remainder is dropped
    IOC      // Immediate-or-Cancel: match at limit or better; remainder is dropped
};

inline const char* to_string(OrderType t) {
    switch (t) {
        case OrderType::LIMIT:  return "LIMIT";
        case OrderType::MARKET: return "MARKET";
        case OrderType::IOC:    return "IOC";
    }
    return "UNKNOWN";
}

using Price =  int64_t; // Price in 4 decimal places (e.g., $100.50 -> 10050) so that we can avoid floating point issues.
constexpr int64_t PRICE_SCALE = 10000; 

inline Price to_fixed(double price) {
    return static_cast<Price>(price * PRICE_SCALE + 0.5); // Round to nearest
}

inline double to_double(Price price) {
    return static_cast<double>(price) / PRICE_SCALE;
}