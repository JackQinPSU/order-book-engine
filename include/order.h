#ifndef ORDER_H
#define ORDER_H

#include <string>
#include <cstdint>

enum class Side : uint8_t {
    BUY,
    SELL
};

inline const char* to_string(Side s) {
    return (s == Side::BUY) ? "BUY" : "SELL";
}

class Order {
public:
    Order(int64_t order_id, std::string symbol, Side side,
          double price, int quantity, int64_t timestamp);
    
    // Getters
    int64_t getOrderId() const { return order_id_; }
    std::string getSymbol() const { return symbol_; }
    Side getSide() const { return side_; }
    double getPrice() const { return price_; }
    int getQuantity() const { return quantity_; }
    int getFilled() const { return filled_; }
    int64_t getTimestamp() const { return timestamp_; }
    
    // State
    int getRemaining() const { return quantity_ - filled_; }
    bool isFilled() const { return filled_ >= quantity_; }
    
    // Actions
    void fill(int qty);
    
    // Display
    void print() const;
    
    // Comparison for priority (price-time)
    bool operator<(const Order& other) const;
    
private:
    int64_t order_id_;
    std::string symbol_;
    Side side_;  // "BUY" or "SELL"
    double price_;
    int quantity_;
    int filled_;
    int64_t timestamp_;
};

#endif // ORDER_H