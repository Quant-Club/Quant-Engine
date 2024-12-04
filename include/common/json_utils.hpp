#pragma once

#include <string>
#include <vector>
#include <simdjson.h>
#include "common/logger.hpp"

namespace quant_hub {
namespace json {

class JsonUtils {
public:
    static simdjson::ondemand::parser& getParser() {
        static simdjson::ondemand::parser parser;
        return parser;
    }

    static simdjson::padded_string loadJsonFile(const std::string& filePath) {
        try {
            return simdjson::padded_string::load(filePath);
        } catch (const simdjson::simdjson_error& e) {
            LOG_ERROR("Failed to load JSON file: ", filePath, " Error: ", e.what());
            throw;
        }
    }

    template<typename T>
    static T getValue(simdjson::ondemand::value value, const T& defaultValue) {
        if (value.is_null()) {
            return defaultValue;
        }
        return static_cast<T>(value);
    }

    static std::string getString(simdjson::ondemand::value value, 
                               const std::string& defaultValue = "") {
        if (value.is_null()) {
            return defaultValue;
        }
        return std::string(value.get_string().value());
    }

    static std::vector<std::string> getStringArray(simdjson::ondemand::array array) {
        std::vector<std::string> result;
        for (auto value : array) {
            result.push_back(std::string(value.get_string().value()));
        }
        return result;
    }

    template<typename T>
    static std::vector<T> getArray(simdjson::ondemand::array array) {
        std::vector<T> result;
        for (auto value : array) {
            result.push_back(static_cast<T>(value));
        }
        return result;
    }

    // Helper function for parsing market data
    static void parseMarketData(simdjson::ondemand::object& obj, 
                              MarketData& data) {
        data.symbol = getString(obj["symbol"]);
        data.timestamp = getValue(obj["timestamp"], 0ULL);
        data.lastPrice = getValue(obj["last_price"], 0.0);
        data.volume = getValue(obj["volume"], 0.0);
        data.bidPrice = getValue(obj["bid_price"], 0.0);
        data.askPrice = getValue(obj["ask_price"], 0.0);
        data.bidSize = getValue(obj["bid_size"], 0.0);
        data.askSize = getValue(obj["ask_size"], 0.0);
    }

    // Helper function for parsing order data
    static void parseOrderData(simdjson::ondemand::object& obj, 
                             Order& order) {
        order.symbol = getString(obj["symbol"]);
        order.orderId = getString(obj["order_id"]);
        order.side = static_cast<OrderSide>(getValue(obj["side"], 0));
        order.type = static_cast<OrderType>(getValue(obj["type"], 0));
        order.price = getValue(obj["price"], 0.0);
        order.volume = getValue(obj["volume"], 0.0);
        order.timestamp = getValue(obj["timestamp"], 0ULL);
    }

    // Helper function for parsing trade data
    static void parseTradeData(simdjson::ondemand::object& obj, 
                             Trade& trade) {
        trade.symbol = getString(obj["symbol"]);
        trade.tradeId = getString(obj["trade_id"]);
        trade.orderId = getString(obj["order_id"]);
        trade.side = static_cast<TradeSide>(getValue(obj["side"], 0));
        trade.price = getValue(obj["price"], 0.0);
        trade.volume = getValue(obj["volume"], 0.0);
        trade.timestamp = getValue(obj["timestamp"], 0ULL);
    }
};

} // namespace json
} // namespace quant_hub
