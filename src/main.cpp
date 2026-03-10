#include "matching_engine.h"
#include "event.h"
#include "types.h"
#include <iostream>

int main() {
    MatchingEngine engine;

    // Log every trade to stdout
    engine.onTrade([](const Trade& t) {
        t.print();
    });

    int64_t ts = getCurrentTimestamp();

    // Two resting bids
    engine.process(NewOrderEvent(1, "AAPL", Side::BUY,  to_fixed(150.00), 100, ts++));
    engine.process(NewOrderEvent(2, "AAPL", Side::BUY,  to_fixed(149.50),  50, ts++));

    // Resting ask that doesn't cross
    engine.process(NewOrderEvent(3, "AAPL", Side::SELL, to_fixed(151.00),  30, ts++));

    // Aggressive sell that crosses order 1
    engine.process(NewOrderEvent(4, "AAPL", Side::SELL, to_fixed(150.00),  80, ts++));

    // Cancel the remaining ask
    engine.process(CancelOrderEvent(3, ts++));

    std::cout << "\n";
    engine.printAll();
    std::cout << "Total trades: " << engine.totalTrades() << "\n";

    return 0;
}