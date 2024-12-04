#pragma once

#include <memory>
#include <string>
#include <map>
#include <functional>
#include "event_processor_impl.hpp"
#include "order_router.hpp"
#include "risk/risk_manager.hpp"
#include "exchange/exchange_interface.hpp"
#include "common/logger.hpp"

namespace quant_hub {
namespace execution {

class ExecutionEngine {
public:
    ExecutionEngine(size_t eventBufferSize = 1024)
        : eventProcessor_(std::make_shared<EventProcessorImpl>(eventBufferSize))
        , riskManager_(std::make_shared<risk::RiskManager>())
        , orderRouter_(std::make_shared<OrderRouter>(riskManager_))
    {
        LOG_INFO("Initializing execution engine");
        setupEventHandlers();
    }

    void start() {
        eventProcessor_->start();
        LOG_INFO("Execution engine started");
    }

    void stop() {
        eventProcessor_->stop();
        LOG_INFO("Execution engine stopped");
    }

    void registerExchange(const std::string& name,
                         std::shared_ptr<exchange::ExchangeInterface> exchange) {
        orderRouter_->registerExchange(name, exchange);
    }

    void unregisterExchange(const std::string& name) {
        orderRouter_->unregisterExchange(name);
    }

    OrderId submitOrder(const Order& order, const std::string& exchangeName) {
        try {
            return orderRouter_->submitOrder(order, exchangeName);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to submit order: ", e.what());
            throw;
        }
    }

    void cancelOrder(const OrderId& orderId, const std::string& exchangeName) {
        try {
            orderRouter_->cancelOrder(orderId, exchangeName);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to cancel order: ", e.what());
            throw;
        }
    }

    OrderStatus getOrderStatus(const OrderId& orderId, const std::string& exchangeName) {
        return orderRouter_->getOrderStatus(orderId, exchangeName);
    }

    std::vector<Order> getActiveOrders(const std::string& exchangeName = "") {
        return orderRouter_->getActiveOrders(exchangeName);
    }

    void subscribeToMarketData(const std::string& symbol,
                              const std::string& exchangeName,
                              std::function<void(const MarketData&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        marketDataCallbacks_[symbol][exchangeName] = callback;

        Event event;
        event.type = EventType::MARKET_DATA;
        event.source = exchangeName;
        event.data = "subscribe:" + symbol;
        
        eventProcessor_->publish(event);
    }

    void unsubscribeFromMarketData(const std::string& symbol,
                                  const std::string& exchangeName) {
        std::lock_guard<std::mutex> lock(mutex_);
        marketDataCallbacks_[symbol].erase(exchangeName);
        
        if (marketDataCallbacks_[symbol].empty()) {
            marketDataCallbacks_.erase(symbol);
        }

        Event event;
        event.type = EventType::MARKET_DATA;
        event.source = exchangeName;
        event.data = "unsubscribe:" + symbol;
        
        eventProcessor_->publish(event);
    }

    void enableRiskManager() {
        riskManager_->enable();
    }

    void disableRiskManager() {
        riskManager_->disable();
    }

    void setRiskLimits(const risk::RiskLimits& limits) {
        riskManager_->setLimits(limits);
    }

private:
    void setupEventHandlers() {
        // Handle market data events
        eventProcessor_->subscribe(
            EventType::MARKET_DATA,
            [this](const Event& event) {
                handleMarketDataEvent(event);
            }
        );

        // Handle order updates
        eventProcessor_->subscribe(
            EventType::ORDER_UPDATE,
            [this](const Event& event) {
                handleOrderUpdateEvent(event);
            }
        );

        // Handle trade updates
        eventProcessor_->subscribe(
            EventType::TRADE_UPDATE,
            [this](const Event& event) {
                handleTradeUpdateEvent(event);
            }
        );

        // Handle system events
        eventProcessor_->subscribe(
            EventType::SYSTEM_EVENT,
            [this](const Event& event) {
                handleSystemEvent(event);
            }
        );
    }

    void handleMarketDataEvent(const Event& event) {
        try {
            // Parse market data from event
            MarketData data = parseMarketData(event.data);
            
            // Notify subscribers
            std::lock_guard<std::mutex> lock(mutex_);
            auto symbolIt = marketDataCallbacks_.find(data.symbol);
            if (symbolIt != marketDataCallbacks_.end()) {
                auto exchangeIt = symbolIt->second.find(event.source);
                if (exchangeIt != symbolIt->second.end()) {
                    exchangeIt->second(data);
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling market data event: ", e.what());
        }
    }

    void handleOrderUpdateEvent(const Event& event) {
        try {
            OrderUpdate update = parseOrderUpdate(event.data);
            
            // Update risk manager
            if (update.status == OrderStatus::FILLED ||
                update.status == OrderStatus::PARTIAL) {
                riskManager_->updatePosition(
                    update.symbol,
                    update.filledVolume,
                    update.filledPrice
                );
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling order update event: ", e.what());
        }
    }

    void handleTradeUpdateEvent(const Event& event) {
        try {
            TradeUpdate update = parseTradeUpdate(event.data);
            
            // Update risk metrics
            riskManager_->updatePosition(
                update.symbol,
                update.volume,
                update.price
            );
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling trade update event: ", e.what());
        }
    }

    void handleSystemEvent(const Event& event) {
        try {
            if (event.data == "EOD") {
                // End of day processing
                riskManager_->resetDailyMetrics();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error handling system event: ", e.what());
        }
    }

    // Helper functions to parse event data
    MarketData parseMarketData(const std::string& data);
    OrderUpdate parseOrderUpdate(const std::string& data);
    TradeUpdate parseTradeUpdate(const std::string& data);

    std::shared_ptr<EventProcessorImpl> eventProcessor_;
    std::shared_ptr<risk::RiskManager> riskManager_;
    std::shared_ptr<OrderRouter> orderRouter_;
    
    std::mutex mutex_;
    std::map<std::string, std::map<std::string, std::function<void(const MarketData&)>>> marketDataCallbacks_;
};

} // namespace execution
} // namespace quant_hub
