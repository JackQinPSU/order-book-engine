#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

#include "book_side.h"
#include "order.h"
#include "types.h"

// ---- Helpers ----

static std::shared_ptr<Order> MakeOrder(
   int64_t id,
   Side side,
   double price,
   int qty,
   int64_t ts
){
    return std::make_shared<Order>(id, "AAPL", side, price, qty, ts);
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
    EXPECT_DOUBLE_EQ(*best, 101.0);
}

TEST_F(BookSideTest, Asks_BestPriceIsLowest) {
    asks.addOrder(MakeOrder(1, Side::SELL, 100.0, 10, 1));
    asks.addOrder(MakeOrder(2, Side::SELL,  98.0, 10, 2));
    asks.addOrder(MakeOrder(3, Side::SELL, 101.0, 10, 3));

    auto best = asks.getBestPrice();
    ASSERT_TRUE(best.has_value());
    EXPECT_DOUBLE_EQ(*best, 98.0);
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
    EXPECT_DOUBLE_EQ(*best0, 101.0);

    bids.removeOrder(2); // remove only 101 level

    auto best1 = bids.getBestPrice();
    ASSERT_TRUE(best1.has_value());
    EXPECT_DOUBLE_EQ(*best1, 100.0);
}

// -------------------- Filled orders are skipped --------------------

TEST_F(BookSideTest, GetBestOrder_SkipsFilledOrdersAtBestLevel) {
    auto o1 = MakeOrder(1, Side::BUY, 100.0, 10, 100);
    auto o2 = MakeOrder(2, Side::BUY, 100.0, 10, 101);

    bids.addOrder(o1);
    bids.addOrder(o2);

    // Fully fill the older order
    o1->fill(10);
    ASSERT_TRUE(o1->isFilled());

    auto best = bids.getBestOrder();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->getOrderId(), 2);
}

TEST_F(BookSideTest, GetBestOrder_AllOrdersFilledAtBestLevel_ReturnsNullptr) {
    // This matches your earlier getBestOrder() logic: it checks best price level only
    auto o1 = MakeOrder(1, Side::BUY, 101.0, 10, 100);
    auto o2 = MakeOrder(2, Side::BUY, 101.0, 10, 101);
    bids.addOrder(o1);
    bids.addOrder(o2);

    o1->fill(10);
    o2->fill(10);

    auto best = bids.getBestOrder();
    EXPECT_EQ(best, nullptr);
}