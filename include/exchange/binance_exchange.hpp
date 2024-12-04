#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include "base_exchange.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace quant_hub {
namespace exchange {

class BinanceExchange : public BaseExchange {
public:
    BinanceExchange(const std::string& apiKey, const std::string& secretKey)
        : BaseExchange("Binance", apiKey, secretKey)
    {
        loadConfig();
        restClient_ = std::make_unique<HttpClient>("api.binance.com", "443");
    }

protected:
    void loadConfig() override {
        auto& config = Config::getInstance();
        restEndpoint_ = config.get<std::string>("exchanges.binance.rest_endpoint", "https://api.binance.com");
        wsEndpoint_ = config.get<std::string>("exchanges.binance.ws_endpoint", "stream.binance.com");
    }

    std::string getRestEndpoint() const override {
        return restEndpoint_;
    }

    std::string getWsEndpoint() const override {
        return wsEndpoint_;
    }

    std::string getMarketDataEndpoint(const std::string& symbol) const override {
        return "/api/v3/ticker/bookTicker?symbol=" + symbol;
    }

    std::string getOrderEndpoint() const override {
        return "/api/v3/order";
    }

    std::string getBalanceEndpoint() const override {
        return "/api/v3/account";
    }

    std::string getPositionsEndpoint() const override {
        return "/api/v3/openOrders";
    }

    std::string createSubscriptionMessage(const std::string& symbol) const override {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();
        writer.Key("method");
        writer.String("SUBSCRIBE");
        writer.Key("params");
        writer.StartArray();
        writer.String((symbol + "@bookTicker").c_str());
        writer.EndArray();
        writer.Key("id");
        writer.Int(1);
        writer.EndObject();

        return buffer.GetString();
    }

    std::string createUnsubscriptionMessage(const std::string& symbol) const override {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();
        writer.Key("method");
        writer.String("UNSUBSCRIBE");
        writer.Key("params");
        writer.StartArray();
        writer.String((symbol + "@bookTicker").c_str());
        writer.EndArray();
        writer.Key("id");
        writer.Int(1);
        writer.EndObject();

        return buffer.GetString();
    }

    void handleMarketDataMessage(const std::string& message) override {
        rapidjson::Document d;
        d.Parse(message.c_str());

        if (d.HasParseError() || !d.IsObject()) {
            LOG_ERROR("Failed to parse market data message: ", message);
            return;
        }

        MarketData data;
        data.symbol = d["s"].GetString();
        data.timestamp = std::chrono::system_clock::now();
        data.bestBid = std::stod(d["b"].GetString());
        data.bestAsk = std::stod(d["a"].GetString());
        data.bidVolume = std::stod(d["B"].GetString());
        data.askVolume = std::stod(d["A"].GetString());

        // Notify subscribers
        if (marketDataHandler_) {
            marketDataHandler_(data);
        }
    }

    void handleTradingMessage(const std::string& message) override {
        rapidjson::Document d;
        d.Parse(message.c_str());

        if (d.HasParseError() || !d.IsObject()) {
            LOG_ERROR("Failed to parse trading message: ", message);
            return;
        }

        if (d.HasMember("e")) {
            std::string eventType = d["e"].GetString();
            
            if (eventType == "executionReport") {
                OrderUpdate update;
                update.orderId = d["i"].GetString();
                update.status = parseOrderStatus(d["X"].GetString());
                update.filledVolume = std::stod(d["z"].GetString());
                update.filledPrice = std::stod(d["L"].GetString());
                
                if (orderUpdateHandler_) {
                    orderUpdateHandler_(update);
                }
            }
        }
    }

    std::map<std::string, std::string> createAuthHeaders() const override {
        std::string timestamp = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );

        std::string signature = createSignature(timestamp);

        return {
            {"X-MBX-APIKEY", apiKey_},
            {"timestamp", timestamp},
            {"signature", signature}
        };
    }

    std::string serializeOrder(const Order& order) const override {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();
        writer.Key("symbol");
        writer.String(order.symbol.c_str());
        writer.Key("side");
        writer.String(order.side == OrderSide::BUY ? "BUY" : "SELL");
        writer.Key("type");
        writer.String(orderTypeToString(order.type));
        writer.Key("quantity");
        writer.Double(order.volume);
        
        if (order.type == OrderType::LIMIT) {
            writer.Key("price");
            writer.Double(order.price);
            writer.Key("timeInForce");
            writer.String("GTC");
        }
        
        writer.EndObject();
        return buffer.GetString();
    }

    OrderId parseOrderResponse(const std::string& response) const override {
        rapidjson::Document d;
        d.Parse(response.c_str());

        if (d.HasParseError() || !d.HasMember("orderId")) {
            throw std::runtime_error("Invalid order response: " + response);
        }

        return std::to_string(d["orderId"].GetInt64());
    }

    MarketData parseMarketData(const std::string& response) const override {
        rapidjson::Document d;
        d.Parse(response.c_str());

        if (d.HasParseError() || !d.IsObject()) {
            throw std::runtime_error("Invalid market data response: " + response);
        }

        MarketData data;
        data.symbol = d["symbol"].GetString();
        data.timestamp = std::chrono::system_clock::now();
        data.bestBid = std::stod(d["bidPrice"].GetString());
        data.bestAsk = std::stod(d["askPrice"].GetString());
        data.bidVolume = std::stod(d["bidQty"].GetString());
        data.askVolume = std::stod(d["askQty"].GetString());

        return data;
    }

    OrderStatus parseOrderStatus(const std::string& status) const override {
        if (status == "NEW") return OrderStatus::PENDING;
        if (status == "PARTIALLY_FILLED") return OrderStatus::PARTIAL;
        if (status == "FILLED") return OrderStatus::FILLED;
        if (status == "CANCELED") return OrderStatus::CANCELLED;
        if (status == "REJECTED") return OrderStatus::REJECTED;
        return OrderStatus::REJECTED;
    }

    Balance parseBalance(const std::string& response) const override {
        rapidjson::Document d;
        d.Parse(response.c_str());

        if (d.HasParseError() || !d.HasMember("balances")) {
            throw std::runtime_error("Invalid balance response: " + response);
        }

        Balance balance;
        for (const auto& asset : d["balances"].GetArray()) {
            std::string symbol = asset["asset"].GetString();
            balance.free[symbol] = std::stod(asset["free"].GetString());
            balance.locked[symbol] = std::stod(asset["locked"].GetString());
            balance.total[symbol] = balance.free[symbol] + balance.locked[symbol];
        }

        return balance;
    }

    std::vector<Position> parsePositions(const std::string& response) const override {
        rapidjson::Document d;
        d.Parse(response.c_str());

        if (d.HasParseError() || !d.IsArray()) {
            throw std::runtime_error("Invalid positions response: " + response);
        }

        std::vector<Position> positions;
        for (const auto& pos : d.GetArray()) {
            Position position;
            position.symbol = pos["symbol"].GetString();
            position.volume = std::stod(pos["origQty"].GetString());
            position.averagePrice = std::stod(pos["price"].GetString());
            positions.push_back(position);
        }

        return positions;
    }

private:
    std::string createSignature(const std::string& timestamp) const {
        std::string payload = "timestamp=" + timestamp;
        
        unsigned char* digest = HMAC(EVP_sha256(),
                                   secretKey_.c_str(), secretKey_.length(),
                                   (unsigned char*)payload.c_str(), payload.length(),
                                   NULL, NULL);
                                   
        char signature[65];
        for(int i = 0; i < 32; i++)
            sprintf(&signature[i*2], "%02x", (unsigned int)digest[i]);
            
        return std::string(signature);
    }

    const char* orderTypeToString(OrderType type) const {
        switch (type) {
            case OrderType::MARKET: return "MARKET";
            case OrderType::LIMIT: return "LIMIT";
            case OrderType::STOP: return "STOP_LOSS";
            case OrderType::STOP_LIMIT: return "STOP_LOSS_LIMIT";
            default: return "MARKET";
        }
    }

    std::string restEndpoint_;
    std::string wsEndpoint_;
    
    std::function<void(const MarketData&)> marketDataHandler_;
    std::function<void(const OrderUpdate&)> orderUpdateHandler_;
};

} // namespace exchange
} // namespace quant_hub
