#pragma once
#include <cstdint>

enum class Side : uint8_t {
    BUY,
    SELL
};

inline const char* to_string(Side s) {
    return (s == Side::BUY) ? "BUY" : "SELL";
}