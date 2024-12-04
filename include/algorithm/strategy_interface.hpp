#pragma once

#include <string>
#include "common/types.hpp"

namespace quant_hub {
namespace algorithm {

class StrategyInterface {
public:
    virtual ~StrategyInterface() = default;

    // Strategy lifecycle
    virtual void initialize() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void cleanup() = 0;

    // Market data handling
    virtual void onMarketData(const MarketData& data) = 0;
    virtual void onOrderUpdate(const OrderUpdate& update) = 0;
    virtual void onTradeUpdate(const TradeUpdate& update) = 0;

    // Risk management
    virtual bool checkRiskLimits() = 0;
    virtual void updateRiskMetrics() = 0;

    // Strategy info
    virtual std::string getName() const = 0;
    virtual StrategyType getType() const = 0;
    virtual StrategyStatus getStatus() const = 0;

protected:
    StrategyInterface() = default;
};

} // namespace algorithm
} // namespace quant_hub
