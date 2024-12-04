#include "exchange/base_exchange.hpp"

namespace quant_hub {
namespace exchange {

BaseExchange::BaseExchange(const std::string& name,
                         const std::string& apiKey,
                         const std::string& secretKey)
    : name_(name)
    , apiKey_(apiKey)
    , secretKey_(secretKey)
{
    loadConfig();
}

BaseExchange::~BaseExchange() {
    if (marketDataWs_) {
        marketDataWs_->stop();
    }
    if (tradingWs_) {
        tradingWs_->stop();
    }
}

void BaseExchange::subscribeMarketData(const std::string& symbol) {
    if (!marketDataWs_) {
        initializeMarketDataWs();
    }
    
    std::string sub = createSubscriptionMessage(symbol);
    marketDataWs_->send(sub);
    subscriptions_[symbol] = sub;
    LOG_INFO("Subscribed to market data for ", symbol);
}

void BaseExchange::unsubscribeMarketData(const std::string& symbol) {
    if (!marketDataWs_) return;
    
    auto it = subscriptions_.find(symbol);
    if (it != subscriptions_.end()) {
        std::string unsub = createUnsubscriptionMessage(symbol);
        marketDataWs_->send(unsub);
        subscriptions_.erase(it);
        LOG_INFO("Unsubscribed from market data for ", symbol);
    }
}

OrderId BaseExchange::placeOrder(const Order& order) {
    std::string endpoint = getOrderEndpoint();
    std::string requestBody = serializeOrder(order);
    
    try {
        auto response = restClient_->request(
            HttpMethod::POST,
            endpoint,
            requestBody,
            createAuthHeaders()
        );
        
        return parseOrderResponse(response.body());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to place order: ", e.what());
        throw;
    }
}

void BaseExchange::cancelOrder(const OrderId& orderId) {
    std::string endpoint = getOrderEndpoint() + "/" + orderId;
    
    try {
        restClient_->request(
            HttpMethod::DELETE,
            endpoint,
            "",
            createAuthHeaders()
        );
        LOG_INFO("Order cancelled: ", orderId);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to cancel order: ", e.what());
        throw;
    }
}

OrderStatus BaseExchange::getOrderStatus(const OrderId& orderId) {
    std::string endpoint = getOrderEndpoint() + "/" + orderId;
    
    try {
        auto response = restClient_->request(
            HttpMethod::GET,
            endpoint,
            "",
            createAuthHeaders()
        );
        
        return parseOrderStatus(response.body());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get order status: ", e.what());
        throw;
    }
}

Balance BaseExchange::getBalance(const std::string& asset) {
    std::string endpoint = getBalanceEndpoint();
    
    try {
        auto response = restClient_->request(
            HttpMethod::GET,
            endpoint,
            "",
            createAuthHeaders()
        );
        
        return parseBalance(response.body());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get balance: ", e.what());
        throw;
    }
}

std::vector<Position> BaseExchange::getPositions() {
    std::string endpoint = getPositionsEndpoint();
    
    try {
        auto response = restClient_->request(
            HttpMethod::GET,
            endpoint,
            "",
            createAuthHeaders()
        );
        
        return parsePositions(response.body());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get positions: ", e.what());
        throw;
    }
}

void BaseExchange::setMarketDataCallback(MarketDataCallback callback) {
    marketDataCallback_ = std::move(callback);
}

void BaseExchange::setOrderUpdateCallback(OrderUpdateCallback callback) {
    orderUpdateCallback_ = std::move(callback);
}

void BaseExchange::setTradeUpdateCallback(TradeUpdateCallback callback) {
    tradeUpdateCallback_ = std::move(callback);
}

void BaseExchange::initializeMarketDataWs() {
    marketDataWs_ = std::make_unique<WebsocketClient>(
        getWsEndpoint(),
        "443",
        "/ws/market",
        true
    );

    marketDataWs_->connect(
        [this](const std::string& msg) { handleMarketDataMessage(msg); },
        [](const std::string& err) { LOG_ERROR("Market data WS error: ", err); }
    );
}

void BaseExchange::initializeTradingWs() {
    tradingWs_ = std::make_unique<WebsocketClient>(
        getWsEndpoint(),
        "443",
        "/ws/trading",
        true
    );

    tradingWs_->connect(
        [this](const std::string& msg) {
            handleOrderUpdateMessage(msg);
            handleTradeUpdateMessage(msg);
        },
        [](const std::string& err) { LOG_ERROR("Trading WS error: ", err); }
    );
}

} // namespace exchange
} // namespace quant_hub
