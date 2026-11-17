# Benchmark Results

1M random limit orders (price 1-1000, qty 1-100, seed 42), Release build. Latencies in ns; clock resolution ~100 ns on Windows.

| Date | Commit | Throughput (orders/s) | Avg | p50 | p99 | p99.9 | Max | Trades |
|---|---|--:|--:|--:|--:|--:|--:|--:|
| 2026-11-15 01:16 | 76b8d20-dirty | 977630 | 1022 | 800 | 2900 | 26400 | 45607900 | 773459 |
| 2026-11-15 01:30 | d8ea385-dirty | 1010622 | 989 | 800 | 2900 | 20700 | 50408500 | 773459 |
| 2026-11-15 01:30 | d8ea385-dirty | 901202 | 1109 | 800 | 3000 | 25300 | 48121000 | 773459 |
| 2026-11-15 01:32 | d8ea385-dirty | 1143573 | 874 | 700 | 2400 | 13700 | 40068600 | 773459 |
| 2026-11-15 01:36 | d8ea385-dirty | 1124397 | 889 | 800 | 2800 | 28400 | 47753000 | 773459 |
| 2026-11-15 01:43 | d8ea385-dirty | 943135 | 1060 | 800 | 3000 | 25100 | 47183700 | 772955 |
| 2026-11-15 01:43 | d8ea385-dirty | 923974 | 1082 | 900 | 3000 | 26400 | 43456900 | 772955 |
| 2026-11-15 01:43 | d8ea385-dirty | 887053 | 1127 | 800 | 2900 | 22700 | 51921700 | 772955 |
| 2026-11-17 14:46 | e38e8ec | 957377 | 1044 | 800 | 2900 | 25900 | 48666800 | 772955 |

## Mixed workload: add / match / cancel

100k untimed warm-up orders, then 1M measured ops: ~70% new random limit orders (price 1-1000, qty 1-100) and ~30% cancels of random resting orders, seed 42, Release build. add = new order that rested without trading, match = new order that traded, cancel = cancel of a resting order. Latencies in ns; clock resolution ~100 ns on Windows.

| Date | Commit | Throughput (ops/s) | Add p50 | Add p99 | Add p99.9 | Match p50 | Match p99 | Match p99.9 | Cancel p50 | Cancel p99 | Cancel p99.9 | Trades |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 2026-11-17 14:48 | e38e8ec-dirty | 1449440 | 800 | 1500 | 26900 | 800 | 3300 | 9000 | 100 | 500 | 700 | 545023 |
