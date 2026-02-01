#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <cstdint>
#include <chrono>
#include "types.h"

//Event types
enum class EventType {
    NEW_ORDER,
    CANCEL_ORDER,
    MODIFY_ORDER  
};

//  Base event class
class Event {
public:
    Event(EventType type, int64_t timestamp)
        : type_(type), timestamp_(timestamp) {}
    
    EventType getType() const { return type_; } 
    int64_t getTimestamp() const { return timestamp_; }
    
    virtual ~Event() = default;  // Virtual destructor for base class
    
protected:
    EventType type_;
    int64_t timestamp_;
};

// New Order Event
class NewOrderEvent : public Event {
public:
    NewOrderEvent(int64_t order_id, std::string symbol, Side side,
                  double price, int quantity, int64_t timestamp)
        : Event(EventType::NEW_ORDER, timestamp),
          order_id_(order_id),
          symbol_(symbol),
          side_(side),
          price_(price),
          quantity_(quantity) {}
    
    int64_t getOrderId() const { return order_id_; }
    std::string getSymbol() const { return symbol_; }
    Side getSide() const { return side_; }
    double getPrice() const { return price_; }
    int getQuantity() const { return quantity_; }
    
private:
    int64_t order_id_;
    std::string symbol_;
    Side side_;
    double price_;
    int quantity_;
};

// Cancel Order Event
class CancelOrderEvent : public Event {
public:
    CancelOrderEvent(int64_t order_id, int64_t timestamp)
        : Event(EventType::CANCEL_ORDER, timestamp),
          order_id_(order_id) {}
    
    int64_t getOrderId() const { return order_id_; }
    
private:
    int64_t order_id_;
};

// Helper function to get current timestamp
inline int64_t getCurrentTimestamp() {
    using namespace std::chrono;
    return duration_cast<microseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

#endif // EVENT_H