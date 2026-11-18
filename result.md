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
| 2026-11-17 15:21 | 296730a-dirty | 1656912 | 800 | 1400 | 28600 | 800 | 3000 | 8700 | 100 | 400 | 500 | 545023 |
| 2026-11-17 15:45 | 8f6a079 | 1526937 | 700 | 1400 | 25400 | 700 | 2800 | 9000 | 100 | 400 | 600 | 545023 |
| 2026-11-17 15:45 | 8f6a079-dirty | 1422011 | 800 | 2300 | 28500 | 800 | 3300 | 8900 | 100 | 500 | 2200 | 545023 |
| 2026-11-17 15:46 | 8f6a079-dirty | 1445390 | 800 | 1600 | 26600 | 800 | 3300 | 8500 | 100 | 500 | 800 | 545023 |
| 2026-11-17 15:55 | 8f6a079-dirty | 1461620 | 900 | 2000 | 33400 | 700 | 3400 | 10200 | 100 | 600 | 1000 | 545023 |
| 2026-11-17 15:55 | 8f6a079-dirty | 1660649 | 800 | 1400 | 26200 | 700 | 2800 | 8000 | 100 | 400 | 700 | 545023 |
| 2026-11-17 15:55 | 8f6a079-dirty | 1631657 | 700 | 1300 | 25300 | 700 | 2700 | 7700 | 100 | 400 | 600 | 545023 |
| 2026-11-18 15:35 | 7b404a4-dirty | 1598431 | 800 | 1500 | 31100 | 500 | 2100 | 5100 | 100 | 500 | 700 | 545023 |


## Deep-queue workload: add / match / cancel

100k untimed warm-up orders, then 1M measured ops: ~60% new limit orders and ~40% cancels of random resting orders, seed 42, Release build. Passive orders rest on 10 levels each side of a fixed mid (bids 90-99, asks 101-110), so each level queues thousands of orders; 10% of new orders are aggressive (buy at 110 / sell at 90) and trade. qty 1-100. Same columns as the mixed workload. Latencies in ns; clock resolution ~100 ns on Windows.

| Date | Commit | Throughput (ops/s) | Add p50 | Add p99 | Add p99.9 | Match p50 | Match p99 | Match p99.9 | Cancel p50 | Cancel p99 | Cancel p99.9 | Trades |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 2026-11-17 15:21 | 296730a-dirty | 76755 | 1700 | 127700 | 297200 | 1500 | 534100 | 1087200 | 700 | 1700 | 5900 | 118100 |
| 2026-11-17 15:45 | 8f6a079 | 219052 | 900 | 122800 | 170000 | 1000 | 419300 | 778500 | 400 | 1000 | 4000 | 118100 |
| 2026-11-17 15:45 | 8f6a079-dirty | 201120 | 900 | 123400 | 167900 | 1000 | 400500 | 774000 | 400 | 1000 | 3700 | 118100 |
| 2026-11-17 15:46 | 8f6a079-dirty | 79515 | 2100 | 230700 | 460100 | 2200 | 824500 | 1689000 | 800 | 2000 | 13200 | 118100 |
| 2026-11-17 15:55 | 8f6a079-dirty | 1283642 | 800 | 1400 | 47100 | 800 | 4100 | 14800 | 300 | 700 | 3700 | 118100 |
| 2026-11-17 15:55 | 8f6a079-dirty | 1358463 | 800 | 2400 | 48500 | 800 | 4100 | 13600 | 300 | 700 | 4000 | 118100 |
| 2026-11-17 15:55 | 8f6a079-dirty | 1355958 | 800 | 1400 | 47500 | 800 | 4100 | 17900 | 400 | 700 | 4000 | 118100 |
| 2026-11-18 15:35 | 7b404a4-dirty | 1084967 | 800 | 2200 | 45300 | 600 | 3200 | 14400 | 400 | 800 | 4100 | 118100 |
