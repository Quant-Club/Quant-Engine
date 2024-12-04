#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "exchange/binance_exchange.hpp"

using namespace quant_hub;
using namespace testing;

class BinanceExchangeTest : public Test {
protected:
    void SetUp() override {
        exchange = std::make_unique<exchange::BinanceExchange>("test_key", "test_secret");
    }

    // Helper function to create a sample market data JSON
    std::string createMarketDataJson() {
        return R"({
            "e": "bookTicker",
            "s": "BTCUSDT",
            "b": "50000.00",
            "B": "1.0",
            "a": "50001.00",
            "A": "1.0",
            "E": 1623456789000
        })";
    }

    // Helper function to create a sample order update JSON
    std::string createOrderUpdateJson() {
        return R"({
            "e": "executionReport",
            "s": "BTCUSDT",
            "c": "test_client_123",
            "i": "test_order_123",
            "X": "FILLED",
            "q": "1.0",
            "p": "50000.00",
            "E": 1623456789000
        })";
    }

    std::unique_ptr<exchange::BinanceExchange> exchange;
};

// Configuration Tests
TEST_F(BinanceExchangeTest, LoadConfig) {
    EXPECT_NO_THROW(exchange->loadConfig());
    EXPECT_EQ(exchange->getRestEndpoint(), "https://api.binance.com");
    EXPECT_EQ(exchange->getWsEndpoint(), "wss://stream.binance.com:9443");
}

// Market Data Tests
TEST_F(BinanceExchangeTest, MarketDataSubscription) {
    const std::string symbol = "BTCUSDT";
    
    // Test subscription message format
    std::string subMsg = exchange->createSubscriptionMessage(symbol);
    EXPECT_THAT(subMsg, HasSubstr("SUBSCRIBE"));
    EXPECT_THAT(subMsg, HasSubstr(symbol));
    
    // Test market data handling
    EXPECT_NO_THROW(exchange->handleMarketDataMessage(createMarketDataJson()));
}

TEST_F(BinanceExchangeTest, MarketDataParsing) {
    bool callbackCalled = false;
    exchange->setMarketDataCallback([&](const MarketData& data) {
        callbackCalled = true;
        EXPECT_EQ(data.symbol, "BTCUSDT");
        EXPECT_EQ(data.bidPrice, 50000.00);
        EXPECT_EQ(data.askPrice, 50001.00);
        EXPECT_EQ(data.bidSize, 1.0);
        EXPECT_EQ(data.askSize, 1.0);
        EXPECT_EQ(data.timestamp, 1623456789000);
    });
    
    exchange->handleMarketDataMessage(createMarketDataJson());
    EXPECT_TRUE(callbackCalled);
}

// Order Tests
TEST_F(BinanceExchangeTest, PlaceOrder) {
    Order order;
    order.symbol = "BTCUSDT";
    order.side = OrderSide::BUY;
    order.type = OrderType::LIMIT;
    order.price = 50000.0;
    order.volume = 1.0;
    
    EXPECT_NO_THROW(exchange->placeOrder(order));
}

TEST_F(BinanceExchangeTest, OrderUpdateParsing) {
    bool callbackCalled = false;
    exchange->setOrderUpdateCallback([&](const OrderUpdate& update) {
        callbackCalled = true;
        EXPECT_EQ(update.symbol, "BTCUSDT");
        EXPECT_EQ(update.orderId, "test_order_123");
        EXPECT_EQ(update.status, OrderStatus::FILLED);
        EXPECT_EQ(update.filledVolume, 1.0);
        EXPECT_EQ(update.price, 50000.00);
    });
    
    exchange->handleOrderUpdateMessage(createOrderUpdateJson());
    EXPECT_TRUE(callbackCalled);
}

// Authentication Tests
TEST_F(BinanceExchangeTest, SignatureGeneration) {
    std::string testData = "symbol=BTCUSDT&side=BUY&type=LIMIT&timeInForce=GTC&quantity=1&price=50000&timestamp=1623456789000";
    std::string signature = exchange->createSignature(testData);
    EXPECT_FALSE(signature.empty());
    EXPECT_EQ(signature.length(), 64); // SHA256 hex string length
}

TEST_F(BinanceExchangeTest, AuthHeaders) {
    auto headers = exchange->createAuthHeaders();
    EXPECT_TRUE(headers.find("X-MBX-APIKEY") != headers.end());
    EXPECT_EQ(headers["X-MBX-APIKEY"], "test_key");
}

// Error Handling Tests
TEST_F(BinanceExchangeTest, InvalidJson) {
    EXPECT_THROW(exchange->handleMarketDataMessage("invalid json"), std::runtime_error);
}

TEST_F(BinanceExchangeTest, InvalidSymbol) {
    Order order;
    order.symbol = "INVALID-PAIR";
    EXPECT_THROW(exchange->placeOrder(order), std::invalid_argument);
}

// Rate Limiting Tests
TEST_F(BinanceExchangeTest, OrderRateLimit) {
    const int numOrders = 10;
    const int expectedTimeMs = 1000; // 1 second
    
    Order order;
    order.symbol = "BTCUSDT";
    order.side = OrderSide::BUY;
    order.type = OrderType::LIMIT;
    order.price = 50000.0;
    order.volume = 1.0;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numOrders; ++i) {
        exchange->placeOrder(order);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Ensure rate limiting is working
    EXPECT_GE(duration.count(), expectedTimeMs);
}

// Websocket Tests
TEST_F(BinanceExchangeTest, WebsocketReconnection) {
    const std::string symbol = "BTCUSDT";
    
    // Initial subscription
    exchange->subscribeMarketData(symbol);
    
    // Simulate disconnection
    exchange->handleWebsocketDisconnection();
    
    // Verify automatic resubscription
    bool resubscribed = false;
    exchange->setMarketDataCallback([&](const MarketData& data) {
        if (data.symbol == symbol) {
            resubscribed = true;
        }
    });
    
    exchange->handleMarketDataMessage(createMarketDataJson());
    EXPECT_TRUE(resubscribed);
}

// Performance Tests
TEST_F(BinanceExchangeTest, MarketDataProcessing) {
    const int numUpdates = 10000;
    const std::string marketDataJson = createMarketDataJson();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numUpdates; ++i) {
        exchange->handleMarketDataMessage(marketDataJson);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Expect processing time less than 10 microseconds per update
    double avgProcessingTime = static_cast<double>(duration.count()) / numUpdates;
    EXPECT_LT(avgProcessingTime, 10.0);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
