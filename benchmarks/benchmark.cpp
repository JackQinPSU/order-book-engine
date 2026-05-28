#include "matching_engine.h"
#include "event.h"
#include "types.h"

#include <chrono>
#include <iostream>
#include <random>
#include <vector>

int main() {
    MatchingEngine engine;

    constexpr int NUM_ORDERS = 1'000'000;

    std::vector<NewOrderEvent> events;
    events.reserve(NUM_ORDERS);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> price_dist(99, 101);
    std::uniform_int_distribution<int> qty_dist(1, 100);

    for (int i = 0; i < NUM_ORDERS; ++i) {
        Side side = side_dist(rng) == 0 ? Side::BUY : Side::SELL;

        double price = static_cast<double>(price_dist(rng));
        int qty = qty_dist(rng);

        events.emplace_back(
            i + 1,
            "AAPL",
            side,
            to_fixed(price),
            qty,
            i,
            OrderType::LIMIT
        );
    }

    auto start = std::chrono::high_resolution_clock::now();

    int total_trades = 0;

    for (const auto& event : events) {
        auto trades = engine.process(event);
        total_trades += static_cast<int>(trades.size());
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto duration_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    double duration_sec = duration_ns / 1'000'000'000.0;
    double orders_per_sec = NUM_ORDERS / duration_sec;
    double avg_latency_ns = static_cast<double>(duration_ns) / NUM_ORDERS;

    std::cout << "Benchmark Results\n";
    std::cout << "=================\n";
    std::cout << "Orders processed: " << NUM_ORDERS << "\n";
    std::cout << "Trades generated: " << total_trades << "\n";
    std::cout << "Total time:       " << duration_sec << " sec\n";
    std::cout << "Throughput:       " << orders_per_sec << " orders/sec\n";
    std::cout << "Avg latency:      " << avg_latency_ns << " ns/order\n";

    return 0;
}