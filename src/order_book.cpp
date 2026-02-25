#include "order_book.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
int64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}
} // namespace

OrderBook::OrderBook(std::string symbol)
    : symbol_(std::move(symbol)),
      bids_(Side::BUY),
      asks_(Side::SELL),
      trade_counter_(0) {
    if (symbol_.empty()) {
        throw std::invalid_argument("OrderBook symbol cannot be empty");
    }
}

std::vector<Trade> OrderBook::addOrder(const std::shared_ptr<Order>& order) {
    if (!order) throw std::invalid_argument("Null order");
    // If Order has symbol, enforce:
    if (order->getSymbol() != symbol_) throw std::invalid_argument("Wrong symbol");

    std::vector<Trade> trades;

    BookSide& opposite = (order->getSide() == Side::BUY) ? asks_ : bids_;
    BookSide& my_side  = (order->getSide() == Side::BUY) ? bids_ : asks_;

    // 1) match first
    auto matched = matchIncoming(order, opposite);
    trades.insert(trades.end(), matched.begin(), matched.end());

    // 2) rest remainder
    if (!order->isFilled()) {
        my_side.addOrder(order);
        order_side_[order->getOrderId()] = order->getSide();
    }

    return trades;
}

bool OrderBook::cancelOrder(int64_t order_id) {
    auto it = order_side_.find(order_id);
    if (it == order_side_.end()) return false;

    const Side s = it->second;
    bool ok = false;

    if (s == Side::BUY) ok = bids_.removeOrder(order_id);
    else                ok = asks_.removeOrder(order_id);

    if (ok) order_side_.erase(it);
    return ok;
}

std::shared_ptr<Order> OrderBook::getBestBid() {
    // should be "safe": BookSide::getBestOrder() cleans filled-at-front best levels
    return bids_.getBestOrder();
}

std::shared_ptr<Order> OrderBook::getBestAsk() {
    return asks_.getBestOrder();
}

double OrderBook::getSpread() {
    auto bid = bids_.getBestPrice();
    auto ask = asks_.getBestPrice();
    if (!bid || !ask) return 0.0;
    return (*ask) - (*bid);
}

void OrderBook::print(int levels) const {
    std::cout << "=== ORDER BOOK: " << symbol_ << " ===\n";
    asks_.print(levels);
    bids_.print(levels);
}

std::vector<Trade> OrderBook::matchIncoming(const std::shared_ptr<Order>& incoming, BookSide& opposite) {
    std::vector<Trade> trades;
    if (!incoming) return trades;

    while (!incoming->isFilled()) {
        // Should return nullptr if empty (or after cleaning reveals no live orders)
        auto maker = opposite.getBestOrder();
        if (!maker) break;

        const double inc_px  = incoming->getPrice();
        const double maker_px = maker->getPrice();

        const bool crosses =
            (incoming->getSide() == Side::BUY) ? (inc_px >= maker_px)
                                               : (inc_px <= maker_px);

        if (!crosses) break;

        const int qty = std::min(incoming->getRemaining(), maker->getRemaining());
        if (qty <= 0) break;

        // apply fills (must exist on Order)
        incoming->fill(qty);
        maker->fill(qty);

        trades.push_back(makeTrade(incoming, maker, maker_px, qty));

        // If maker is fully filled, it is no longer cancelable.
        // If you keep lazy cleanup in BookSide, you don't need to erase it here,
        // but SHOULD remove its routing entry.
        if (maker->isFilled()) {
            order_side_.erase(maker->getOrderId());
            // If you prefer eager removal instead of lazy cleanup, uncomment:
            // opposite.removeOrder(maker->getOrderId());
        }
    }

    return trades;
}

Trade OrderBook::makeTrade(const std::shared_ptr<Order>& taker,
                           const std::shared_ptr<Order>& maker,
                           double price,
                           int qty) {
    if (!taker || !maker) throw std::invalid_argument("Null taker/maker in makeTrade");

    Trade t;        // Create Trade snapshot
    t.trade_id = ++trade_counter_;
    t.symbol = symbol_;
    t.taker_order_id = taker->getOrderId();
    t.maker_order_id = maker->getOrderId();
    t.taker_side = taker->getSide();
    t.price = price;
    t.quantity = qty;
    t.timestamp = now_ns();
    return t;
}