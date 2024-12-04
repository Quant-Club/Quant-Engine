#pragma once

#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include "common/types.hpp"
#include "common/logger.hpp"
#include "common/config.hpp"

namespace quant_hub {
namespace risk {

struct RiskLimits {
    double maxOrderSize;
    double maxPositionSize;
    double maxLeverage;
    double maxDrawdown;
    double maxDailyLoss;
    std::map<std::string, double> symbolLimits;
};

class RiskManager {
public:
    RiskManager()
        : enabled_(true)
    {
        loadConfig();
        LOG_INFO("Risk manager initialized");
    }

    bool checkOrderRisk(const Order& order) {
        if (!enabled_) return true;

        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check order size
        if (order.volume * order.price > limits_.maxOrderSize) {
            LOG_WARNING("Order size exceeds limit: ", order.volume * order.price,
                       " > ", limits_.maxOrderSize);
            return false;
        }

        // Check symbol-specific limits
        auto symbolLimit = limits_.symbolLimits.find(order.symbol);
        if (symbolLimit != limits_.symbolLimits.end() &&
            order.volume > symbolLimit->second) {
            LOG_WARNING("Order size exceeds symbol limit: ", order.volume,
                       " > ", symbolLimit->second);
            return false;
        }

        // Check position limits
        double newPosition = calculateNewPosition(order);
        if (std::abs(newPosition) > limits_.maxPositionSize) {
            LOG_WARNING("Position size would exceed limit: ", std::abs(newPosition),
                       " > ", limits_.maxPositionSize);
            return false;
        }

        // Check leverage
        if (calculateLeverage(order) > limits_.maxLeverage) {
            LOG_WARNING("Leverage would exceed limit");
            return false;
        }

        // Check drawdown
        if (calculateDrawdown() > limits_.maxDrawdown) {
            LOG_WARNING("Drawdown limit exceeded");
            return false;
        }

        // Check daily loss
        if (calculateDailyLoss() > limits_.maxDailyLoss) {
            LOG_WARNING("Daily loss limit exceeded");
            return false;
        }

        return true;
    }

    void updatePosition(const std::string& symbol, double volume, double price) {
        std::lock_guard<std::mutex> lock(mutex_);
        positions_[symbol] = volume;
        averagePrices_[symbol] = price;
    }

    void updateBalance(double balance) {
        std::lock_guard<std::mutex> lock(mutex_);
        currentBalance_ = balance;
        
        if (balance > peakBalance_) {
            peakBalance_ = balance;
        }
    }

    void resetDailyMetrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        dailyStartBalance_ = currentBalance_;
        dailyLow_ = currentBalance_;
    }

    void enable() {
        enabled_ = true;
        LOG_INFO("Risk manager enabled");
    }

    void disable() {
        enabled_ = false;
        LOG_WARNING("Risk manager disabled");
    }

    bool isEnabled() const {
        return enabled_;
    }

    RiskLimits getLimits() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return limits_;
    }

    void setLimits(const RiskLimits& limits) {
        std::lock_guard<std::mutex> lock(mutex_);
        limits_ = limits;
        LOG_INFO("Risk limits updated");
    }

private:
    void loadConfig() {
        auto& config = Config::getInstance();
        
        limits_.maxOrderSize = config.get<double>("risk.max_order_size", 100000.0);
        limits_.maxPositionSize = config.get<double>("risk.max_position_size", 1000000.0);
        limits_.maxLeverage = config.get<double>("risk.max_leverage", 3.0);
        limits_.maxDrawdown = config.get<double>("risk.max_drawdown", 0.1);
        limits_.maxDailyLoss = config.get<double>("risk.max_daily_loss", 10000.0);
    }

    double calculateNewPosition(const Order& order) {
        auto it = positions_.find(order.symbol);
        double currentPosition = (it != positions_.end()) ? it->second : 0.0;
        
        if (order.side == OrderSide::BUY) {
            return currentPosition + order.volume;
        } else {
            return currentPosition - order.volume;
        }
    }

    double calculateLeverage(const Order& order) {
        double totalPositionValue = 0.0;
        for (const auto& [symbol, volume] : positions_) {
            auto priceIt = averagePrices_.find(symbol);
            if (priceIt != averagePrices_.end()) {
                totalPositionValue += std::abs(volume * priceIt->second);
            }
        }

        // Add new order to total position value
        totalPositionValue += order.volume * order.price;

        return totalPositionValue / currentBalance_;
    }

    double calculateDrawdown() {
        if (peakBalance_ <= 0.0) return 0.0;
        return (peakBalance_ - currentBalance_) / peakBalance_;
    }

    double calculateDailyLoss() {
        return dailyStartBalance_ - currentBalance_;
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_;
    
    RiskLimits limits_;
    std::map<std::string, double> positions_;
    std::map<std::string, double> averagePrices_;
    
    double currentBalance_ = 0.0;
    double peakBalance_ = 0.0;
    double dailyStartBalance_ = 0.0;
    double dailyLow_ = 0.0;
};

} // namespace risk
} // namespace quant_hub
