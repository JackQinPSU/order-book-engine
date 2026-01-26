#include <gtest/gtest.h>
#include "event.h"

TEST(EventTest, CreateNewOrderEvent) {
    int64_t ts = getCurrentTimestamp();
    NewOrderEvent event(1, "AAPL", "BUY", 150.50, 100, ts);
    
    EXPECT_EQ(event.getOrderId(), 1);
    EXPECT_EQ(event.getSymbol(), "AAPL");
    EXPECT_EQ(event.getSide(), "BUY");
    EXPECT_DOUBLE_EQ(event.getPrice(), 150.50);
    EXPECT_EQ(event.getQuantity(), 100);
    EXPECT_EQ(event.getTimestamp(), ts);
}

TEST(EventTest, CreateCancelEvent) {
    int64_t ts = getCurrentTimestamp();
    CancelOrderEvent event(42, ts);
    
    EXPECT_EQ(event.getOrderId(), 42);
    EXPECT_EQ(event.getTimestamp(), ts);
}