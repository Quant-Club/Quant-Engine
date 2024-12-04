#pragma once

#include <vector>
#include <deque>
#include <map>
#include "algorithm/base_strategy.hpp"
#include "common/types.hpp"
#include "common/logger.hpp"
#include "model/kernels.hpp"

namespace quant_hub {
namespace algorithm {

struct StatArbitrageConfig {
    int lookbackPeriod;        // Period for calculating correlation and spread
    double entryZScore;        // Z-score threshold for entry
    double exitZScore;         // Z-score threshold for exit
    double positionSize;       // Base position size
    double maxPositionSize;    // Maximum position size per pair
    int minObservations;      // Minimum observations before trading
    double corrThreshold;     // Minimum correlation threshold
    double maxSpreadValue;    // Maximum absolute spread value
    double stopLossZScore;    // Stop loss z-score threshold
};

struct PairState {
    std::deque<double> spreadHistory;
    double meanSpread;
    double stdSpread;
    double currentSpread;
    double correlation;
    double beta;
    double position1;
    double position2;
    double entrySpread;
};

class StatArbitrage : public BaseStrategy {
public:
    StatArbitrage(const std::string& name,
                 std::shared_ptr<execution::ExecutionEngine> executionEngine,
                 std::shared_ptr<model::ComputeEngine> computeEngine,
                 const StatArbitrageConfig& config)
        : BaseStrategy(name, executionEngine)
        , computeEngine_(computeEngine)
        , config_(config)
        , computeKernels_(model::ComputeKernels::create(computeEngine))
    {
        LOG_INFO("Statistical Arbitrage strategy created: ", name);
    }

protected:
    void onInitialize() override {
        // Reset state
        pairStates_.clear();
        
        LOG_INFO("Statistical Arbitrage initialized");
    }

    void onStart() override {
        // Initial analysis if we have enough data
        for (const auto& pair : pairStates_) {
            if (hasEnoughData(pair.second)) {
                updatePairStats(pair.first, pair.second);
                checkSignals(pair.first, pair.second);
            }
        }
        
        LOG_INFO("Statistical Arbitrage started");
    }

    void onStop() override {
        // Close all positions
        closeAllPositions();
        
        LOG_INFO("Statistical Arbitrage stopped");
    }

    void processMarketData(const MarketData& data) override {
        try {
            // Update pair data
            updatePairData(data);

            // Check for trading signals
            for (auto& [pairId, state] : pairStates_) {
                if (hasEnoughData(state)) {
                    updatePairStats(pairId, state);
                    checkSignals(pairId, state);
                }
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
        // Check position limits for all pairs
        for (const auto& [_, state] : pairStates_) {
            if (std::abs(state.position1) > config_.maxPositionSize ||
                std::abs(state.position2) > config_.maxPositionSize) {
                LOG_WARNING("Position size limit exceeded");
                return false;
            }
        }
        return true;
    }

    void onUpdateRiskMetrics() override {
        // Update risk metrics for all pairs
        calculatePortfolioRisk();
    }

private:
    void updatePairData(const MarketData& data) {
        // Find the corresponding pair and update its state
        for (auto& [pairId, state] : pairStates_) {
            if (isPairComponent(pairId, data.symbol)) {
                updateSpreadHistory(pairId, state, data);
            }
        }
    }

    void updateSpreadHistory(const std::string& pairId,
                           PairState& state,
                           const MarketData& data) {
        // Update spread calculation
        if (state.spreadHistory.size() >= config_.lookbackPeriod) {
            state.spreadHistory.pop_front();
        }
        
        // Calculate and store new spread value
        double newSpread = calculateSpread(pairId, data.lastPrice);
        state.spreadHistory.push_back(newSpread);
        state.currentSpread = newSpread;
    }

    void updatePairStats(const std::string& pairId, PairState& state) {
        try {
            std::vector<double> spreads(state.spreadHistory.begin(),
                                      state.spreadHistory.end());
            
            // Calculate mean and standard deviation
            double sum = 0.0;
            double sumSq = 0.0;
            for (double spread : spreads) {
                sum += spread;
                sumSq += spread * spread;
            }
            
            state.meanSpread = sum / spreads.size();
            state.stdSpread = std::sqrt(sumSq / spreads.size() - 
                                      state.meanSpread * state.meanSpread);

            // Calculate correlation and beta
            calculatePairMetrics(pairId, state);

        } catch (const std::exception& e) {
            LOG_ERROR("Error updating pair stats: ", e.what());
            throw;
        }
    }

    void calculatePairMetrics(const std::string& pairId, PairState& state) {
        // Get price histories for both assets
        auto [prices1, prices2] = getPairPrices(pairId);
        
        if (prices1.size() != prices2.size() || 
            prices1.size() < config_.minObservations) {
            return;
        }

        // Calculate correlation and beta using compute engine
        std::vector<double> returns1 = calculateReturns(prices1);
        std::vector<double> returns2 = calculateReturns(prices2);
        
        // Use compute kernels for calculations
        state.correlation = calculateCorrelation(returns1, returns2);
        state.beta = calculateBeta(returns1, returns2);
    }

    void checkSignals(const std::string& pairId, PairState& state) {
        if (!hasEnoughData(state) || 
            std::abs(state.correlation) < config_.corrThreshold) {
            return;
        }

        double zScore = (state.currentSpread - state.meanSpread) / state.stdSpread;

        // Check for entry signals
        if (std::abs(state.position1) < 0.0001 && 
            std::abs(state.position2) < 0.0001) {
            if (zScore > config_.entryZScore) {
                enterPairTrade(pairId, false); // Short the spread
            } else if (zScore < -config_.entryZScore) {
                enterPairTrade(pairId, true);  // Long the spread
            }
        }
        // Check for exit signals
        else {
            bool isLongSpread = state.position1 > 0;
            if ((isLongSpread && zScore >= -config_.exitZScore) ||
                (!isLongSpread && zScore <= config_.exitZScore) ||
                std::abs(zScore) > config_.stopLossZScore) {
                exitPairTrade(pairId);
            }
        }
    }

    void enterPairTrade(const std::string& pairId, bool isLongSpread) {
        if (!onCheckRiskLimits()) return;

        auto& state = pairStates_[pairId];
        double positionSize = calculatePositionSize(pairId);

        // Place orders for both legs
        Order order1, order2;
        order1.volume = positionSize;
        order2.volume = positionSize * state.beta;

        if (isLongSpread) {
            order1.side = OrderSide::BUY;
            order2.side = OrderSide::SELL;
        } else {
            order1.side = OrderSide::SELL;
            order2.side = OrderSide::BUY;
        }

        auto [symbol1, symbol2] = getPairSymbols(pairId);
        
        OrderId orderId1 = submitOrder(order1, symbol1);
        OrderId orderId2 = submitOrder(order2, symbol2);

        state.entrySpread = state.currentSpread;
        
        LOG_INFO("Entered pair trade: ", pairId,
                 " Direction: ", isLongSpread ? "Long" : "Short",
                 " Orders: ", orderId1, ", ", orderId2);
    }

    void exitPairTrade(const std::string& pairId) {
        auto& state = pairStates_[pairId];
        if (std::abs(state.position1) < 0.0001 && 
            std::abs(state.position2) < 0.0001) {
            return;
        }

        // Place closing orders
        Order order1, order2;
        order1.volume = std::abs(state.position1);
        order2.volume = std::abs(state.position2);
        
        order1.side = state.position1 > 0 ? OrderSide::SELL : OrderSide::BUY;
        order2.side = state.position2 > 0 ? OrderSide::SELL : OrderSide::BUY;

        auto [symbol1, symbol2] = getPairSymbols(pairId);
        
        OrderId orderId1 = submitOrder(order1, symbol1);
        OrderId orderId2 = submitOrder(order2, symbol2);

        LOG_INFO("Exited pair trade: ", pairId,
                 " Orders: ", orderId1, ", ", orderId2);
    }

    void closeAllPositions() {
        for (const auto& [pairId, _] : pairStates_) {
            exitPairTrade(pairId);
        }
    }

    void handleFill(const OrderUpdate& update) {
        // Find corresponding pair and update positions
        for (auto& [pairId, state] : pairStates_) {
            auto [symbol1, symbol2] = getPairSymbols(pairId);
            
            if (update.symbol == symbol1) {
                updatePosition(state.position1, update);
            } else if (update.symbol == symbol2) {
                updatePosition(state.position2, update);
            }
        }
    }

    void updatePosition(double& position, const OrderUpdate& update) {
        if (update.side == OrderSide::BUY) {
            position += update.filledVolume;
        } else {
            position -= update.filledVolume;
        }
    }

    double calculatePositionSize(const std::string& pairId) {
        const auto& state = pairStates_[pairId];
        
        // Base position size adjusted for volatility
        double size = config_.positionSize;
        
        // Adjust for spread volatility
        if (state.stdSpread > 0) {
            size *= (1.0 / state.stdSpread);
        }
        
        // Cap at maximum size
        return std::min(size, config_.maxPositionSize);
    }

    void calculatePortfolioRisk() {
        double totalExposure = 0.0;
        double totalPnL = 0.0;

        for (const auto& [pairId, state] : pairStates_) {
            // Calculate exposure
            double exposure1 = std::abs(state.position1);
            double exposure2 = std::abs(state.position2);
            totalExposure += exposure1 + exposure2;

            // Calculate P&L
            double spreadPnL = (state.currentSpread - state.entrySpread) *
                             (state.position1 > 0 ? 1 : -1);
            totalPnL += spreadPnL;
        }

        // Log risk metrics
        LOG_INFO("Portfolio Risk Metrics - Total Exposure: ", totalExposure,
                 " Total P&L: ", totalPnL);
    }

    bool hasEnoughData(const PairState& state) {
        return state.spreadHistory.size() >= config_.minObservations;
    }

    // Helper functions
    std::pair<std::string, std::string> getPairSymbols(const std::string& pairId) {
        // Implementation depends on pair ID format
        return {"", ""};
    }

    std::pair<std::vector<double>, std::vector<double>> getPairPrices(
        const std::string& pairId) {
        // Implementation to retrieve price histories
        return {{}, {}};
    }

    bool isPairComponent(const std::string& pairId, const std::string& symbol) {
        auto [symbol1, symbol2] = getPairSymbols(pairId);
        return symbol == symbol1 || symbol == symbol2;
    }

    double calculateSpread(const std::string& pairId, double price) {
        // Implementation of spread calculation
        return 0.0;
    }

    std::vector<double> calculateReturns(const std::vector<double>& prices) {
        std::vector<double> returns;
        returns.reserve(prices.size() - 1);
        
        for (size_t i = 1; i < prices.size(); ++i) {
            returns.push_back((prices[i] - prices[i-1]) / prices[i-1]);
        }
        
        return returns;
    }

    double calculateCorrelation(const std::vector<double>& x,
                              const std::vector<double>& y) {
        // Implementation using compute kernels
        return 0.0;
    }

    double calculateBeta(const std::vector<double>& x,
                        const std::vector<double>& y) {
        // Implementation using compute kernels
        return 0.0;
    }

    std::shared_ptr<model::ComputeEngine> computeEngine_;
    std::shared_ptr<model::ComputeKernels> computeKernels_;
    StatArbitrageConfig config_;
    std::map<std::string, PairState> pairStates_;
};

} // namespace algorithm
} // namespace quant_hub
