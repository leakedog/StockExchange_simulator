#include <gtest/gtest.h>
#include "homework.h"

TEST(AggresiveTest, Entrance1) {
    auto order1 = std::make_shared<LimitOrder>(OrderSide::BUY_OS, 0, 99, 50000);
    auto order2 = std::make_shared<LimitOrder>(OrderSide::BUY_OS, 1, 98, 25500);
    auto order3 = std::make_shared<LimitOrder>(OrderSide::SELL_OS, 2, 100, 10000);
    auto order4 = std::make_shared<LimitOrder>(OrderSide::SELL_OS, 3, 100, 7500);
    auto order5 = std::make_shared<LimitOrder>(OrderSide::SELL_OS, 4, 101, 20000);

    MatchingSystem system;
    for (auto order : {order1, order2, order3, order4, order5}) {
        system.AddOrder(order);
    }
    
    auto iceberg_order = std::make_shared<IcebergOrder>(OrderSide::BUY_OS, 5, 100, 10000, 100000);
    system.AddOrder(iceberg_order);

    EXPECT_EQ(iceberg_order->quantity(), 10000);
    EXPECT_EQ(iceberg_order->hidden_full_volume(), 82500);
    EXPECT_EQ(order1->quantity(), 50000);
    EXPECT_EQ(order2->quantity(), 25500);
    EXPECT_EQ(order3->quantity(), 0);
    EXPECT_EQ(order4->quantity(), 0);
    EXPECT_EQ(order5->quantity(), 20000);
}

TEST(PassiveTest, Test1) {
    auto order1 = std::make_shared<LimitOrder>(OrderSide::BUY_OS, 0, 99, 50000);
    auto order2 = std::make_shared<LimitOrder>(OrderSide::BUY_OS, 1, 98, 25500);
    auto order3 = std::make_shared<LimitOrder>(OrderSide::SELL_OS, 2, 100, 10000);
    auto order4 = std::make_shared<LimitOrder>(OrderSide::SELL_OS, 3, 100, 7500);
    auto order5 = std::make_shared<LimitOrder>(OrderSide::SELL_OS, 4, 101, 20000);

    MatchingSystem system;
    for (auto order : {order1, order2, order3, order4, order5}) {
        system.AddOrder(order);
    }
    
    auto iceberg_order = std::make_shared<IcebergOrder>(OrderSide::BUY_OS, 5, 100, 10000, 100000);
    system.AddOrder(iceberg_order);
    

    auto order6 = std::make_shared<LimitOrder>(OrderSide::SELL_OS, 6, 99, 10000);

    system.AddOrder(order6);

    EXPECT_EQ(iceberg_order->quantity(), 10000);
    EXPECT_EQ(iceberg_order->hidden_full_volume(), 72500);
    EXPECT_EQ(order1->quantity(), 50000);
    EXPECT_EQ(order2->quantity(), 25500);
    EXPECT_EQ(order3->quantity(), 0);
    EXPECT_EQ(order4->quantity(), 0);
    EXPECT_EQ(order5->quantity(), 20000);
    EXPECT_EQ(order6->quantity(), 0);

    auto order7= std::make_shared<LimitOrder>(OrderSide::SELL_OS, 7, 98, 11000);
    system.AddOrder(order7);
    EXPECT_EQ(iceberg_order->quantity(), 9000);
    EXPECT_EQ(iceberg_order->hidden_full_volume(), 61500);
    EXPECT_EQ(order7->quantity(), 0);

    auto another_iceberg_order = std::make_shared<IcebergOrder>(OrderSide::BUY_OS, 8, 100, 20000, 50000);
    system.AddOrder(another_iceberg_order);

    auto order8= std::make_shared<LimitOrder>(OrderSide::SELL_OS, 9, 98, 35000);

    system.AddOrder(order8);

    EXPECT_EQ(order8->quantity(), 0);
    EXPECT_EQ(iceberg_order->quantity(), 4000);
    EXPECT_EQ(iceberg_order->hidden_full_volume(), 46500);
    EXPECT_EQ(another_iceberg_order->quantity(), 20000);
    EXPECT_EQ(another_iceberg_order->hidden_full_volume(), 30000);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(output) = "stdout";

    return RUN_ALL_TESTS();
}