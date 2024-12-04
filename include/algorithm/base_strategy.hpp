#pragma once

#include <string>
#include <memory>
#include <map>
#include <atomic>
#include "strategy_interface.hpp"
#include "execution/execution_engine.hpp"
#include "common/logger.hpp"
#include "common/config.hpp"

namespace quant_hub {
namespace algorithm {

class BaseStrategy : public StrategyInterface {
public:
    BaseStrategy(const std::string& name,
                std::shared_ptr<execution::ExecutionEngine> executionEngine)
        : name_(name)
        , executionEngine_(executionEngine)
        , status_(StrategyStatus::INITIALIZED)
    {
        loadConfig();
    }

    virtual ~BaseStrategy() {
        if (status_ == StrategyStatus::RUNNING) {
            stop();
        }
    }

    // Strategy lifecycle
    void initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            onInitialize();
            status_ = StrategyStatus::INITIALIZED;
            LOG_INFO("Strategy initialized: ", name_);
        } catch (const std::exception& e) {
            status_ = StrategyStatus::ERROR;
            LOG_ERROR("Failed to initialize strategy ", name_, ": ", e.what());
            throw;
        }
    }

    void start() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != StrategyStatus::INITIALIZED) {
            throw std::runtime_error("Strategy not initialized");
        }

        try {
            subscribeToMarketData();
            onStart();
            status_ = StrategyStatus::RUNNING;
            LOG_INFO("Strategy started: ", name_);
        } catch (const std::exception& e) {
            status_ = StrategyStatus::ERROR;
            LOG_ERROR("Failed to start strategy ", name_, ": ", e.what());
            throw;
        }
    }

    void stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (status_ != StrategyStatus::RUNNING) {
            return;
        }

        try {
            unsubscribeFromMarketData();
            onStop();
            status_ = StrategyStatus::STOPPED;
            LOG_INFO("Strategy stopped: ", name_);
        } catch (const std::exception& e) {
            status_ = StrategyStatus::ERROR;
            LOG_ERROR("Failed to stop strategy ", name_, ": ", e.what());
            throw;
        }
    }

    void cleanup() override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            onCleanup();
            LOG_INFO("Strategy cleaned up: ", name_);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to cleanup strategy ", name_, ": ", e.what());
            throw;
        }
    }

    // Market data handling
    void onMarketData(const MarketData& data) override {
        if (status_ != StrategyStatus::RUNNING) return;

        try {
            processMarketData(data);
        } catch (const std::exception& e) {
            LOG_ERROR("Error processing market data in strategy ", name_, ": ", e.what());
        }
    }

    void onOrderUpdate(const OrderUpdate& update) override {
        if (status_ != StrategyStatus::RUNNING) return;

        try {
            processOrderUpdate(update);
        } catch (const std::exception& e) {
            LOG_ERROR("Error processing order update in strategy ", name_, ": ", e.what());
        }
    }

    void onTradeUpdate(const TradeUpdate& update) override {
        if (status_ != StrategyStatus::RUNNING) return;

        try {
            processTradeUpdate(update);
            updatePosition(update);
        } catch (const std::exception& e) {
            LOG_ERROR("Error processing trade update in strategy ", name_, ": ", e.what());
        }
    }

    // Risk management
    bool checkRiskLimits() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return onCheckRiskLimits();
    }

    void updateRiskMetrics() override {
        std::lock_guard<std::mutex> lock(mutex_);
        onUpdateRiskMetrics();
    }

    // Strategy info
    std::string getName() const override {
        return name_;
    }

    StrategyType getType() const override {
        return type_;
    }

    StrategyStatus getStatus() const override {
        return status_;
    }

protected:
    // Virtual methods to be implemented by derived classes
    virtual void onInitialize() = 0;
    virtual void onStart() = 0;
    virtual void onStop() = 0;
    virtual void onCleanup() = 0;
    
    virtual void processMarketData(const MarketData& data) = 0;
    virtual void processOrderUpdate(const OrderUpdate& update) = 0;
    virtual void processTradeUpdate(const TradeUpdate& update) = 0;
    
    virtual bool onCheckRiskLimits() = 0;
    virtual void onUpdateRiskMetrics() = 0;

    // Helper methods for derived classes
    OrderId submitOrder(const Order& order, const std::string& exchangeName) {
        return executionEngine_->submitOrder(order, exchangeName);
    }

    void cancelOrder(const OrderId& orderId, const std::string& exchangeName) {
        executionEngine_->cancelOrder(orderId, exchangeName);
    }

    OrderStatus getOrderStatus(const OrderId& orderId, const std::string& exchangeName) {
        return executionEngine_->getOrderStatus(orderId, exchangeName);
    }

    Position getPosition(const std::string& symbol) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = positions_.find(symbol);
        return it != positions_.end() ? it->second : Position{symbol, 0, 0, 0, 0};
    }

    std::map<std::string, Position> getPositions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return positions_;
    }

private:
    void loadConfig() {
        auto& config = Config::getInstance();
        std::string configPath = "strategies." + name_;
        
        // Load strategy-specific configuration
        symbols_ = config.get<std::vector<std::string>>(configPath + ".symbols", {});
        exchanges_ = config.get<std::vector<std::string>>(configPath + ".exchanges", {});
    }

    void subscribeToMarketData() {
        for (const auto& symbol : symbols_) {
            for (const auto& exchange : exchanges_) {
                executionEngine_->subscribeToMarketData(
                    symbol,
                    exchange,
                    [this](const MarketData& data) {
                        this->onMarketData(data);
                    }
                );
            }
        }
    }

    void unsubscribeFromMarketData() {
        for (const auto& symbol : symbols_) {
            for (const auto& exchange : exchanges_) {
                executionEngine_->unsubscribeFromMarketData(symbol, exchange);
            }
        }
    }

    void updatePosition(const TradeUpdate& update) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& position = positions_[update.symbol];
        
        position.symbol = update.symbol;
        if (update.side == OrderSide::BUY) {
            position.volume += update.volume;
        } else {
            position.volume -= update.volume;
        }
        
        // Update average price and P&L
        if (position.volume != 0) {
            position.averagePrice = (position.averagePrice * (position.volume - update.volume) +
                                   update.price * update.volume) / position.volume;
        } else {
            position.averagePrice = 0;
        }
    }

    std::string name_;
    StrategyType type_;
    std::atomic<StrategyStatus> status_;
    
    std::shared_ptr<execution::ExecutionEngine> executionEngine_;
    
    mutable std::mutex mutex_;
    std::vector<std::string> symbols_;
    std::vector<std::string> exchanges_;
    std::map<std::string, Position> positions_;
};

} // namespace algorithm
} // namespace quant_hub
