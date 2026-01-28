#include <gtest/gtest.h>
#include "order.h"
#include "event.h"

// Test fixture for Order class
class OrderTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Run before each test
        }
};

TEST_F(OrderTest, CreateOrder) {
    Order order(1, "AAPL", "BUY", 150.50, 100, getCurrentTimestamp());

    EXPECT_EQ(order.getRemaining(), 100);
    EXPECT_FALSE(order.isFilled());
}

TEST_F(OrderTest, PartialFill) {
    Order order(1, "MSFT", "BUY", 150.50, 100, getCurrentTimestamp());
    
    order.fill(30);
    
    EXPECT_EQ(order.getRemaining(), 70);
    EXPECT_FALSE(order.isFilled());
}

TEST_F(OrderTest, CompleteFill) {
    Order order(1, "META","BUY", 150.50, 100, getCurrentTimestamp());
    
    order.fill(100);
    
    EXPECT_EQ(order.getRemaining(), 0);
    EXPECT_TRUE(order.isFilled());
}

TEST_F(OrderTest, MultipleFills) {
    Order order(1, "TSLA", "SELL", 151.00, 100, getCurrentTimestamp());
    
    order.fill(30);
    order.fill(50);
    order.fill(20);
    
    EXPECT_TRUE(order.isFilled());
}