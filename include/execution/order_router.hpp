#pragma once

#include <memory>
#include <map>
#include <string>
#include <mutex>
#include "common/types.hpp"
#include "exchange/exchange_interface.hpp"
#include "risk/risk_manager.hpp"
#include "common/logger.hpp"

namespace quant_hub {
namespace execution {

class OrderRouter {
public:
    OrderRouter(std::shared_ptr<risk::RiskManager> riskManager)
        : riskManager_(riskManager)
    {
        LOG_INFO("Initializing order router");
    }

    void registerExchange(const std::string& name,
                         std::shared_ptr<exchange::ExchangeInterface> exchange) {
        std::lock_guard<std::mutex> lock(mutex_);
        exchanges_[name] = exchange;
        LOG_INFO("Registered exchange: ", name);
    }

    void unregisterExchange(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        exchanges_.erase(name);
        LOG_INFO("Unregistered exchange: ", name);
    }

    OrderId submitOrder(const Order& order, const std::string& exchangeName) {
        // Check if exchange exists
        auto exchange = getExchange(exchangeName);
        if (!exchange) {
            throw std::runtime_error("Exchange not found: " + exchangeName);
        }

        // Perform risk checks
        if (!riskManager_->checkOrderRisk(order)) {
            LOG_ERROR("Order rejected by risk manager: ", order.clientOrderId);
            throw std::runtime_error("Order rejected by risk manager");
        }

        // Submit order to exchange
        try {
            OrderId orderId = exchange->submitOrder(order);
            
            // Record order in order book
            recordOrder(orderId, order, exchangeName);
            
            LOG_INFO("Order submitted successfully: ", orderId);
            return orderId;
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to submit order: ", e.what());
            throw;
        }
    }

    void cancelOrder(const OrderId& orderId, const std::string& exchangeName) {
        auto exchange = getExchange(exchangeName);
        if (!exchange) {
            throw std::runtime_error("Exchange not found: " + exchangeName);
        }

        try {
            exchange->cancelOrder(orderId);
            removeOrder(orderId);
            LOG_INFO("Order cancelled successfully: ", orderId);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to cancel order: ", e.what());
            throw;
        }
    }

    OrderStatus getOrderStatus(const OrderId& orderId, const std::string& exchangeName) {
        auto exchange = getExchange(exchangeName);
        if (!exchange) {
            throw std::runtime_error("Exchange not found: " + exchangeName);
        }

        return exchange->getOrderStatus(orderId);
    }

    std::vector<Order> getActiveOrders(const std::string& exchangeName = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Order> activeOrders;

        if (!exchangeName.empty()) {
            // Get orders for specific exchange
            auto it = activeOrdersByExchange_.find(exchangeName);
            if (it != activeOrdersByExchange_.end()) {
                for (const auto& orderId : it->second) {
                    auto orderIt = orderBook_.find(orderId);
                    if (orderIt != orderBook_.end()) {
                        activeOrders.push_back(orderIt->second);
                    }
                }
            }
        } else {
            // Get all active orders
            for (const auto& [orderId, order] : orderBook_) {
                activeOrders.push_back(order);
            }
        }

        return activeOrders;
    }

private:
    std::shared_ptr<exchange::ExchangeInterface> getExchange(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = exchanges_.find(name);
        if (it == exchanges_.end()) {
            return nullptr;
        }
        return it->second;
    }

    void recordOrder(const OrderId& orderId, const Order& order, const std::string& exchangeName) {
        std::lock_guard<std::mutex> lock(mutex_);
        orderBook_[orderId] = order;
        activeOrdersByExchange_[exchangeName].insert(orderId);
    }

    void removeOrder(const OrderId& orderId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto orderIt = orderBook_.find(orderId);
        if (orderIt != orderBook_.end()) {
            for (auto& [exchange, orders] : activeOrdersByExchange_) {
                orders.erase(orderId);
            }
            orderBook_.erase(orderIt);
        }
    }

    std::mutex mutex_;
    std::shared_ptr<risk::RiskManager> riskManager_;
    std::map<std::string, std::shared_ptr<exchange::ExchangeInterface>> exchanges_;
    std::map<OrderId, Order> orderBook_;
    std::map<std::string, std::set<OrderId>> activeOrdersByExchange_;
};

} // namespace execution
} // namespace quant_hub
