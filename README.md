# Order Book Engine

A C++17 limit order book and matching engine that simulates the core logic used in electronic trading systems. The engine supports multi-symbol order routing, price-time priority matching, order cancellation, trade generation, and unit testing with GoogleTest.

This project is designed to demonstrate low-level systems programming, data structure design, and event-driven order processing.

## Features

- Limit order book implementation in C++17
- Price-time priority matching
- Multi-symbol matching engine
- Bid and ask side book management
- FIFO order queues at each price level
- Fixed-point price representation to avoid floating-point precision errors
- Order cancellation by order ID
- Trade generation after successful matches
- Callback support for trade events
- Unit tests with GoogleTest
- CMake-based build system

## Architecture

NewOrderEvent / CancelOrderEvent
            |
            v
    MatchingEngine
            |
            v
      OrderBook per symbol
            |
     -----------------
     |               |
   BidSide         AskSide
     |               |
 price levels    price levels
 FIFO queues     FIFO queues

The matching engine receives order events and routes them to the correct OrderBook based on symbol. Each OrderBook owns a bid side and an ask side. Incoming orders are matched against the opposite side of the book using price-time priority.

## Core Components

### Order

Represents an individual order in the system.

Stores:

- Order ID
- Symbol
- Side: buy or sell
- Price
- Quantity
- Filled quantity
- Timestamp
- Order type

### BookSide

Represents one side of the order book.

For example:

- Bid side for buy orders
- Ask side for sell orders

Orders are stored by price level, and each price level contains a FIFO queue of orders.

std::map<Price, std::deque<std::shared_ptr<Order>>>

The best price is selected differently depending on the side:

- Best bid: highest buy price
- Best ask: lowest sell price

### OrderBook

Represents the full order book for a single trading symbol.

Responsibilities:

- Add new orders
- Match incoming orders against the opposite side
- Generate trades
- Cancel resting orders
- Track order IDs within the book

### MatchingEngine

Top-level engine that handles incoming events.

Responsibilities:

- Route events to the correct symbol book
- Create books for new symbols
- Track order ID to symbol mappings
- Return generated trades
- Trigger trade callbacks

### Trade

Represents an executed trade between an incoming aggressor order and a resting maker order.

Stores:

- Symbol
- Buy order ID
- Sell order ID
- Execution price
- Execution quantity
- Timestamp

## Matching Logic

The engine uses price-time priority.

A buy order matches against the best available ask when:

buy price >= best ask price

A sell order matches against the best available bid when:

sell price <= best bid price

The trade price is the price of the resting order already in the book.

Example:

Resting sell order:
SELL 100 shares of AAPL @ 150.00

Incoming buy order:
BUY 50 shares of AAPL @ 151.00

Result:
Trade executes for 50 shares @ 150.00

The resting order's price is used because it was already available in the book.

## Example

#include "matching_engine.h"
#include "event.h"
#include "types.h"

#include <iostream>

int main() {
    MatchingEngine engine;

    engine.setTradeCallback([](const Trade& trade) {
        trade.print();
    });

    NewOrderEvent sellOrder(
        1,
        "AAPL",
        Side::SELL,
        to_fixed(150.00),
        100,
        1000
    );

    NewOrderEvent buyOrder(
        2,
        "AAPL",
        Side::BUY,
        to_fixed(151.00),
        50,
        1001
    );

    engine.process(sellOrder);
    auto trades = engine.process(buyOrder);

    std::cout << "Trades generated: " << trades.size() << std::endl;

    return 0;
}

Expected result:

Trades generated: 1

The buy order crosses the resting sell order, producing a trade for 50 shares at 150.00.

## Fixed-Point Prices

Prices are represented as fixed-point integers instead of floating-point numbers.

For example:

Price price = to_fixed(150.25);

This avoids precision issues like:

0.1 + 0.2 != 0.3

which can happen with floating-point arithmetic.

Use:

to_fixed(150.25)

to convert a decimal price into the internal format.

Use:

to_double(price)

to convert an internal price back to a readable decimal value.

## Build Instructions

### Prerequisites

- C++17-compatible compiler
- CMake
- GoogleTest

### Build

mkdir build
cd build
cmake ..
make

### Run Tests

ctest

or run the test binary directly:

./tests/order_book_tests

The exact test executable name may vary depending on your CMake configuration.

## Project Structure

order-book-engine/
├── include/
│   ├── book_side.h
│   ├── event.h
│   ├── matching_engine.h
│   ├── order.h
│   ├── order_book.h
│   ├── trade.h
│   └── types.h
│
├── src/
│   ├── book_side.cpp
│   ├── matching_engine.cpp
│   ├── order.cpp
│   ├── order_book.cpp
│   └── trade.cpp
│
├── tests/
│   ├── book_side_tests.cpp
│   ├── event_tests.cpp
│   └── test_order.cpp
│
├── CMakeLists.txt
└── README.md

## Current Limitations

The current implementation focuses on the core limit order book logic. Some features are represented in the type system but are still planned or partially implemented.

Known limitations:

- Market orders are not fully implemented yet
- IOC orders are not fully implemented yet
- Modify order events are defined but not yet handled by the matching engine
- Engine-level order tracking may need cleanup when resting orders are fully filled
- More edge-case validation should be added for invalid prices, quantities, and symbols
- Benchmarking is not yet included

## Planned Improvements

Planned features include:

- Market order support
- IOC order support
- Modify order support
- Level 2 order book snapshots
- Better input validation
- More complete matching tests
- Performance benchmarks
- CSV event replay support
- Latency and throughput measurements
- Better logging and debugging tools

## Example Future Benchmark Goals

The engine can be extended with benchmarks such as:

Processed 1,000,000 orders
Average latency: X ns/order
Throughput: Y orders/sec

This would make the project more comparable to real-world trading infrastructure and systems programming workloads.

## Why This Project Matters

Matching engines are the core infrastructure behind electronic exchanges. They must process orders quickly, maintain correct price-time priority, generate executions, and keep the order book in a consistent state.

This project demonstrates:

- Data structure design
- Event-driven architecture
- Systems programming in C++
- Financial market mechanics
- Unit testing
- Careful handling of price precision
- Separation of engine, book, event, and trade logic

## Technologies Used

- C++17
- CMake
- GoogleTest
- STL containers including std::map, std::deque, and std::unordered_map

## License

This project is for educational and portfolio purposes.