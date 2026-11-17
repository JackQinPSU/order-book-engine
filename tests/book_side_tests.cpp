#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

#include "book_side.h"
#include "order.h"
#include "types.h"
#include "order_book.h"


// ---- Helpers ----

static std::shared_ptr<Order> MakeOrder(
   int64_t id,
   Side side,
   double price,
   int qty,
   int64_t ts
){
    return std::make_shared<Order>(id, "AAPL", side, to_fixed(price), qty, ts);
}

class BookSideTest : public ::testing::Test {
protected:
    BookSide bids{Side::BUY};
    BookSide asks{Side::SELL};
};

// -------------------- Empty state --------------------

TEST_F(BookSideTest, EmptyBook_BestPriceIsNullopt_AndBestOrderNull) {
    EXPECT_FALSE(bids.getBestPrice().has_value());
    EXPECT_EQ(bids.getBestOrder(), nullptr);
}

// -------------------- Best price --------------------

TEST_F(BookSideTest, Bids_BestPriceIsHighest) {
    bids.addOrder(MakeOrder(1, Side::BUY, 100.0, 10, 1));
    bids.addOrder(MakeOrder(2, Side::BUY, 101.0, 10, 2));
    bids.addOrder(MakeOrder(3, Side::BUY,  99.0, 10, 3));

    auto best = bids.getBestPrice();
    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(*best, to_fixed(101.0));
}

TEST_F(BookSideTest, Asks_BestPriceIsLowest) {
    asks.addOrder(MakeOrder(1, Side::SELL, 100.0, 10, 1));
    asks.addOrder(MakeOrder(2, Side::SELL,  98.0, 10, 2));
    asks.addOrder(MakeOrder(3, Side::SELL, 101.0, 10, 3));

    auto best = asks.getBestPrice();
    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(*best, to_fixed(98.0));
}

// -------------------- FIFO within same price level --------------------

TEST_F(BookSideTest, SamePrice_FIFO_ReturnsOldestFirst) {
    // Same price, increasing timestamps (older = smaller timestamp)
    bids.addOrder(MakeOrder(10, Side::BUY, 100.0, 10, 100));
    bids.addOrder(MakeOrder(11, Side::BUY, 100.0, 10, 101));
    bids.addOrder(MakeOrder(12, Side::BUY, 100.0, 10, 102));

    auto best = bids.getBestOrder();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->getOrderId(), 10);
}

TEST_F(BookSideTest, SamePrice_FIFO_AfterRemovingOldest_NextOldestIsBest) {
    bids.addOrder(MakeOrder(10, Side::BUY, 100.0, 10, 100));
    bids.addOrder(MakeOrder(11, Side::BUY, 100.0, 10, 101));

    bids.removeOrder(10);

    auto best = bids.getBestOrder();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->getOrderId(), 11);
}


// -------------------- Cancel updates best price --------------------

TEST_F(BookSideTest, CancelOnlyOrderAtBestPrice_UpdatesBestPrice) {
    bids.addOrder(MakeOrder(1, Side::BUY, 100.0, 10, 1));
    bids.addOrder(MakeOrder(2, Side::BUY, 101.0, 10, 2)); // best
    bids.addOrder(MakeOrder(3, Side::BUY,  99.0, 10, 3));

    auto best0 = bids.getBestPrice();
    ASSERT_TRUE(best0.has_value());
    EXPECT_EQ(*best0, to_fixed(101.0));

    bids.removeOrder(2); // remove only 101 level

    auto best1 = bids.getBestPrice();
    ASSERT_TRUE(best1.has_value());
    EXPECT_EQ(*best1, to_fixed(100.0));
}

// -------------------- popBestOrder --------------------

TEST_F(BookSideTest, PopBestOrder_RemovesFront_NextOldestBecomesBest) {
    bids.addOrder(MakeOrder(1, Side::BUY, 100.0, 10, 100));
    bids.addOrder(MakeOrder(2, Side::BUY, 100.0, 10, 101));

    bids.popBestOrder();

    auto best = bids.getBestOrder();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->getOrderId(), 2);
    EXPECT_EQ(bids.findOrder(1), nullptr);   // id lookup cleared too
    EXPECT_FALSE(bids.removeOrder(1));       // can't cancel a popped order
}

TEST_F(BookSideTest, PopBestOrder_LastAtLevel_MovesToNextLevel_ThenEmpty) {
    bids.addOrder(MakeOrder(1, Side::BUY, 101.0, 10, 100));   // best bid
    bids.addOrder(MakeOrder(2, Side::BUY, 100.0, 10, 101));

    bids.popBestOrder();   // 101 level now empty and removed
    auto best = bids.getBestPrice();
    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(*best, to_fixed(100.0));

    bids.popBestOrder();
    EXPECT_TRUE(bids.isEmpty());
    EXPECT_EQ(bids.getBestOrder(), nullptr);

    bids.popBestOrder();   // no-op on an empty side
    EXPECT_TRUE(bids.isEmpty());
}

TEST_F(BookSideTest, PopBestOrder_Asks_RemovesLowestPrice) {
    asks.addOrder(MakeOrder(1, Side::SELL, 101.0, 10, 100));
    asks.addOrder(MakeOrder(2, Side::SELL,  99.0, 10, 101));   // best ask

    asks.popBestOrder();

    auto best = asks.getBestOrder();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->getOrderId(), 1);
}

// -------------------- Filled makers leave the book during matching --------------------

TEST(OrderBookMatchTest, FullyFilledMakersAreRemoved_PartialStaysAtFront) {
    OrderBook book("AAPL");
    book.addOrder(MakeOrder(1, Side::SELL, 100.0, 10, 1));
    book.addOrder(MakeOrder(2, Side::SELL, 100.0, 10, 2));
    book.addOrder(MakeOrder(3, Side::SELL, 100.0, 10, 3));

    // Takes all of #1, all of #2 and 5 of #3.
    auto trades = book.addOrder(MakeOrder(4, Side::BUY, 100.0, 25, 4));
    ASSERT_EQ(trades.size(), 3u);

    EXPECT_FALSE(book.hasOrder(1));
    EXPECT_FALSE(book.hasOrder(2));
    EXPECT_FALSE(book.cancelOrder(1));       // filled orders are not cancelable
    EXPECT_TRUE(book.hasOrder(3));

    auto best = book.getBestAsk();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->getOrderId(), 3);
    EXPECT_EQ(best->getRemaining(), 5);
}

TEST(OrderBookMatchTest, WholeLevelFilled_LevelRemoved_BookEmpty) {
    OrderBook book("AAPL");
    book.addOrder(MakeOrder(1, Side::SELL, 100.0, 10, 1));
    book.addOrder(MakeOrder(2, Side::SELL, 100.0, 10, 2));

    auto trades = book.addOrder(MakeOrder(3, Side::BUY, 100.0, 20, 3));
    ASSERT_EQ(trades.size(), 2u);

    EXPECT_EQ(book.getBestAsk(), nullptr);
    EXPECT_EQ(book.getBestBid(), nullptr);   // taker fully filled, nothing rests
}

// -------------------- LIMIT orders rest if not filled, MARKET orders don't rest --------------------
TEST(OrderTypeTest, LimitOrderRestsWhenNotFilled) {
    OrderBook book("AAPL");

    auto buy = std::make_shared<Order>(
        1,
        "AAPL",
        Side::BUY,
        to_fixed(150.00),
        100,
        1000,
        OrderType::LIMIT
    );

    auto trades = book.addOrder(buy);

    EXPECT_EQ(trades.size(), 0);

    auto bestBid = book.getBestBid();
    ASSERT_NE(bestBid, nullptr);
    EXPECT_EQ(bestBid->getPrice(), to_fixed(150.00));
}

TEST(OrderTypeTest, MarketOrderDoesNotRestWhenUnfilled) {
    OrderBook book("AAPL");

    auto buy = std::make_shared<Order>(
        1,
        "AAPL",
        Side::BUY,
        0,
        100,
        1000,
        OrderType::MARKET
    );

    auto trades = book.addOrder(buy);

    EXPECT_EQ(trades.size(), 0);
    EXPECT_EQ(book.getBestBid(), nullptr);
}


// ---------- IOC orders should also not rest when unfilled ---------  
TEST(OrderTypeTest, IOCOrderDoesNotRestWhenUnfilled) {
    OrderBook book("AAPL");

    auto buy = std::make_shared<Order>(
        1,
        "AAPL",
        Side::BUY,
        to_fixed(150.00),
        100,
        1000,
        OrderType::IOC
    );

    auto trades = book.addOrder(buy);

    EXPECT_EQ(trades.size(), 0);
    EXPECT_EQ(book.getBestBid(), nullptr);
}


// ---------- MARKET orders should match at any price, even if it means sweeping through multiple levels ---------
TEST(OrderTypeTest, MarketBuySweepsMultipleAskLevels) {
    OrderBook book("AAPL");

    auto sell1 = std::make_shared<Order>(
        1, "AAPL", Side::SELL, to_fixed(150.00), 50, 1000, OrderType::LIMIT
    );

    auto sell2 = std::make_shared<Order>(
        2, "AAPL", Side::SELL, to_fixed(151.00), 50, 1001, OrderType::LIMIT
    );

    book.addOrder(sell1);
    book.addOrder(sell2);

    auto marketBuy = std::make_shared<Order>(
        3, "AAPL", Side::BUY, 0, 100, 1002, OrderType::MARKET
    );

    auto trades = book.addOrder(marketBuy);

    EXPECT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].quantity, 50);
    EXPECT_EQ(trades[1].quantity, 50);
}

// --------------------- IOC orders should match at limit price or better, but not worse -----------------
TEST(OrderTypeTest, IOCBuyRespectsLimitPrice) {
    OrderBook book("AAPL");

    auto sell1 = std::make_shared<Order>(
        1, "AAPL", Side::SELL, to_fixed(150.00), 50, 1000, OrderType::LIMIT
    );

    auto sell2 = std::make_shared<Order>(
        2, "AAPL", Side::SELL, to_fixed(152.00), 50, 1001, OrderType::LIMIT
    );

    book.addOrder(sell1);
    book.addOrder(sell2);

    auto iocBuy = std::make_shared<Order>(
        3, "AAPL", Side::BUY, to_fixed(151.00), 100, 1002, OrderType::IOC
    );

    auto trades = book.addOrder(iocBuy);
    auto bestAsk = book.getBestAsk();

    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 50);
    ASSERT_NE(bestAsk, nullptr);

    // The 152 sell order should still be resting.
    EXPECT_EQ(bestAsk->getPrice(), to_fixed(152.00));
}