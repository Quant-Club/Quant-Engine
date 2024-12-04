#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "exchange/base_exchange.hpp"

using namespace quant_hub;
using namespace testing;

// Mock class for testing abstract BaseExchange
class MockExchange : public exchange::BaseExchange {
public:
    MockExchange() : BaseExchange("MockExchange", "test_key", "test_secret") {}

    MOCK_METHOD(void, loadConfig, (), (override));
    MOCK_METHOD(std::string, getRestEndpoint, (), (const, override));
    MOCK_METHOD(std::string, getWsEndpoint, (), (const, override));
    MOCK_METHOD(std::string, getMarketDataEndpoint, (const std::string&), (const, override));
    MOCK_METHOD(std::string, getOrderEndpoint, (), (const, override));
    MOCK_METHOD(std::string, getBalanceEndpoint, (), (const, override));
    MOCK_METHOD(std::string, createSubscriptionMessage, (const std::string&), (override));
    MOCK_METHOD(std::string, createUnsubscriptionMessage, (const std::string&), (override));
    MOCK_METHOD(void, handleMarketDataMessage, (const std::string&), (override));
    MOCK_METHOD(void, handleOrderUpdateMessage, (const std::string&), (override));
    MOCK_METHOD(void, handleTradeUpdateMessage, (const std::string&), (override));
};

class BaseExchangeTest : public Test {
protected:
    void SetUp() override {
        exchange = std::make_unique<MockExchange>();
        
        // Set up default expectations
        EXPECT_CALL(*exchange, getRestEndpoint())
            .WillRepeatedly(Return("https://api.mock.com"));
        EXPECT_CALL(*exchange, getWsEndpoint())
            .WillRepeatedly(Return("wss://ws.mock.com"));
        EXPECT_CALL(*exchange, getMarketDataEndpoint(_))
            .WillRepeatedly(Return("/api/v1/ticker"));
        EXPECT_CALL(*exchange, getOrderEndpoint())
            .WillRepeatedly(Return("/api/v1/order"));
        EXPECT_CALL(*exchange, getBalanceEndpoint())
            .WillRepeatedly(Return("/api/v1/account"));
    }

    std::unique_ptr<MockExchange> exchange;
};

// Market Data Tests
TEST_F(BaseExchangeTest, SubscribeMarketData) {
    const std::string symbol = "BTC-USDT";
    
    EXPECT_CALL(*exchange, createSubscriptionMessage(symbol))
        .WillOnce(Return(R"({"type":"subscribe","symbol":"BTC-USDT"})"));
    
    exchange->subscribeMarketData(symbol);
}

TEST_F(BaseExchangeTest, UnsubscribeMarketData) {
    const std::string symbol = "BTC-USDT";
    
    EXPECT_CALL(*exchange, createUnsubscriptionMessage(symbol))
        .WillOnce(Return(R"({"type":"unsubscribe","symbol":"BTC-USDT"})"));
    
    exchange->unsubscribeMarketData(symbol);
}

// Order Tests
TEST_F(BaseExchangeTest, PlaceOrder) {
    Order order;
    order.symbol = "BTC-USDT";
    order.side = OrderSide::BUY;
    order.type = OrderType::LIMIT;
    order.price = 50000.0;
    order.volume = 1.0;
    
    EXPECT_NO_THROW(exchange->placeOrder(order));
}

TEST_F(BaseExchangeTest, CancelOrder) {
    const std::string orderId = "test_order_123";
    EXPECT_NO_THROW(exchange->cancelOrder(orderId));
}

TEST_F(BaseExchangeTest, GetOrderStatus) {
    const std::string orderId = "test_order_123";
    EXPECT_NO_THROW(exchange->getOrderStatus(orderId));
}

// Account Tests
TEST_F(BaseExchangeTest, GetBalance) {
    const std::string asset = "BTC";
    EXPECT_NO_THROW(exchange->getBalance(asset));
}

TEST_F(BaseExchangeTest, GetPositions) {
    EXPECT_NO_THROW(exchange->getPositions());
}

// Callback Tests
TEST_F(BaseExchangeTest, MarketDataCallback) {
    bool callbackCalled = false;
    MarketData testData;
    
    exchange->setMarketDataCallback([&](const MarketData& data) {
        callbackCalled = true;
        EXPECT_EQ(data.symbol, "BTC-USDT");
    });
    
    testData.symbol = "BTC-USDT";
    exchange->handleMarketDataMessage(R"({"symbol":"BTC-USDT","price":50000})");
    
    EXPECT_TRUE(callbackCalled);
}

TEST_F(BaseExchangeTest, OrderUpdateCallback) {
    bool callbackCalled = false;
    OrderUpdate testUpdate;
    
    exchange->setOrderUpdateCallback([&](const OrderUpdate& update) {
        callbackCalled = true;
        EXPECT_EQ(update.orderId, "test_order_123");
    });
    
    testUpdate.orderId = "test_order_123";
    exchange->handleOrderUpdateMessage(R"({"orderId":"test_order_123","status":"FILLED"})");
    
    EXPECT_TRUE(callbackCalled);
}

TEST_F(BaseExchangeTest, TradeUpdateCallback) {
    bool callbackCalled = false;
    TradeUpdate testUpdate;
    
    exchange->setTradeUpdateCallback([&](const TradeUpdate& update) {
        callbackCalled = true;
        EXPECT_EQ(update.tradeId, "test_trade_123");
    });
    
    testUpdate.tradeId = "test_trade_123";
    exchange->handleTradeUpdateMessage(R"({"tradeId":"test_trade_123","price":50000})");
    
    EXPECT_TRUE(callbackCalled);
}

// Error Handling Tests
TEST_F(BaseExchangeTest, InvalidOrderParameters) {
    Order order;
    // Missing required fields
    EXPECT_THROW(exchange->placeOrder(order), std::invalid_argument);
}

TEST_F(BaseExchangeTest, InvalidOrderId) {
    EXPECT_THROW(exchange->cancelOrder(""), std::invalid_argument);
}

TEST_F(BaseExchangeTest, WebSocketReconnection) {
    // Simulate websocket disconnection and reconnection
    exchange->subscribeMarketData("BTC-USDT");
    // Force reconnection
    exchange->unsubscribeMarketData("BTC-USDT");
    exchange->subscribeMarketData("BTC-USDT");
}

// Performance Tests
TEST_F(BaseExchangeTest, OrderThroughput) {
    const int numOrders = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    Order order;
    order.symbol = "BTC-USDT";
    order.side = OrderSide::BUY;
    order.type = OrderType::LIMIT;
    order.price = 50000.0;
    order.volume = 1.0;
    
    for (int i = 0; i < numOrders; ++i) {
        exchange->placeOrder(order);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Expect processing time less than 1ms per order on average
    EXPECT_LT(duration.count() / numOrders, 1);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
