#pragma once

#include <memory>
#include <map>
#include <string>
#include <mutex>
#include "strategy_interface.hpp"
#include "common/logger.hpp"
#include "execution/execution_engine.hpp"

namespace quant_hub {
namespace algorithm {

class StrategyManager {
public:
    explicit StrategyManager(std::shared_ptr<execution::ExecutionEngine> executionEngine)
        : executionEngine_(executionEngine)
    {
        LOG_INFO("Initializing strategy manager");
    }

    void registerStrategy(const std::string& name,
                         std::shared_ptr<StrategyInterface> strategy) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (strategies_.find(name) != strategies_.end()) {
            LOG_WARNING("Strategy already exists: ", name);
            return;
        }

        strategies_[name] = strategy;
        LOG_INFO("Registered strategy: ", name);
    }

    void unregisterStrategy(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = strategies_.find(name);
        if (it != strategies_.end()) {
            if (it->second->getStatus() == StrategyStatus::RUNNING) {
                stopStrategy(name);
            }
            strategies_.erase(it);
            LOG_INFO("Unregistered strategy: ", name);
        }
    }

    void startStrategy(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = strategies_.find(name);
        if (it == strategies_.end()) {
            LOG_ERROR("Strategy not found: ", name);
            return;
        }

        try {
            it->second->initialize();
            it->second->start();
            LOG_INFO("Started strategy: ", name);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to start strategy ", name, ": ", e.what());
            throw;
        }
    }

    void stopStrategy(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = strategies_.find(name);
        if (it == strategies_.end()) {
            LOG_ERROR("Strategy not found: ", name);
            return;
        }

        try {
            it->second->stop();
            it->second->cleanup();
            LOG_INFO("Stopped strategy: ", name);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to stop strategy ", name, ": ", e.what());
            throw;
        }
    }

    void startAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, strategy] : strategies_) {
            try {
                if (strategy->getStatus() != StrategyStatus::RUNNING) {
                    strategy->initialize();
                    strategy->start();
                    LOG_INFO("Started strategy: ", name);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to start strategy ", name, ": ", e.what());
            }
        }
    }

    void stopAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, strategy] : strategies_) {
            try {
                if (strategy->getStatus() == StrategyStatus::RUNNING) {
                    strategy->stop();
                    strategy->cleanup();
                    LOG_INFO("Stopped strategy: ", name);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to stop strategy ", name, ": ", e.what());
            }
        }
    }

    StrategyStatus getStrategyStatus(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = strategies_.find(name);
        if (it == strategies_.end()) {
            throw std::runtime_error("Strategy not found: " + name);
        }
        return it->second->getStatus();
    }

    std::vector<std::string> getStrategyNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, _] : strategies_) {
            names.push_back(name);
        }
        return names;
    }

    void onMarketData(const MarketData& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [_, strategy] : strategies_) {
            if (strategy->getStatus() == StrategyStatus::RUNNING) {
                try {
                    strategy->onMarketData(data);
                } catch (const std::exception& e) {
                    LOG_ERROR("Error processing market data in strategy: ", e.what());
                }
            }
        }
    }

    void onOrderUpdate(const OrderUpdate& update) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [_, strategy] : strategies_) {
            if (strategy->getStatus() == StrategyStatus::RUNNING) {
                try {
                    strategy->onOrderUpdate(update);
                } catch (const std::exception& e) {
                    LOG_ERROR("Error processing order update in strategy: ", e.what());
                }
            }
        }
    }

    void onTradeUpdate(const TradeUpdate& update) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [_, strategy] : strategies_) {
            if (strategy->getStatus() == StrategyStatus::RUNNING) {
                try {
                    strategy->onTradeUpdate(update);
                } catch (const std::exception& e) {
                    LOG_ERROR("Error processing trade update in strategy: ", e.what());
                }
            }
        }
    }

private:
    std::shared_ptr<execution::ExecutionEngine> executionEngine_;
    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<StrategyInterface>> strategies_;
};

} // namespace algorithm
} // namespace quant_hub
