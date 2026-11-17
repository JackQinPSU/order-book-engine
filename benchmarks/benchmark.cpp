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
#include <iterator>
#include <random>
#include <string>
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

// Nearest-rank percentile on an already-sorted vector (0 if empty).
static std::int64_t percentile(const std::vector<std::int64_t>& sorted, double p) {
    if (sorted.empty()) return 0;
    size_t idx = static_cast<size_t>(p / 100.0 * (sorted.size() - 1));
    return sorted[idx];
}

// One measured operation: either a new order or a cancel, indexing into its event vector.
struct Op {
    enum class Kind { NEW, CANCEL } kind;
    size_t idx;
};

// Latency samples split by what the operation actually did.
struct Buckets {
    std::vector<std::int64_t> add;     // new order that rested without trading
    std::vector<std::int64_t> match;   // new order that produced >= 1 trade
    std::vector<std::int64_t> cancel;  // cancel of a resting order
};

int main() {
    constexpr int    WARMUP_ORDERS = 100'000;    // new orders processed first, never timed
    constexpr int    NUM_OPS       = 1'000'000;  // measured operations (new orders + cancels)
    constexpr double CANCEL_RATIO  = 0.30;       // share of measured ops that are cancels
    const std::string SYMBOL = "AAPL";

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> price_dist(1, 1000);
    std::uniform_int_distribution<int> qty_dist(1, 100);
    std::uniform_real_distribution<double> op_dist(0.0, 1.0);

    std::vector<NewOrderEvent>    new_events;
    std::vector<CancelOrderEvent> cancel_events;
    std::vector<Op>               ops;  // measured sequence only
    new_events.reserve(WARMUP_ORDERS + NUM_OPS);
    cancel_events.reserve(static_cast<size_t>(NUM_OPS * CANCEL_RATIO * 1.1));
    ops.reserve(NUM_OPS);

    int64_t next_id = 1;
    int64_t ts = 0;

    auto make_new_order = [&]() -> const NewOrderEvent& {
        Side side = side_dist(rng) == 0 ? Side::BUY : Side::SELL;
        double price = static_cast<double>(price_dist(rng));
        int qty = qty_dist(rng);
        new_events.emplace_back(next_id++, SYMBOL, side, to_fixed(price), qty, ts++, OrderType::LIMIT);
        return new_events.back();
    };

    // ---- Build the workload by simulating it once (untimed) ----
    // Cancels must target orders that are actually resting at that moment, so we run a
    // reference engine while generating. Matching is deterministic, so the timed runs
    // below replay the exact same book states and every cancel hits a live order.
    {
        MatchingEngine sim;
        std::vector<int64_t> live;  // ids that rested at some point (may since have been filled)

        auto submit = [&](const NewOrderEvent& e) {
            sim.process(e);
            OrderBook* book = sim.getBook(SYMBOL);
            if (book && book->hasOrder(e.getOrderId())) live.push_back(e.getOrderId());
        };

        for (int i = 0; i < WARMUP_ORDERS; ++i) submit(make_new_order());

        std::uniform_int_distribution<size_t> pick;
        for (int i = 0; i < NUM_OPS; ++i) {
            bool did_cancel = false;
            if (op_dist(rng) < CANCEL_RATIO) {
                OrderBook* book = sim.getBook(SYMBOL);
                // Pick random candidates, dropping ones that were filled since they rested.
                while (!live.empty()) {
                    size_t j = pick(rng, decltype(pick)::param_type(0, live.size() - 1));
                    int64_t id = live[j];
                    live[j] = live.back();
                    live.pop_back();
                    if (book && book->hasOrder(id)) {
                        cancel_events.emplace_back(id, ts++);
                        sim.process(cancel_events.back());
                        ops.push_back({Op::Kind::CANCEL, cancel_events.size() - 1});
                        did_cancel = true;
                        break;
                    }
                }
            }
            if (!did_cancel) {  // not a cancel, or nothing left to cancel
                submit(make_new_order());
                ops.push_back({Op::Kind::NEW, new_events.size() - 1});
            }
        }
    }

    // Warm-up: fills the book and warms caches/branch predictor/allocator. Not timed.
    auto warm_up = [&](MatchingEngine& engine) {
        for (int i = 0; i < WARMUP_ORDERS; ++i) engine.process(new_events[i]);
    };

    // ---- Run 1: throughput (no per-op timing, so no clock overhead) ----
    std::int64_t throughput_ns = 0;
    int64_t total_trades = 0;
    {
        MatchingEngine engine;
        warm_up(engine);
        auto start = Clock::now();
        for (const Op& op : ops) {
            if (op.kind == Op::Kind::NEW) {
                auto trades = engine.process(new_events[op.idx]);
                total_trades += static_cast<int64_t>(trades.size());
            } else {
                engine.process(cancel_events[op.idx]);
            }
        }
        auto end = Clock::now();
        throughput_ns = elapsed_ns(start, end);
    }

    // ---- Run 2: per-op latency, bucketed (fresh engine, same warm-up, same ops) ----
    Buckets lat;
    lat.add.reserve(NUM_OPS);
    lat.match.reserve(NUM_OPS);
    lat.cancel.reserve(cancel_events.size());
    int64_t latency_trades = 0;
    {
        MatchingEngine engine;
        warm_up(engine);
        for (const Op& op : ops) {
            if (op.kind == Op::Kind::NEW) {
                auto t0 = Clock::now();
                auto trades = engine.process(new_events[op.idx]);
                auto t1 = Clock::now();
                (trades.empty() ? lat.add : lat.match).push_back(elapsed_ns(t0, t1));
                latency_trades += static_cast<int64_t>(trades.size());
            } else {
                auto t0 = Clock::now();
                engine.process(cancel_events[op.idx]);
                auto t1 = Clock::now();
                lat.cancel.push_back(elapsed_ns(t0, t1));
            }
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

    std::sort(lat.add.begin(), lat.add.end());
    std::sort(lat.match.begin(), lat.match.end());
    std::sort(lat.cancel.begin(), lat.cancel.end());

    double duration_sec = throughput_ns / 1'000'000'000.0;

    std::cout << "Benchmark Results\n";
    std::cout << "=================\n";
    std::cout << "Measured ops:     " << NUM_OPS << " (" << cancel_events.size() << " cancels, "
              << NUM_OPS - cancel_events.size() << " new orders)\n";
    std::cout << "Trades generated: " << total_trades << "\n";
    if (latency_trades != total_trades) {
        std::cout << "WARNING: latency run produced " << latency_trades
                  << " trades (runs should be identical)\n";
    }
    std::cout << "\n-- Throughput run --\n";
    std::cout << "Total time:       " << duration_sec << " sec\n";
    std::cout << "Throughput:       " << NUM_OPS / duration_sec << " ops/sec\n";
    std::cout << "Avg latency:      " << static_cast<double>(throughput_ns) / NUM_OPS << " ns/op\n";

    std::cout << "\n-- Latency run (per op, ns) --\n";
    std::cout << std::left << std::setw(8) << "" << std::right
              << std::setw(10) << "count" << std::setw(8) << "p50" << std::setw(8) << "p99"
              << std::setw(9) << "p99.9" << std::setw(12) << "max" << "\n";
    auto print_bucket = [](const char* name, const std::vector<std::int64_t>& v) {
        std::cout << std::left << std::setw(8) << name << std::right
                  << std::setw(10) << v.size()
                  << std::setw(8) << percentile(v, 50.0)
                  << std::setw(8) << percentile(v, 99.0)
                  << std::setw(9) << percentile(v, 99.9)
                  << std::setw(12) << (v.empty() ? 0 : v.back()) << "\n";
    };
    print_bucket("add", lat.add);
    print_bucket("match", lat.match);
    print_bucket("cancel", lat.cancel);

    std::cout << "\nClock overhead:   ~" << percentile(clock_cost, 50.0)
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

    // This workload gets its own table. Older rows (new orders only) are not comparable,
    // so add this section's heading the first time it's missing and append rows below it.
    const std::string SECTION = "## Mixed workload: add / match / cancel";
    std::string existing;
    {
        std::ifstream in(RESULTS_PATH);
        if (in) existing.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    std::ofstream md(RESULTS_PATH, std::ios::app);
    if (!md) {
        std::cerr << "Could not write " << RESULTS_PATH << "\n";
        return 1;
    }
    if (existing.find(SECTION) == std::string::npos) {
        if (existing.empty()) md << "# Benchmark Results\n";
        else if (existing.back() != '\n') md << "\n";
        md << "\n" << SECTION << "\n\n";
        md << "100k untimed warm-up orders, then 1M measured ops: ~70% new random limit orders "
              "(price 1-1000, qty 1-100) and ~30% cancels of random resting orders, seed 42, "
              "Release build. add = new order that rested without trading, match = new order "
              "that traded, cancel = cancel of a resting order. Latencies in ns; clock "
              "resolution ~100 ns on Windows.\n\n";
        md << "| Date | Commit | Throughput (ops/s) "
              "| Add p50 | Add p99 | Add p99.9 "
              "| Match p50 | Match p99 | Match p99.9 "
              "| Cancel p50 | Cancel p99 | Cancel p99.9 | Trades |\n";
        md << "|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|\n";
    }
    md << "| " << std::put_time(&local, "%Y-%m-%d %H:%M")
       << " | " << git_commit()
       << " | " << static_cast<std::int64_t>(NUM_OPS / duration_sec);
    for (const auto* v : {&lat.add, &lat.match, &lat.cancel}) {
        md << " | " << percentile(*v, 50.0)
           << " | " << percentile(*v, 99.0)
           << " | " << percentile(*v, 99.9);
    }
    md << " | " << total_trades << " |\n";

    std::cout << "\nAppended results to " << RESULTS_PATH << "\n";

    return 0;
}
