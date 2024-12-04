#pragma once

#include <map>
#include <deque>
#include "algorithm/base_strategy.hpp"
#include "common/types.hpp"
#include "common/logger.hpp"

namespace quant_hub {
namespace algorithm {

struct MarketMakerConfig {
    double spreadPercentage;      // Target spread as percentage
    double inventoryLimit;        // Maximum inventory in base currency
    double orderSize;            // Size of each order
    double minSpread;            // Minimum spread to quote
    double maxSpread;            // Maximum spread to quote
    int priceQueueSize;         // Size of price queue for volatility calculation
    double volMultiplier;        // Multiplier for volatility in spread calculation
    double skewFactor;          // Factor to skew quotes based on inventory
};

class MarketMaker : public BaseStrategy {
public:
    MarketMaker(const std::string& name,
                std::shared_ptr<execution::ExecutionEngine> executionEngine,
                const MarketMakerConfig& config)
        : BaseStrategy(name, executionEngine)
        , config_(config)
    {
        LOG_INFO("Market Maker strategy created: ", name);
    }

protected:
    void onInitialize() override {
        // Reset state
        activeBids_.clear();
        activeAsks_.clear();
        priceQueue_.clear();
        lastMidPrice_ = 0.0;
        inventory_ = 0.0;
        
        LOG_INFO("Market Maker initialized");
    }

    void onStart() override {
        // Initial market analysis
        updateMarketState();
        
        // Place initial orders
        placeOrders();
        
        LOG_INFO("Market Maker started");
    }

    void onStop() override {
        // Cancel all active orders
        cancelAllOrders();
        
        LOG_INFO("Market Maker stopped");
    }

    void processMarketData(const MarketData& data) override {
        try {
            // Update market state
            updateMarketState(data);

            // Check if we need to adjust orders
            if (shouldAdjustOrders()) {
                cancelAllOrders();
                placeOrders();
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Error processing market data: ", e.what());
        }
    }

    void processOrderUpdate(const OrderUpdate& update) override {
        try {
            // Update order tracking
            if (update.side == OrderSide::BUY) {
                updateOrderMap(activeBids_, update);
            } else {
                updateOrderMap(activeAsks_, update);
            }

            // Handle filled orders
            if (update.status == OrderStatus::FILLED) {
                handleFill(update);
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Error processing order update: ", e.what());
        }
    }

    bool onCheckRiskLimits() override {
        // Check inventory limits
        if (std::abs(inventory_) > config_.inventoryLimit) {
            LOG_WARNING("Inventory limit exceeded: ", inventory_);
            return false;
        }
        return true;
    }

    void onUpdateRiskMetrics() override {
        // Update risk metrics
        calculateVolatility();
    }

private:
    void updateMarketState(const MarketData& data = MarketData()) {
        if (data.lastPrice > 0) {
            // Update price queue
            priceQueue_.push_back(data.lastPrice);
            if (priceQueue_.size() > config_.priceQueueSize) {
                priceQueue_.pop_front();
            }

            lastMidPrice_ = data.lastPrice;
        }
    }

    void calculateVolatility() {
        if (priceQueue_.size() < 2) return;

        // Calculate rolling volatility
        double sum = 0.0;
        double sumSq = 0.0;
        
        for (double price : priceQueue_) {
            sum += price;
            sumSq += price * price;
        }
        
        double mean = sum / priceQueue_.size();
        currentVolatility_ = std::sqrt(sumSq / priceQueue_.size() - mean * mean);
    }

    bool shouldAdjustOrders() {
        // Implement logic to determine if orders need adjustment
        // For example, if price has moved significantly or inventory is skewed
        return true;
    }

    void placeOrders() {
        if (!onCheckRiskLimits()) return;

        // Calculate target spread based on volatility
        double targetSpread = calculateTargetSpread();
        
        // Calculate order prices with inventory skew
        double skew = inventory_ * config_.skewFactor;
        double bidPrice = lastMidPrice_ * (1.0 - targetSpread/2 + skew);
        double askPrice = lastMidPrice_ * (1.0 + targetSpread/2 + skew);

        // Place bid order
        Order bidOrder;
        bidOrder.side = OrderSide::BUY;
        bidOrder.price = bidPrice;
        bidOrder.volume = config_.orderSize;
        OrderId bidOrderId = submitOrder(bidOrder, "Binance");
        activeBids_[bidOrderId] = bidOrder;

        // Place ask order
        Order askOrder;
        askOrder.side = OrderSide::SELL;
        askOrder.price = askPrice;
        askOrder.volume = config_.orderSize;
        OrderId askOrderId = submitOrder(askOrder, "Binance");
        activeAsks_[askOrderId] = askOrder;
    }

    double calculateTargetSpread() {
        // Base spread
        double spread = config_.spreadPercentage;
        
        // Adjust for volatility
        spread += currentVolatility_ * config_.volMultiplier;
        
        // Clamp to min/max
        spread = std::max(config_.minSpread, std::min(config_.maxSpread, spread));
        
        return spread;
    }

    void cancelAllOrders() {
        // Cancel all active orders
        for (const auto& [orderId, _] : activeBids_) {
            cancelOrder(orderId, "Binance");
        }
        for (const auto& [orderId, _] : activeAsks_) {
            cancelOrder(orderId, "Binance");
        }
        
        activeBids_.clear();
        activeAsks_.clear();
    }

    void handleFill(const OrderUpdate& update) {
        // Update inventory
        if (update.side == OrderSide::BUY) {
            inventory_ += update.filledVolume;
        } else {
            inventory_ -= update.filledVolume;
        }

        // Log fill
        LOG_INFO("Order filled: ", update.orderId, 
                 " Side: ", update.side == OrderSide::BUY ? "BUY" : "SELL",
                 " Price: ", update.filledPrice,
                 " Volume: ", update.filledVolume,
                 " New inventory: ", inventory_);
    }

    void updateOrderMap(std::map<OrderId, Order>& orderMap,
                       const OrderUpdate& update) {
        if (update.status == OrderStatus::CANCELED ||
            update.status == OrderStatus::REJECTED ||
            update.status == OrderStatus::FILLED) {
            orderMap.erase(update.orderId);
        }
    }

    MarketMakerConfig config_;
    std::map<OrderId, Order> activeBids_;
    std::map<OrderId, Order> activeAsks_;
    std::deque<double> priceQueue_;
    double lastMidPrice_;
    double inventory_;
    double currentVolatility_;
};

} // namespace algorithm
} // namespace quant_hub
