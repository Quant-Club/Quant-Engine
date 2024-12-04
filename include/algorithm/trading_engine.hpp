#pragma once

#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include "strategy_manager.hpp"
#include "execution/execution_engine.hpp"
#include "common/logger.hpp"
#include "common/config.hpp"

namespace quant_hub {
namespace algorithm {

struct TradingEngineConfig {
    bool enableRiskManager;
    std::vector<std::string> activeExchanges;
    std::map<std::string, std::string> exchangeCredentials;
    std::map<std::string, std::vector<std::string>> symbolsByExchange;
};

class TradingEngine {
public:
    TradingEngine(const TradingEngineConfig& config)
        : config_(config)
        , executionEngine_(std::make_shared<execution::ExecutionEngine>())
        , strategyManager_(std::make_shared<StrategyManager>(executionEngine_))
        , running_(false)
    {
        initialize();
        LOG_INFO("Trading engine initialized");
    }

    ~TradingEngine() {
        if (running_) {
            stop();
        }
    }

    void start() {
        if (running_) return;
        
        try {
            // Start execution engine
            executionEngine_->start();

            // Start market data subscriptions
            for (const auto& exchange : config_.activeExchanges) {
                auto symbols = config_.symbolsByExchange.find(exchange);
                if (symbols != config_.symbolsByExchange.end()) {
                    for (const auto& symbol : symbols->second) {
                        subscribeToMarketData(symbol, exchange);
                    }
                }
            }

            // Start strategies
            strategyManager_->startAll();

            running_ = true;
            monitorThread_ = std::thread(&TradingEngine::monitor, this);
            LOG_INFO("Trading engine started");

        } catch (const std::exception& e) {
            LOG_ERROR("Failed to start trading engine: ", e.what());
            cleanup();
            throw;
        }
    }

    void stop() {
        if (!running_) return;
        
        try {
            running_ = false;
            
            // Stop strategies
            strategyManager_->stopAll();

            // Stop execution engine
            executionEngine_->stop();

            // Stop monitor thread
            if (monitorThread_.joinable()) {
                monitorThread_.join();
            }

            cleanup();
            LOG_INFO("Trading engine stopped");

        } catch (const std::exception& e) {
            LOG_ERROR("Error stopping trading engine: ", e.what());
            throw;
        }
    }

    void addStrategy(const std::string& name,
                    std::shared_ptr<StrategyInterface> strategy) {
        strategyManager_->registerStrategy(name, strategy);
    }

    void removeStrategy(const std::string& name) {
        strategyManager_->unregisterStrategy(name);
    }

    std::vector<std::string> getActiveStrategies() const {
        return strategyManager_->getStrategyNames();
    }

    StrategyStatus getStrategyStatus(const std::string& name) const {
        return strategyManager_->getStrategyStatus(name);
    }

    void enableStrategy(const std::string& name) {
        if (running_) {
            strategyManager_->startStrategy(name);
        }
    }

    void disableStrategy(const std::string& name) {
        if (running_) {
            strategyManager_->stopStrategy(name);
        }
    }

    std::shared_ptr<execution::ExecutionEngine> getExecutionEngine() {
        return executionEngine_;
    }

private:
    void initialize() {
        // Initialize exchanges
        for (const auto& exchange : config_.activeExchanges) {
            auto credentials = config_.exchangeCredentials.find(exchange);
            if (credentials == config_.exchangeCredentials.end()) {
                throw std::runtime_error("Missing credentials for exchange: " + exchange);
            }

            initializeExchange(exchange, credentials->second);
        }

        // Configure risk management
        if (config_.enableRiskManager) {
            executionEngine_->enableRiskManager();
        } else {
            executionEngine_->disableRiskManager();
        }
    }

    void initializeExchange(const std::string& name, const std::string& credentials) {
        // Create exchange instance based on name
        std::shared_ptr<exchange::ExchangeInterface> exchange;
        
        if (name == "Binance") {
            // Parse credentials
            auto [apiKey, secretKey] = parseCredentials(credentials);
            exchange = std::make_shared<exchange::BinanceExchange>(apiKey, secretKey);
        }
        // Add more exchanges here
        
        if (!exchange) {
            throw std::runtime_error("Unsupported exchange: " + name);
        }

        executionEngine_->registerExchange(name, exchange);
        LOG_INFO("Initialized exchange: ", name);
    }

    void subscribeToMarketData(const std::string& symbol,
                              const std::string& exchange) {
        executionEngine_->subscribeToMarketData(
            symbol,
            exchange,
            [this](const MarketData& data) {
                onMarketData(data);
            }
        );
        LOG_INFO("Subscribed to market data: ", symbol, " on ", exchange);
    }

    void onMarketData(const MarketData& data) {
        try {
            strategyManager_->onMarketData(data);
        } catch (const std::exception& e) {
            LOG_ERROR("Error processing market data: ", e.what());
        }
    }

    void monitor() {
        while (running_) {
            try {
                // Check strategy health
                for (const auto& name : strategyManager_->getStrategyNames()) {
                    auto status = strategyManager_->getStrategyStatus(name);
                    if (status == StrategyStatus::ERROR) {
                        LOG_ERROR("Strategy error detected: ", name);
                        // Implement error handling logic
                    }
                }

                // Monitor system resources
                monitorSystemResources();

                // Sleep for monitoring interval
                std::this_thread::sleep_for(std::chrono::seconds(1));

            } catch (const std::exception& e) {
                LOG_ERROR("Error in monitor thread: ", e.what());
            }
        }
    }

    void monitorSystemResources() {
        // Monitor CPU usage
        // Monitor memory usage
        // Monitor network latency
        // Implement system monitoring logic
    }

    void cleanup() {
        // Cleanup resources
        // Close connections
        // Save state if needed
    }

    std::pair<std::string, std::string> parseCredentials(const std::string& credentials) {
        // Parse credentials string into API key and secret
        // Implementation depends on credentials format
        return {"api_key", "secret_key"};
    }

    TradingEngineConfig config_;
    std::shared_ptr<execution::ExecutionEngine> executionEngine_;
    std::shared_ptr<StrategyManager> strategyManager_;
    
    std::atomic<bool> running_;
    std::thread monitorThread_;
};

} // namespace algorithm
} // namespace quant_hub
