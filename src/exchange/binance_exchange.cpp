#include "exchange/binance_exchange.hpp"
#include "common/config.hpp"
#include "common/logger.hpp"
#include <sstream>
#include <iomanip>

namespace quant_hub {
namespace exchange {

BinanceExchange::BinanceExchange(const std::string& apiKey, const std::string& secretKey)
    : BaseExchange("Binance", apiKey, secretKey)
{
    loadConfig();
    restClient_ = std::make_unique<HttpClient>(restEndpoint_, "443");
}

void BinanceExchange::loadConfig() {
    auto& config = Config::getInstance();
    restEndpoint_ = config.get<std::string>("exchanges.binance.rest_endpoint", "api.binance.com");
    wsEndpoint_ = config.get<std::string>("exchanges.binance.ws_endpoint", "stream.binance.com");
}

std::string BinanceExchange::getRestEndpoint() const {
    return restEndpoint_;
}

std::string BinanceExchange::getWsEndpoint() const {
    return wsEndpoint_;
}

std::string BinanceExchange::getMarketDataEndpoint(const std::string& symbol) const {
    return "/api/v3/ticker/bookTicker?symbol=" + symbol;
}

std::string BinanceExchange::getOrderEndpoint() const {
    return "/api/v3/order";
}

std::string BinanceExchange::getBalanceEndpoint() const {
    return "/api/v3/account";
}

std::string BinanceExchange::createSubscriptionMessage(const std::string& symbol) {
    std::stringstream ss;
    ss << R"({"method": "SUBSCRIBE","params": [")" << symbol.lower() 
       << "@bookTicker\"],"id": " << std::time(nullptr) << "}";
    return ss.str();
}

std::string BinanceExchange::createUnsubscriptionMessage(const std::string& symbol) {
    std::stringstream ss;
    ss << R"({"method": "UNSUBSCRIBE","params": [")" << symbol.lower() 
       << "@bookTicker\"],"id": " << std::time(nullptr) << "}";
    return ss.str();
}

void BinanceExchange::handleMarketDataMessage(const std::string& message) {
    try {
        auto marketData = parseMarketData(message);
        if (marketDataCallback_) {
            marketDataCallback_(marketData);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to handle market data message: ", e.what());
    }
}

void BinanceExchange::handleOrderUpdateMessage(const std::string& message) {
    try {
        auto orderUpdate = parseOrderUpdate(message);
        if (orderUpdateCallback_) {
            orderUpdateCallback_(orderUpdate);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to handle order update message: ", e.what());
    }
}

void BinanceExchange::handleTradeUpdateMessage(const std::string& message) {
    try {
        auto tradeUpdate = parseTradeUpdate(message);
        if (tradeUpdateCallback_) {
            tradeUpdateCallback_(tradeUpdate);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to handle trade update message: ", e.what());
    }
}

std::map<std::string, std::string> BinanceExchange::createAuthHeaders() const {
    std::string timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count());
    
    std::string signature = createSignature(timestamp);
    
    return {
        {"X-MBX-APIKEY", apiKey_},
        {"signature", signature},
        {"timestamp", timestamp}
    };
}

std::string BinanceExchange::createSignature(const std::string& data) const {
    unsigned char* digest = HMAC(EVP_sha256(),
                                secretKey_.c_str(), secretKey_.length(),
                                (unsigned char*)data.c_str(), data.length(),
                                nullptr, nullptr);
    
    std::stringstream ss;
    for(int i = 0; i < 32; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return ss.str();
}

MarketData BinanceExchange::parseMarketData(const std::string& message) const {
    MarketData data;
    auto json = simdjson::padded_string(message);
    auto doc = jsonParser_.iterate(json);
    
    data.symbol = std::string(doc["s"].get_string().value());
    data.bidPrice = doc["b"].get_double().value();
    data.askPrice = doc["a"].get_double().value();
    data.bidSize = doc["B"].get_double().value();
    data.askSize = doc["A"].get_double().value();
    data.timestamp = doc["E"].get_uint64().value();
    
    return data;
}

OrderUpdate BinanceExchange::parseOrderUpdate(const std::string& message) const {
    OrderUpdate update;
    auto json = simdjson::padded_string(message);
    auto doc = jsonParser_.iterate(json);
    
    update.orderId = std::string(doc["i"].get_string().value());
    update.symbol = std::string(doc["s"].get_string().value());
    update.side = static_cast<OrderSide>(doc["S"].get_string().value() == "BUY" ? 1 : 2);
    update.status = static_cast<OrderStatus>(doc["X"].get_string().value_unsafe()[0]);
    update.filledVolume = doc["z"].get_double().value();
    update.remainingVolume = doc["q"].get_double().value() - update.filledVolume;
    update.price = doc["p"].get_double().value();
    update.timestamp = doc["E"].get_uint64().value();
    
    return update;
}

TradeUpdate BinanceExchange::parseTradeUpdate(const std::string& message) const {
    TradeUpdate update;
    auto json = simdjson::padded_string(message);
    auto doc = jsonParser_.iterate(json);
    
    update.tradeId = std::string(doc["t"].get_string().value());
    update.orderId = std::string(doc["i"].get_string().value());
    update.symbol = std::string(doc["s"].get_string().value());
    update.side = static_cast<TradeSide>(doc["S"].get_string().value() == "BUY" ? 1 : 2);
    update.price = doc["p"].get_double().value();
    update.volume = doc["q"].get_double().value();
    update.timestamp = doc["E"].get_uint64().value();
    
    return update;
}

Balance BinanceExchange::parseBalance(const std::string& message) const {
    Balance balance;
    auto json = simdjson::padded_string(message);
    auto doc = jsonParser_.iterate(json);
    
    for (auto asset : doc["balances"]) {
        std::string assetName = std::string(asset["asset"].get_string().value());
        double free = asset["free"].get_double().value();
        double locked = asset["locked"].get_double().value();
        
        balance.assets[assetName] = {free, locked};
    }
    
    return balance;
}

std::vector<Position> BinanceExchange::parsePositions(const std::string& message) const {
    std::vector<Position> positions;
    auto json = simdjson::padded_string(message);
    auto doc = jsonParser_.iterate(json);
    
    for (auto pos : doc["positions"]) {
        Position position;
        position.symbol = std::string(pos["symbol"].get_string().value());
        position.volume = pos["positionAmt"].get_double().value();
        position.entryPrice = pos["entryPrice"].get_double().value();
        position.unrealizedPnl = pos["unrealizedProfit"].get_double().value();
        position.leverage = pos["leverage"].get_int64().value();
        
        positions.push_back(position);
    }
    
    return positions;
}

} // namespace exchange
} // namespace quant_hub
