#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "common/types.hpp"

namespace quant_hub {
namespace exchange {

class ExchangeInterface {
public:
    virtual ~ExchangeInterface() = default;

    // Market Data
    virtual void subscribeMarketData(const std::string& symbol) = 0;
    virtual void unsubscribeMarketData(const std::string& symbol) = 0;
    virtual MarketData getMarketData(const std::string& symbol) = 0;

    // Trading
    virtual OrderId submitOrder(const Order& order) = 0;
    virtual void cancelOrder(const OrderId& orderId) = 0;
    virtual OrderStatus getOrderStatus(const OrderId& orderId) = 0;
    
    // Account
    virtual Balance getBalance() = 0;
    virtual std::vector<Position> getPositions() = 0;

    // Exchange Info
    virtual std::string getName() const = 0;
    virtual std::vector<std::string> getSupportedSymbols() const = 0;
    virtual ExchangeInfo getExchangeInfo() const = 0;

protected:
    ExchangeInterface() = default;
};

} // namespace exchange
} // namespace quant_hub
