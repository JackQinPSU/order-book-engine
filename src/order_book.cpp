#include "order_book.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>


namespace {
int64_t now_ns() {
    // Monotonic timestemp for trades.
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
    if (!order->isFilled() && order->getOrderType() == OrderType::LIMIT) {
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
    return to_double(*ask - *bid);
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

        const Price inc_px  = incoming->getPrice();
        const Price maker_px = maker->getPrice();

        const bool crosses =
            (incoming->getOrderType() == OrderType::MARKET)
                ? true
                : ((incoming->getSide() == Side::BUY) ? (inc_px >= maker_px)
                                                      : (inc_px <= maker_px));
                                                      
        if (!crosses) break;

        const int qty = std::min(incoming->getRemaining(), maker->getRemaining());
        if (qty <= 0) break;

        // apply fills (must exist on Order)
        incoming->fill(qty);
        maker->fill(qty);

        trades.push_back(makeTrade(incoming, maker, maker_px, qty));

        // If maker is fully filled, it is no longer cancelable.
        if (maker->isFilled()) {
            order_side_.erase(maker->getOrderId());
        }
    }

    return trades;
}

Trade OrderBook::makeTrade(const std::shared_ptr<Order>& taker,
                           const std::shared_ptr<Order>& maker,
                           Price price,
                           int qty) {
    if (!taker || !maker) throw std::invalid_argument("Null taker/maker in makeTrade");

    Trade t;        // Create Trade snapshot
    t.trade_id = ++trade_counter_;
    t.symbol = symbol_;
    t.taker_order_id = taker->getOrderId();
    t.maker_order_id = maker->getOrderId();
    t.taker_side = taker->getSide();
    t.price = to_double(price);
    t.quantity = qty;
    t.timestamp = now_ns();
    return t;
}

// Check if order_id exists on this side
bool OrderBook::hasOrder(int64_t order_id) const {
    return order_side_.find(order_id) != order_side_.end();
}

std::vector<Trade> OrderBook::modifyOrder(
    int64_t order_id,
    Price new_price,
    int new_qty,
    int64_t timestamp
) {
    std::vector<Trade> trades;

    if (new_qty <= 0) {
        throw std::invalid_argument("Modified quantity must be positive");
    }

    auto side_it = order_side_.find(order_id);
    if (side_it == order_side_.end()) {
        return trades;
    }

    Side side = side_it->second;
    BookSide& my_side = (side == Side::BUY) ? bids_ : asks_;

    auto existing = my_side.findOrder(order_id);
    if (!existing) {
        order_side_.erase(order_id);
        return trades;
    }

    Price old_price = existing->getPrice();
    int old_qty = existing->getQuantity();
      int old_filled = existing->getFilled();

    if (new_qty < old_filled) {
        throw std::invalid_argument("Modified quantity cannot be less than filled quantity");
    }

    // Case 1:
    // Same price + quantity reduction keeps time priority.
    if (new_price == old_price && new_qty < old_qty) {
        if (!existing->resize(new_qty)) {
            throw std::invalid_argument("Invalid modified quantity");
        }
        return trades;
    }

    // Case 2:
    // Same price + quantity increase OR price change loses priority.
    // Remove old order and re-add updated order through normal matching path.
    bool canceled = cancelOrder(order_id);
    if (!canceled) {
        return trades;
    }

  

    auto modified = std::make_shared<Order>(
        order_id,
        symbol_,
        side,
        new_price,
        new_qty,
        timestamp,
        OrderType::LIMIT
    );

    if (old_filled > 0) {
        modified->fill(old_filled);
    }

    trades = addOrder(modified);

    return trades;
}