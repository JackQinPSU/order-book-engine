#include "matching_engine.h"
#include "event.h"
#include "types.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
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

static const std::string SYMBOL = "AAPL";

// What a scenario's order generator returns for one new limit order.
struct OrderSpec {
    Side   side;
    double price;
    int    qty;
};

// A workload shape. Each scenario gets its own result.md section, because numbers
// from different shapes aren't comparable.
struct Scenario {
    std::string name;         // short label for the console
    std::string heading;      // result.md section heading (must stay stable across runs)
    std::string description;  // paragraph written under the heading the first time
    int         warmup_orders;
    int         num_ops;
    double      cancel_ratio;
    std::function<OrderSpec(std::mt19937&)> next_order;
};

// One measured operation: either a new order or a cancel, indexing into its event vector.
struct Op {
    enum class Kind { NEW, CANCEL } kind;
    size_t idx;
};

// The full pre-generated event sequence for one scenario.
struct Workload {
    std::vector<NewOrderEvent>    new_events;     // warm-up orders first, then measured ones
    std::vector<CancelOrderEvent> cancel_events;
    std::vector<Op>               ops;            // measured sequence only
    int                           warmup_orders = 0;
    size_t                        peak_resting  = 0;  // upper bound on book size seen while generating
};

// Latency samples split by what the operation actually did.
struct Buckets {
    std::vector<std::int64_t> add;     // new order that rested without trading
    std::vector<std::int64_t> match;   // new order that produced >= 1 trade
    std::vector<std::int64_t> cancel;  // cancel of a resting order
};

struct Result {
    std::int64_t throughput_ns  = 0;
    std::int64_t total_trades   = 0;
    std::int64_t latency_trades = 0;
    Buckets      lat;
};

// ---- Build the workload by simulating it once (untimed) ----
// Cancels must target orders that are actually resting at that moment, so we run a
// reference engine while generating. Matching is deterministic, so the timed runs
// replay the exact same book states and every cancel hits a live order.
static Workload build_workload(const Scenario& sc) {
    Workload w;
    w.warmup_orders = sc.warmup_orders;
    w.new_events.reserve(sc.warmup_orders + sc.num_ops);
    w.cancel_events.reserve(static_cast<size_t>(sc.num_ops * sc.cancel_ratio * 1.1));
    w.ops.reserve(sc.num_ops);

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> op_dist(0.0, 1.0);
    std::uniform_int_distribution<size_t> pick;

    int64_t next_id = 1;
    int64_t ts = 0;

    MatchingEngine sim;
    std::vector<int64_t> live;  // ids that rested at some point (may since have been filled)

    auto submit_new = [&]() {
        OrderSpec o = sc.next_order(rng);
        w.new_events.emplace_back(next_id++, SYMBOL, o.side, to_fixed(o.price), o.qty, ts++,
                                  OrderType::LIMIT);
        const NewOrderEvent& e = w.new_events.back();
        sim.process(e);
        OrderBook* book = sim.getBook(SYMBOL);
        if (book && book->hasOrder(e.getOrderId())) live.push_back(e.getOrderId());
    };

    for (int i = 0; i < sc.warmup_orders; ++i) submit_new();

    for (int i = 0; i < sc.num_ops; ++i) {
        bool did_cancel = false;
        if (op_dist(rng) < sc.cancel_ratio) {
            OrderBook* book = sim.getBook(SYMBOL);
            // Pick random candidates, dropping ones that were filled since they rested.
            while (!live.empty()) {
                size_t j = pick(rng, decltype(pick)::param_type(0, live.size() - 1));
                int64_t id = live[j];
                live[j] = live.back();
                live.pop_back();
                if (book && book->hasOrder(id)) {
                    w.cancel_events.emplace_back(id, ts++);
                    sim.process(w.cancel_events.back());
                    w.ops.push_back({Op::Kind::CANCEL, w.cancel_events.size() - 1});
                    did_cancel = true;
                    break;
                }
            }
        }
        if (!did_cancel) {  // not a cancel, or nothing left to cancel
            submit_new();
            w.ops.push_back({Op::Kind::NEW, w.new_events.size() - 1});
        }
        w.peak_resting = std::max(w.peak_resting, live.size());
    }
    return w;
}

static Result run_workload(const Workload& w) {
    Result r;

    // Warm-up: fills the book and warms caches/branch predictor/allocator. Not timed.
    auto warm_up = [&](MatchingEngine& engine) {
        for (int i = 0; i < w.warmup_orders; ++i) engine.process(w.new_events[i]);
    };

    // ---- Run 1: throughput (no per-op timing, so no clock overhead) ----
    {
        MatchingEngine engine;
        warm_up(engine);
        auto start = Clock::now();
        for (const Op& op : w.ops) {
            if (op.kind == Op::Kind::NEW) {
                auto trades = engine.process(w.new_events[op.idx]);
                r.total_trades += static_cast<int64_t>(trades.size());
            } else {
                engine.process(w.cancel_events[op.idx]);
            }
        }
        auto end = Clock::now();
        r.throughput_ns = elapsed_ns(start, end);
    }

    // ---- Run 2: per-op latency, bucketed (fresh engine, same warm-up, same ops) ----
    r.lat.add.reserve(w.ops.size());
    r.lat.match.reserve(w.ops.size());
    r.lat.cancel.reserve(w.cancel_events.size());
    {
        MatchingEngine engine;
        warm_up(engine);
        for (const Op& op : w.ops) {
            if (op.kind == Op::Kind::NEW) {
                auto t0 = Clock::now();
                auto trades = engine.process(w.new_events[op.idx]);
                auto t1 = Clock::now();
                (trades.empty() ? r.lat.add : r.lat.match).push_back(elapsed_ns(t0, t1));
                r.latency_trades += static_cast<int64_t>(trades.size());
            } else {
                auto t0 = Clock::now();
                engine.process(w.cancel_events[op.idx]);
                auto t1 = Clock::now();
                r.lat.cancel.push_back(elapsed_ns(t0, t1));
            }
        }
    }

    std::sort(r.lat.add.begin(), r.lat.add.end());
    std::sort(r.lat.match.begin(), r.lat.match.end());
    std::sort(r.lat.cancel.begin(), r.lat.cancel.end());
    return r;
}

static void print_result(const Scenario& sc, const Workload& w, const Result& r) {
    const size_t num_ops = w.ops.size();
    const double duration_sec = r.throughput_ns / 1'000'000'000.0;

    std::cout << "\n=== Scenario: " << sc.name << " ===\n";
    std::cout << "Measured ops:     " << num_ops << " (" << w.cancel_events.size() << " cancels, "
              << num_ops - w.cancel_events.size() << " new orders)\n";
    std::cout << "Cancel pool peak: " << w.peak_resting
              << " ids (upper bound on resting orders; some were since filled)\n";
    std::cout << "Trades generated: " << r.total_trades << "\n";
    if (r.latency_trades != r.total_trades) {
        std::cout << "WARNING: latency run produced " << r.latency_trades
                  << " trades (runs should be identical)\n";
    }
    std::cout << "Total time:       " << duration_sec << " sec (throughput run)\n";
    std::cout << "Throughput:       " << num_ops / duration_sec << " ops/sec\n";
    std::cout << "Avg latency:      " << static_cast<double>(r.throughput_ns) / num_ops << " ns/op\n";

    std::cout << "\nLatency (per op, ns)\n";
    std::cout << std::left << std::setw(8) << "" << std::right
              << std::setw(10) << "count" << std::setw(9) << "p50" << std::setw(9) << "p99"
              << std::setw(10) << "p99.9" << std::setw(12) << "max" << "\n";
    auto print_bucket = [](const char* name, const std::vector<std::int64_t>& v) {
        std::cout << std::left << std::setw(8) << name << std::right
                  << std::setw(10) << v.size()
                  << std::setw(9) << percentile(v, 50.0)
                  << std::setw(9) << percentile(v, 99.0)
                  << std::setw(10) << percentile(v, 99.9)
                  << std::setw(12) << (v.empty() ? 0 : v.back()) << "\n";
    };
    print_bucket("add", r.lat.add);
    print_bucket("match", r.lat.match);
    print_bucket("cancel", r.lat.cancel);
}

// Append one row under the scenario's section in result.md, creating the section if needed.
static bool append_result(const Scenario& sc, const Workload& w, const Result& r,
                          const std::string& date, const std::string& commit) {
    std::string doc;
    {
        std::ifstream in(RESULTS_PATH, std::ios::binary);
        if (in) doc.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    // Build this run's row.
    const double duration_sec = r.throughput_ns / 1'000'000'000.0;
    std::string row = "| " + date + " | " + commit + " | " +
                      std::to_string(static_cast<std::int64_t>(w.ops.size() / duration_sec));
    for (const auto* v : {&r.lat.add, &r.lat.match, &r.lat.cancel}) {
        row += " | " + std::to_string(percentile(*v, 50.0)) +
               " | " + std::to_string(percentile(*v, 99.0)) +
               " | " + std::to_string(percentile(*v, 99.9));
    }
    row += " | " + std::to_string(r.total_trades) + " |\n";

    size_t heading_pos = doc.find(sc.heading);
    if (heading_pos == std::string::npos) {
        // New section at the end of the file: heading, description, table header, row.
        if (doc.empty()) doc = "# Benchmark Results\n";
        else if (doc.back() != '\n') doc += "\n";
        doc += "\n" + sc.heading + "\n\n" + sc.description + "\n\n";
        doc += "| Date | Commit | Throughput (ops/s) "
               "| Add p50 | Add p99 | Add p99.9 "
               "| Match p50 | Match p99 | Match p99.9 "
               "| Cancel p50 | Cancel p99 | Cancel p99.9 | Trades |\n";
        doc += "|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|\n";
        doc += row;
    } else {
        // Existing section: insert the row after the section's last line, i.e. just before
        // the next "## " heading (or the end of the file), skipping trailing blank lines.
        size_t end = doc.find("\n## ", heading_pos + sc.heading.size());
        end = (end == std::string::npos) ? doc.size() : end + 1;
        while (end >= 2 && doc[end - 1] == '\n' && (doc[end - 2] == '\n' || doc[end - 2] == '\r'))
            end -= (doc[end - 2] == '\r') ? 2 : 1;  // step back over blank lines
        if (end > 0 && doc[end - 1] != '\n') { doc.insert(end, "\n"); ++end; }
        doc.insert(end, row);
    }

    std::ofstream md(RESULTS_PATH, std::ios::binary | std::ios::trunc);
    if (!md) {
        std::cerr << "Could not write " << RESULTS_PATH << "\n";
        return false;
    }
    md << doc;
    return true;
}

int main() {
    std::vector<Scenario> scenarios;

    // 1) Wide: prices spread evenly over 1-1000, so each level holds only a few orders.
    //    The draw order (side, price, qty) is kept identical to earlier versions so the
    //    generated workload, and therefore existing result.md rows, stay comparable.
    scenarios.push_back({
        "wide (prices 1-1000)",
        "## Mixed workload: add / match / cancel",
        "100k untimed warm-up orders, then 1M measured ops: ~70% new random limit orders "
        "(price 1-1000, qty 1-100) and ~30% cancels of random resting orders, seed 42, "
        "Release build. add = new order that rested without trading, match = new order "
        "that traded, cancel = cancel of a resting order. Latencies in ns; clock "
        "resolution ~100 ns on Windows.",
        100'000, 1'000'000, 0.30,
        [side_dist  = std::uniform_int_distribution<int>(0, 1),
         price_dist = std::uniform_int_distribution<int>(1, 1000),
         qty_dist   = std::uniform_int_distribution<int>(1, 100)](std::mt19937& rng) mutable {
            Side side = side_dist(rng) == 0 ? Side::BUY : Side::SELL;
            double price = static_cast<double>(price_dist(rng));
            int qty = qty_dist(rng);
            return OrderSpec{side, price, qty};
        }});

    // 2) Deep queues: passive orders sit on 10 levels each side of a fixed mid (bids
    //    90-99, asks 101-110), so every level holds thousands of orders. 10% of new
    //    orders are aggressive and cross the spread. This is the shape where per-level
    //    O(K) work (cancel's scan and erase, cleaning filled orders) shows up.
    static constexpr int MID  = 100;
    static constexpr int BAND = 10;
    scenarios.push_back({
        "deep queues (10 levels per side)",
        "## Deep-queue workload: add / match / cancel",
        "100k untimed warm-up orders, then 1M measured ops: ~60% new limit orders and ~40% "
        "cancels of random resting orders, seed 42, Release build. Passive orders rest on "
        "10 levels each side of a fixed mid (bids 90-99, asks 101-110), so each level "
        "queues thousands of orders; 10% of new orders are aggressive (buy at 110 / sell "
        "at 90) and trade. qty 1-100. Same columns as the mixed workload. Latencies in "
        "ns; clock resolution ~100 ns on Windows.",
        100'000, 1'000'000, 0.40,
        [side_dist  = std::uniform_int_distribution<int>(0, 1),
         aggr_dist  = std::uniform_real_distribution<double>(0.0, 1.0),
         level_dist = std::uniform_int_distribution<int>(1, BAND),
         qty_dist   = std::uniform_int_distribution<int>(1, 100)](std::mt19937& rng) mutable {
            const bool buy = side_dist(rng) == 0;
            const bool aggressive = aggr_dist(rng) < 0.10;
            int px;
            if (aggressive) px = buy ? MID + BAND : MID - BAND;  // crosses the whole spread
            else            px = buy ? MID - level_dist(rng) : MID + level_dist(rng);
            int qty = qty_dist(rng);
            return OrderSpec{buy ? Side::BUY : Side::SELL, static_cast<double>(px), qty};
        }});

    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char date[32];
    std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M", &local);
    const std::string commit = git_commit();

    std::cout << "Benchmark Results (commit " << commit << ")\n";
    std::cout << "=================\n";

    for (const Scenario& sc : scenarios) {
        Workload w = build_workload(sc);
        Result r = run_workload(w);
        print_result(sc, w, r);
        if (!append_result(sc, w, r, date, commit)) return 1;
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

    std::cout << "\nClock overhead:   ~" << percentile(clock_cost, 50.0)
              << " ns per sample (included in numbers above)\n";
    // The clock's type says nanoseconds, but the real tick can be coarser (~100 ns on Windows).
    // Measure it: the smallest non-zero gap seen between two reads.
    auto first_nonzero = std::upper_bound(clock_cost.begin(), clock_cost.end(), std::int64_t{0});
    if (first_nonzero != clock_cost.end()) {
        std::cout << "Clock resolution: ~" << *first_nonzero
                  << " ns (latencies are rounded to this step)\n";
    }

    std::cout << "\nAppended results to " << RESULTS_PATH << "\n";
    return 0;
}
