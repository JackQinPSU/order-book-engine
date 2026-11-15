# Benchmark Results

Run: 2026-11-15 01:16  
Orders: 1000000 random limit orders, price 1-1000, qty 1-100, seed 42  
Trades generated: 773459

## Throughput

| Metric | Value |
|---|---|
| Total time | 1.02288 sec |
| Throughput | 977630 orders/sec |
| Avg latency | 1022.88 ns/order |

## Latency per order

| Percentile | ns |
|---|---|
| p50 | 800 |
| p99 | 2900 |
| p99.9 | 26400 |
| max | 45607900 |

Clock overhead ~0 ns, resolution ~100 ns (latencies rounded to this step).
