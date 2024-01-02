#include <gtest/gtest.h>
#include "homework.h"
#include <memory>
#include <sstream>
#include <streambuf>

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
    
    auto iceberg_order = std::make_shared<IcebergOrder>(OrderSide::BUY_OS, 5, 100, 100000, 10000);
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
    
    auto iceberg_order = std::make_shared<IcebergOrder>(OrderSide::BUY_OS, 5, 100, 100000, 10000);
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

    auto another_iceberg_order = std::make_shared<IcebergOrder>(OrderSide::BUY_OS, 8, 100, 50000, 20000);
    system.AddOrder(another_iceberg_order);

    auto order8= std::make_shared<LimitOrder>(OrderSide::SELL_OS, 9, 98, 35000);

    system.AddOrder(order8);

    EXPECT_EQ(order8->quantity(), 0);
    EXPECT_EQ(iceberg_order->quantity(), 4000);
    EXPECT_EQ(iceberg_order->hidden_full_volume(), 46500);
    EXPECT_EQ(another_iceberg_order->quantity(), 20000);
    EXPECT_EQ(another_iceberg_order->hidden_full_volume(), 30000);
}

TEST(FormattingTest, OrderBookTest) {

    auto order1 = std::make_shared<LimitOrder>(OrderSide::BUY_OS, 1234567890, 32503, 1234567890);
    auto order2 = std::make_shared<LimitOrder>(OrderSide::BUY_OS, 1138, 31502, 7500);
    auto order3 = std::make_shared<LimitOrder>(OrderSide::SELL_OS, 1234567891, 32504, 1234567890);
    auto order4 = std::make_shared<LimitOrder>(OrderSide::SELL_OS, 6808, 32505, 7777);
    auto order5 = std::make_shared<LimitOrder>(OrderSide::SELL_OS, 42100, 32507, 3000);

    OrderBook order_book;
    for (auto order : {order1, order2, order3, order4, order5}) {
        order_book.Add(order);
    }

    


    std::ostringstream expected_result;
    expected_result << "+-----------------------------------------------------------------+\n";
    expected_result << "| BUY                            | SELL                           |\n";
    expected_result << "| Id       | Volume      | Price | Price | Volume      | Id       |\n";
    expected_result << "+----------+-------------+-------+-------+-------------+----------+\n";
    expected_result << "|1234567890|1,234,567,890| 32,503| 32,504|1,234,567,890|1234567891|\n";
    expected_result << "|      1138|        7,500| 31,502| 32,505|        7,777|      6808|\n";
    expected_result << "|          |             |       | 32,507|        3,000|     42100|\n";
    expected_result << "+-----------------------------------------------------------------+\n";

    EXPECT_EQ(order_book.ToString(), expected_result.str());
}

TEST(FormattingTest, TradeTest) {
    Trade trade{.buy_id = 100322, .sell_id = 100345, .price = 5103, .quantity = 7500};
    std::ostringstream expected_result;
    expected_result << "100322,100345,5103,7500";
    EXPECT_EQ(trade.ToString(), expected_result.str());
}

TEST(FormattingTest, LimitOrderTest) {
    std::string line = "#belive me or not i am buying B,100322,5103,7500";
    EXPECT_EQ(OrderParser::Parse(line), std::nullopt);
    line = "B,100322,5103,7500";
    auto order_or_status = OrderParser::Parse(line);
    EXPECT_NE(order_or_status, std::nullopt);
    std::shared_ptr<LimitOrder> limit_order(order_or_status.value().release());
    EXPECT_EQ(limit_order->side(), BUY_OS);
    EXPECT_EQ(limit_order->id(), 100322);
    EXPECT_EQ(limit_order->price(), 5103);
    EXPECT_EQ(limit_order->quantity(), 7500);
}

TEST(FormattingTest, IcebergOrderTest) {
    std::string line = "#belive me or not i am selling S,100345,5103,100000,10000";
    EXPECT_EQ(OrderParser::Parse(line), std::nullopt);
    line = "S,100345,5103,100000,10000";
    auto order_or_status = OrderParser::Parse(line);
    EXPECT_NE(order_or_status, std::nullopt);
    std::shared_ptr<IcebergOrder> iceberg_order(static_cast<IcebergOrder*>(order_or_status.value().release()));
    EXPECT_EQ(iceberg_order->side(), SELL_OS);
    EXPECT_EQ(iceberg_order->id(), 100345);
    EXPECT_EQ(iceberg_order->price(), 5103);
    EXPECT_EQ(iceberg_order->hidden_full_volume(), 100000);
    EXPECT_EQ(iceberg_order->peak_size(), 10000);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::GTEST_FLAG(output) = "stdout";

    return RUN_ALL_TESTS();
}