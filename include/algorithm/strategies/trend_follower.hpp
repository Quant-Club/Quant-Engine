#pragma once

#include <vector>
#include <deque>
#include "algorithm/base_strategy.hpp"
#include "common/types.hpp"
#include "common/logger.hpp"
#include "model/kernels.hpp"

namespace quant_hub {
namespace algorithm {

struct TrendFollowerConfig {
    int shortPeriod;          // Short-term MA period
    int longPeriod;          // Long-term MA period
    double positionSize;      // Base position size
    double maxPositionSize;   // Maximum position size
    double stopLossPercent;   // Stop loss percentage
    double takeProfitPercent; // Take profit percentage
    double atrPeriod;        // ATR period for volatility calculation
    double atrMultiplier;    // ATR multiplier for position sizing
};

class TrendFollower : public BaseStrategy {
public:
    TrendFollower(const std::string& name,
                 std::shared_ptr<execution::ExecutionEngine> executionEngine,
                 std::shared_ptr<model::ComputeEngine> computeEngine,
                 const TrendFollowerConfig& config)
        : BaseStrategy(name, executionEngine)
        , computeEngine_(computeEngine)
        , config_(config)
        , computeKernels_(model::ComputeKernels::create(computeEngine))
    {
        LOG_INFO("Trend Follower strategy created: ", name);
    }

protected:
    void onInitialize() override {
        // Reset state
        prices_.clear();
        shortMa_.clear();
        longMa_.clear();
        atr_.clear();
        position_ = 0.0;
        entryPrice_ = 0.0;
        stopLossPrice_ = 0.0;
        takeProfitPrice_ = 0.0;
        
        LOG_INFO("Trend Follower initialized");
    }

    void onStart() override {
        // Initial market analysis if we have enough data
        if (prices_.size() >= config_.longPeriod) {
            updateIndicators();
            checkSignals();
        }
        
        LOG_INFO("Trend Follower started");
    }

    void onStop() override {
        // Close all positions
        closeAllPositions();
        
        LOG_INFO("Trend Follower stopped");
    }

    void processMarketData(const MarketData& data) override {
        try {
            // Update price history
            updatePrices(data);

            // Update indicators
            if (prices_.size() >= config_.longPeriod) {
                updateIndicators();
                checkSignals();
                updateStopLoss(data.lastPrice);
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Error processing market data: ", e.what());
        }
    }

    void processOrderUpdate(const OrderUpdate& update) override {
        try {
            // Handle filled orders
            if (update.status == OrderStatus::FILLED) {
                handleFill(update);
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Error processing order update: ", e.what());
        }
    }

    bool onCheckRiskLimits() override {
        // Check position limits
        if (std::abs(position_) > config_.maxPositionSize) {
            LOG_WARNING("Position size limit exceeded: ", position_);
            return false;
        }
        return true;
    }

    void onUpdateRiskMetrics() override {
        // Update risk metrics
        calculateDrawdown();
    }

private:
    void updatePrices(const MarketData& data) {
        prices_.push_back(data.lastPrice);
        if (prices_.size() > config_.longPeriod) {
            prices_.pop_front();
        }
    }

    void updateIndicators() {
        try {
            // Calculate moving averages
            std::vector<double> pricesVec(prices_.begin(), prices_.end());
            
            // Short MA
            shortMa_.resize(pricesVec.size() - config_.shortPeriod + 1);
            computeKernels_->movingAverage(pricesVec, shortMa_, config_.shortPeriod);
            
            // Long MA
            longMa_.resize(pricesVec.size() - config_.longPeriod + 1);
            computeKernels_->movingAverage(pricesVec, longMa_, config_.longPeriod);
            
            // ATR calculation
            calculateATR(pricesVec);

        } catch (const std::exception& e) {
            LOG_ERROR("Error updating indicators: ", e.what());
            throw;
        }
    }

    void calculateATR(const std::vector<double>& prices) {
        if (prices.size() < 2) return;

        std::vector<double> trueRanges;
        trueRanges.reserve(prices.size() - 1);

        for (size_t i = 1; i < prices.size(); ++i) {
            double highLow = std::abs(prices[i] - prices[i-1]);
            trueRanges.push_back(highLow);
        }

        atr_.resize(trueRanges.size() - config_.atrPeriod + 1);
        computeKernels_->movingAverage(trueRanges, atr_, config_.atrPeriod);
    }

    void checkSignals() {
        if (shortMa_.empty() || longMa_.empty() || atr_.empty()) return;

        double currentShortMa = shortMa_.back();
        double currentLongMa = longMa_.back();
        double currentAtr = atr_.back();

        // Calculate position size based on ATR
        double positionSize = config_.positionSize * (config_.atrMultiplier / currentAtr);
        positionSize = std::min(positionSize, config_.maxPositionSize);

        // Generate signals
        if (currentShortMa > currentLongMa && position_ <= 0) {
            // Bullish crossover
            if (position_ < 0) {
                closePosition();
            }
            enterLong(positionSize);
        }
        else if (currentShortMa < currentLongMa && position_ >= 0) {
            // Bearish crossover
            if (position_ > 0) {
                closePosition();
            }
            enterShort(positionSize);
        }
    }

    void enterLong(double size) {
        if (!onCheckRiskLimits()) return;

        Order order;
        order.side = OrderSide::BUY;
        order.volume = size;
        order.type = OrderType::MARKET;
        
        OrderId orderId = submitOrder(order, "Binance");
        LOG_INFO("Entered long position: ", orderId, " Size: ", size);
    }

    void enterShort(double size) {
        if (!onCheckRiskLimits()) return;

        Order order;
        order.side = OrderSide::SELL;
        order.volume = size;
        order.type = OrderType::MARKET;
        
        OrderId orderId = submitOrder(order, "Binance");
        LOG_INFO("Entered short position: ", orderId, " Size: ", size);
    }

    void closePosition() {
        if (position_ == 0.0) return;

        Order order;
        order.side = position_ > 0 ? OrderSide::SELL : OrderSide::BUY;
        order.volume = std::abs(position_);
        order.type = OrderType::MARKET;
        
        OrderId orderId = submitOrder(order, "Binance");
        LOG_INFO("Closed position: ", orderId, " Size: ", order.volume);
    }

    void closeAllPositions() {
        closePosition();
    }

    void handleFill(const OrderUpdate& update) {
        // Update position
        if (update.side == OrderSide::BUY) {
            position_ += update.filledVolume;
        } else {
            position_ -= update.filledVolume;
        }

        // Update entry price and stop loss
        if (position_ != 0.0) {
            entryPrice_ = update.filledPrice;
            updateStopLoss(update.filledPrice);
        }

        LOG_INFO("Order filled: ", update.orderId,
                 " Side: ", update.side == OrderSide::BUY ? "BUY" : "SELL",
                 " Price: ", update.filledPrice,
                 " Volume: ", update.filledVolume,
                 " New position: ", position_);
    }

    void updateStopLoss(double currentPrice) {
        if (position_ == 0.0) return;

        // Calculate stop loss and take profit levels
        if (position_ > 0) {
            stopLossPrice_ = entryPrice_ * (1.0 - config_.stopLossPercent);
            takeProfitPrice_ = entryPrice_ * (1.0 + config_.takeProfitPercent);

            // Check if stop loss or take profit hit
            if (currentPrice <= stopLossPrice_ || currentPrice >= takeProfitPrice_) {
                closePosition();
            }
        } else {
            stopLossPrice_ = entryPrice_ * (1.0 + config_.stopLossPercent);
            takeProfitPrice_ = entryPrice_ * (1.0 - config_.takeProfitPercent);

            // Check if stop loss or take profit hit
            if (currentPrice >= stopLossPrice_ || currentPrice <= takeProfitPrice_) {
                closePosition();
            }
        }
    }

    void calculateDrawdown() {
        if (prices_.empty()) return;

        double currentPrice = prices_.back();
        if (position_ > 0) {
            double unrealizedPnl = (currentPrice - entryPrice_) * position_;
            // Update drawdown metrics
        } else if (position_ < 0) {
            double unrealizedPnl = (entryPrice_ - currentPrice) * (-position_);
            // Update drawdown metrics
        }
    }

    std::shared_ptr<model::ComputeEngine> computeEngine_;
    std::shared_ptr<model::ComputeKernels> computeKernels_;
    TrendFollowerConfig config_;
    
    std::deque<double> prices_;
    std::vector<double> shortMa_;
    std::vector<double> longMa_;
    std::vector<double> atr_;
    
    double position_;
    double entryPrice_;
    double stopLossPrice_;
    double takeProfitPrice_;
};

} // namespace algorithm
} // namespace quant_hub
