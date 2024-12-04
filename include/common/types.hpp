#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <map>

namespace quant_hub {

using OrderId = std::string;
using Timestamp = std::chrono::system_clock::time_point;
using Price = double;
using Volume = double;

enum class OrderType {
    MARKET,
    LIMIT,
    STOP,
    STOP_LIMIT
};

enum class OrderSide {
    BUY,
    SELL
};

enum class OrderStatus {
    PENDING,
    PARTIAL,
    FILLED,
    CANCELLED,
    REJECTED
};

enum class EventType {
    MARKET_DATA,
    ORDER_UPDATE,
    TRADE_UPDATE,
    STRATEGY_SIGNAL,
    SYSTEM_EVENT
};

struct MarketData {
    std::string symbol;
    Timestamp timestamp;
    Price lastPrice;
    Price bestBid;
    Price bestAsk;
    Volume bidVolume;
    Volume askVolume;
    std::vector<std::pair<Price, Volume>> bids;
    std::vector<std::pair<Price, Volume>> asks;
};

struct Order {
    std::string symbol;
    OrderType type;
    OrderSide side;
    Price price;
    Volume volume;
    std::string clientOrderId;
};

struct OrderUpdate {
    OrderId orderId;
    OrderStatus status;
    Price filledPrice;
    Volume filledVolume;
    Timestamp timestamp;
    std::string message;
};

struct TradeUpdate {
    OrderId orderId;
    std::string symbol;
    Price price;
    Volume volume;
    OrderSide side;
    Timestamp timestamp;
};

struct Balance {
    std::map<std::string, double> free;
    std::map<std::string, double> locked;
    std::map<std::string, double> total;
};

struct Position {
    std::string symbol;
    Volume volume;
    Price averagePrice;
    Price unrealizedPnl;
    Price realizedPnl;
};

struct ExchangeInfo {
    std::string name;
    std::map<std::string, double> tradingFees;
    std::map<std::string, double> minimumOrders;
    std::map<std::string, int> decimals;
};

struct Event {
    EventType type;
    Timestamp timestamp;
    std::string source;
    std::string data;
};

using EventHandler = std::function<void(const Event&)>;

enum class StrategyType {
    MARKET_MAKING,
    TREND_FOLLOWING,
    MEAN_REVERSION,
    ARBITRAGE
};

enum class StrategyStatus {
    INITIALIZED,
    RUNNING,
    STOPPED,
    ERROR
};

} // namespace quant_hub
