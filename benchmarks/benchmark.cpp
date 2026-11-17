#include "matching_engine.h"
#include "event.h"
#include "types.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

// Set by CMake to <repo>/result.md; fallback writes to the current folder.
#ifndef RESULTS_PATH
#define RESULTS_PATH "result.md"
#endif
#ifndef SOURCE_DIR
#define SOURCE_DIR "."
#endif

using Clock = std::chrono::steady_clock;

static std::int64_t elapsed_ns(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
}

// Short commit hash of the code being benchmarked, with "-dirty" if there are uncommitted changes.
static std::string git_commit() {
#ifdef _WIN32
    FILE* pipe = _popen("git -C \"" SOURCE_DIR "\" describe --always --dirty 2>NUL", "r");
#else
    FILE* pipe = popen("git -C \"" SOURCE_DIR "\" describe --always --dirty 2>/dev/null", "r");
#endif
    if (!pipe) return "unknown";
    char buf[64] = {};
    std::string out = fgets(buf, sizeof(buf), pipe) ? buf : "";
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out.empty() ? "unknown" : out;
}

// Nearest-rank percentile on an already-sorted vector.
static std::int64_t percentile(const std::vector<std::int64_t>& sorted, double p) {
    size_t idx = static_cast<size_t>(p / 100.0 * (sorted.size() - 1));
    return sorted[idx];
}

int main() {
    constexpr int WARMUP_ORDERS = 100'000;  // processed first, never timed
    constexpr int NUM_ORDERS = 1'000'000;   // the orders that are actually measured
    constexpr int TOTAL_ORDERS = WARMUP_ORDERS + NUM_ORDERS;

    std::vector<NewOrderEvent> events;
    events.reserve(TOTAL_ORDERS);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> price_dist(1, 1000);
    std::uniform_int_distribution<int> qty_dist(1, 100);

    for (int i = 0; i < TOTAL_ORDERS; ++i) {
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

    // Split the events: first WARMUP_ORDERS are untimed, the rest are measured.
    const auto warmup_begin   = events.begin();
    const auto measured_begin = events.begin() + WARMUP_ORDERS;
    const auto measured_end   = events.end();

    // Warm-up: fills the book and warms caches/branch predictor/allocator. Not timed.
    auto warm_up = [&](MatchingEngine& engine) {
        for (auto it = warmup_begin; it != measured_begin; ++it) {
            engine.process(*it);
        }
    };

    // ---- Run 1: throughput (no per-order timing, so no clock overhead) ----
    std::int64_t throughput_ns = 0;
    int total_trades = 0;
    {
        MatchingEngine engine;
        warm_up(engine);
        auto start = Clock::now();
        for (auto it = measured_begin; it != measured_end; ++it) {
            auto trades = engine.process(*it);
            total_trades += static_cast<int>(trades.size());
        }
        auto end = Clock::now();
        throughput_ns = elapsed_ns(start, end);
    }

    // ---- Run 2: per-order latency (fresh engine, same warm-up, same events) ----
    std::vector<std::int64_t> latencies;
    latencies.reserve(NUM_ORDERS);  // reserve up front so push_back never reallocates in the loop
    int latency_trades = 0;
    {
        MatchingEngine engine;
        warm_up(engine);
        for (auto it = measured_begin; it != measured_end; ++it) {
            auto t0 = Clock::now();
            auto trades = engine.process(*it);
            auto t1 = Clock::now();
            latencies.push_back(elapsed_ns(t0, t1));
            latency_trades += static_cast<int>(trades.size());
        }
    }

    // Cost of an empty start/stop pair, so you know how much each sample is inflated.
    std::vector<std::int64_t> clock_cost;
    clock_cost.reserve(100'000);
    for (int i = 0; i < 100'000; ++i) {
        auto t0 = Clock::now();
        auto t1 = Clock::now();
        clock_cost.push_back(elapsed_ns(t0, t1));
    }
    std::sort(clock_cost.begin(), clock_cost.end());

    std::sort(latencies.begin(), latencies.end());

    double duration_sec = throughput_ns / 1'000'000'000.0;

    std::cout << "Benchmark Results\n";
    std::cout << "=================\n";
    std::cout << "Orders processed: " << NUM_ORDERS << "\n";
    std::cout << "Trades generated: " << total_trades << "\n";
    if (latency_trades != total_trades) {
        std::cout << "WARNING: latency run produced " << latency_trades
                  << " trades (runs should be identical)\n";
    }
    std::cout << "\n-- Throughput run --\n";
    std::cout << "Total time:       " << duration_sec << " sec\n";
    std::cout << "Throughput:       " << NUM_ORDERS / duration_sec << " orders/sec\n";
    std::cout << "Avg latency:      " << static_cast<double>(throughput_ns) / NUM_ORDERS << " ns/order\n";

    std::cout << "\n-- Latency run (per order, ns) --\n";
    std::cout << "p50:              " << percentile(latencies, 50.0) << "\n";
    std::cout << "p99:              " << percentile(latencies, 99.0) << "\n";
    std::cout << "p99.9:            " << percentile(latencies, 99.9) << "\n";
    std::cout << "max:              " << latencies.back() << "\n";
    std::cout << "Clock overhead:   ~" << percentile(clock_cost, 50.0)
              << " ns per sample (included in numbers above)\n";
    // The clock's type says nanoseconds, but the real tick can be coarser (~100 ns on Windows).
    // Measure it: the smallest non-zero gap seen between two reads.
    auto first_nonzero = std::upper_bound(clock_cost.begin(), clock_cost.end(), std::int64_t{0});
    if (first_nonzero != clock_cost.end()) {
        std::cout << "Clock resolution: ~" << *first_nonzero
                  << " ns (latencies are rounded to this step)\n";
    }

    // ---- Append one row per run to result.md at the repo root ----
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif

    // Write the header only if the file is new/empty; otherwise just append a row.
    bool needs_header = true;
    {
        std::ifstream existing(RESULTS_PATH);
        needs_header = !existing || existing.peek() == std::ifstream::traits_type::eof();
    }

    std::ofstream md(RESULTS_PATH, std::ios::app);
    if (!md) {
        std::cerr << "Could not write " << RESULTS_PATH << "\n";
        return 1;
    }
    if (needs_header) {
        md << "# Benchmark Results\n\n";
        md << "100k untimed warm-up orders, then 1M measured random limit orders "
              "(price 1-1000, qty 1-100, seed 42), Release build. "
              "Latencies in ns; clock resolution ~100 ns on Windows.\n\n";
        md << "| Date | Commit | Throughput (orders/s) | Avg | p50 | p99 | p99.9 | Max | Trades |\n";
        md << "|---|---|--:|--:|--:|--:|--:|--:|--:|\n";
    }
    md << "| " << std::put_time(&local, "%Y-%m-%d %H:%M")
       << " | " << git_commit()
       << " | " << static_cast<std::int64_t>(NUM_ORDERS / duration_sec)
       << " | " << throughput_ns / NUM_ORDERS
       << " | " << percentile(latencies, 50.0)
       << " | " << percentile(latencies, 99.0)
       << " | " << percentile(latencies, 99.9)
       << " | " << latencies.back()
       << " | " << total_trades
       << " |\n";

    std::cout << "\nAppended results to " << RESULTS_PATH << "\n";

    return 0;
}
