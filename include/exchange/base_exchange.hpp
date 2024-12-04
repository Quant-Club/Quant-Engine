#pragma once

#include <string>
#include <memory>
#include <map>
#include <functional>
#include "exchange_interface.hpp"
#include "http_client.hpp"
#include "websocket_client.hpp"
#include "common/logger.hpp"
#include "common/config.hpp"

namespace quant_hub {
namespace exchange {

class BaseExchange : public ExchangeInterface {
public:
    BaseExchange(const std::string& name,
                const std::string& apiKey,
                const std::string& secretKey);

    virtual ~BaseExchange();

    // Market Data
    void subscribeMarketData(const std::string& symbol) override;
    void unsubscribeMarketData(const std::string& symbol) override;
    
    // Trading
    OrderId placeOrder(const Order& order) override;
    void cancelOrder(const OrderId& orderId) override;
    OrderStatus getOrderStatus(const OrderId& orderId) override;
    
    // Account
    Balance getBalance(const std::string& asset) override;
    std::vector<Position> getPositions() override;
    
    // Callbacks
    void setMarketDataCallback(MarketDataCallback callback) override;
    void setOrderUpdateCallback(OrderUpdateCallback callback) override;
    void setTradeUpdateCallback(TradeUpdateCallback callback) override;

protected:
    // Configuration
    virtual void loadConfig() = 0;
    virtual std::string getRestEndpoint() const = 0;
    virtual std::string getWsEndpoint() const = 0;
    virtual std::string getMarketDataEndpoint(const std::string& symbol) const = 0;
    virtual std::string getOrderEndpoint() const = 0;
    virtual std::string getBalanceEndpoint() const = 0;
    
    // WebSocket initialization
    virtual void initializeMarketDataWs();
    virtual void initializeTradingWs();
    
    // Message creation
    virtual std::string createSubscriptionMessage(const std::string& symbol) = 0;
    virtual std::string createUnsubscriptionMessage(const std::string& symbol) = 0;
    
    // Message handling
    virtual void handleMarketDataMessage(const std::string& message) = 0;
    virtual void handleOrderUpdateMessage(const std::string& message) = 0;
    virtual void handleTradeUpdateMessage(const std::string& message) = 0;

protected:
    std::string name_;
    std::string apiKey_;
    std::string secretKey_;
    
    std::unique_ptr<HttpClient> restClient_;
    std::unique_ptr<WebsocketClient> marketDataWs_;
    std::unique_ptr<WebsocketClient> tradingWs_;
    
    MarketDataCallback marketDataCallback_;
    OrderUpdateCallback orderUpdateCallback_;
    TradeUpdateCallback tradeUpdateCallback_;
    
    std::map<std::string, std::string> subscriptions_;
};

} // namespace exchange
} // namespace quant_hub
