#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <cstdint>
#include <chrono>
#include "types.h"

enum class EventType {
    NEW_ORDER,
    CANCEL_ORDER,
    MODIFY_ORDER
};

// Base event
class Event {
public:
    Event(EventType type, int64_t timestamp)
        : type_(type), timestamp_(timestamp) {}

    EventType getType()      const { return type_; }
    int64_t   getTimestamp() const { return timestamp_; }

    virtual ~Event() = default;

protected:
    EventType type_;
    int64_t   timestamp_;
};

// New Order
class NewOrderEvent : public Event {
public:
    NewOrderEvent(int64_t order_id, std::string symbol, Side side,
                  Price price, int quantity, int64_t timestamp)
        : Event(EventType::NEW_ORDER, timestamp),
          order_id_(order_id), symbol_(std::move(symbol)),
          side_(side), price_(price), quantity_(quantity) {}

    int64_t     getOrderId()  const { return order_id_; }
    std::string getSymbol()   const { return symbol_; }
    Side        getSide()     const { return side_; }
    Price       getPrice()    const { return price_; }
    int         getQuantity() const { return quantity_; }

private:
    int64_t     order_id_;
    std::string symbol_;
    Side        side_;
    Price       price_;   // fixed-point
    int         quantity_;
};

// Cancel Order
class CancelOrderEvent : public Event {
public:
    CancelOrderEvent(int64_t order_id, int64_t timestamp)
        : Event(EventType::CANCEL_ORDER, timestamp),
          order_id_(order_id) {}

    int64_t getOrderId() const { return order_id_; }

private:
    int64_t order_id_;
};

// Modify Order (stub — implement alongside MODIFY_ORDER handling in MatchingEngine)
class ModifyOrderEvent : public Event {
public:
    ModifyOrderEvent(int64_t order_id, Price new_price, int new_qty, int64_t timestamp)
        : Event(EventType::MODIFY_ORDER, timestamp),
          order_id_(order_id), new_price_(new_price), new_qty_(new_qty) {}

    int64_t getOrderId()  const { return order_id_; }
    Price   getNewPrice() const { return new_price_; }
    int     getNewQty()   const { return new_qty_; }

private:
    int64_t order_id_;
    Price   new_price_;
    int     new_qty_;
};

inline int64_t getCurrentTimestamp() {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

#endif // EVENT_H