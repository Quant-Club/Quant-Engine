#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <fstream>
#include "strategy_interface.hpp"
#include "common/logger.hpp"
#include "common/config.hpp"

namespace quant_hub {
namespace algorithm {

struct BacktestConfig {
    std::string dataDir;
    std::string startDate;
    std::string endDate;
    double initialCapital;
    std::vector<std::string> symbols;
    std::map<std::string, double> tradingFees;
    std::map<std::string, double> slippage;
};

struct BacktestResult {
    double finalCapital;
    double totalReturn;
    double sharpeRatio;
    double maxDrawdown;
    int totalTrades;
    int winningTrades;
    double winRate;
    double averageWin;
    double averageLoss;
    std::vector<std::pair<Timestamp, double>> equityCurve;
    std::vector<TradeUpdate> trades;
};

class BacktestEngine {
public:
    BacktestEngine(const BacktestConfig& config)
        : config_(config)
        , currentTime_(0)
        , currentCapital_(config.initialCapital)
        , peakCapital_(config.initialCapital)
    {
        loadMarketData();
        LOG_INFO("Backtest engine initialized with ", 
                 marketData_.size(), " market data points");
    }

    BacktestResult run(std::shared_ptr<StrategyInterface> strategy) {
        LOG_INFO("Starting backtest for strategy: ", strategy->getName());
        
        try {
            // Initialize strategy
            strategy->initialize();
            strategy->start();

            // Main backtest loop
            while (!eventQueue_.empty()) {
                auto event = eventQueue_.top();
                eventQueue_.pop();

                currentTime_ = event.timestamp;
                processEvent(event, strategy);
                updateMetrics();
            }

            // Cleanup
            strategy->stop();
            strategy->cleanup();

            // Calculate results
            BacktestResult result = calculateResults();
            LOG_INFO("Backtest completed. Final capital: ", result.finalCapital);
            
            return result;

        } catch (const std::exception& e) {
            LOG_ERROR("Backtest failed: ", e.what());
            throw;
        }
    }

private:
    void loadMarketData() {
        for (const auto& symbol : config_.symbols) {
            std::string filename = config_.dataDir + "/" + symbol + ".csv";
            loadSymbolData(symbol, filename);
        }

        LOG_INFO("Loaded market data for ", config_.symbols.size(), " symbols");
    }

    void loadSymbolData(const std::string& symbol, const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }

        std::string line;
        std::getline(file, line); // Skip header

        while (std::getline(file, line)) {
            MarketData data = parseMarketDataLine(line, symbol);
            if (isWithinDateRange(data.timestamp)) {
                marketData_.push_back(data);
                eventQueue_.push(createEvent(EventType::MARKET_DATA, data));
            }
        }
    }

    void processEvent(const Event& event, std::shared_ptr<StrategyInterface> strategy) {
        switch (event.type) {
            case EventType::MARKET_DATA:
                processMarketData(event, strategy);
                break;
            case EventType::ORDER_UPDATE:
                processOrderUpdate(event, strategy);
                break;
            case EventType::TRADE_UPDATE:
                processTradeUpdate(event, strategy);
                break;
            default:
                LOG_WARNING("Unknown event type: ", static_cast<int>(event.type));
        }
    }

    void processMarketData(const Event& event, std::shared_ptr<StrategyInterface> strategy) {
        MarketData data = parseMarketData(event.data);
        lastPrice_[data.symbol] = data.lastPrice;
        strategy->onMarketData(data);
    }

    void processOrderUpdate(const Event& event, std::shared_ptr<StrategyInterface> strategy) {
        OrderUpdate update = parseOrderUpdate(event.data);
        strategy->onOrderUpdate(update);

        if (update.status == OrderStatus::FILLED) {
            double cost = calculateTradeCost(update);
            currentCapital_ -= cost;
            
            TradeUpdate trade;
            trade.orderId = update.orderId;
            trade.symbol = update.symbol;
            trade.price = update.filledPrice;
            trade.volume = update.filledVolume;
            trade.timestamp = currentTime_;
            
            trades_.push_back(trade);
            strategy->onTradeUpdate(trade);
        }
    }

    void processTradeUpdate(const Event& event, std::shared_ptr<StrategyInterface> strategy) {
        TradeUpdate update = parseTradeUpdate(event.data);
        strategy->onTradeUpdate(update);
    }

    void updateMetrics() {
        // Update equity curve
        double totalEquity = currentCapital_;
        for (const auto& [symbol, position] : positions_) {
            auto it = lastPrice_.find(symbol);
            if (it != lastPrice_.end()) {
                totalEquity += position.volume * it->second;
            }
        }

        equityCurve_.emplace_back(currentTime_, totalEquity);
        
        // Update peak capital
        if (totalEquity > peakCapital_) {
            peakCapital_ = totalEquity;
        }
    }

    BacktestResult calculateResults() {
        BacktestResult result;
        
        // Basic metrics
        result.finalCapital = currentCapital_;
        result.totalReturn = (currentCapital_ - config_.initialCapital) / config_.initialCapital;
        result.totalTrades = trades_.size();
        
        // Calculate winning trades
        result.winningTrades = std::count_if(trades_.begin(), trades_.end(),
            [](const TradeUpdate& trade) {
                return trade.price > trade.averagePrice;
            });
        
        result.winRate = static_cast<double>(result.winningTrades) / result.totalTrades;
        
        // Calculate Sharpe ratio
        result.sharpeRatio = calculateSharpeRatio();
        
        // Calculate maximum drawdown
        result.maxDrawdown = calculateMaxDrawdown();
        
        // Calculate average win/loss
        calculateAverageWinLoss(result);
        
        // Store equity curve and trades
        result.equityCurve = equityCurve_;
        result.trades = trades_;
        
        return result;
    }

    double calculateTradeCost(const OrderUpdate& update) {
        double cost = update.filledPrice * update.filledVolume;
        
        // Add trading fees
        auto feeIt = config_.tradingFees.find(update.symbol);
        if (feeIt != config_.tradingFees.end()) {
            cost *= (1 + feeIt->second);
        }
        
        // Add slippage
        auto slippageIt = config_.slippage.find(update.symbol);
        if (slippageIt != config_.slippage.end()) {
            cost *= (1 + slippageIt->second);
        }
        
        return cost;
    }

    double calculateSharpeRatio() {
        if (equityCurve_.size() < 2) return 0.0;

        std::vector<double> returns;
        for (size_t i = 1; i < equityCurve_.size(); ++i) {
            double ret = (equityCurve_[i].second - equityCurve_[i-1].second) / 
                         equityCurve_[i-1].second;
            returns.push_back(ret);
        }

        double meanReturn = std::accumulate(returns.begin(), returns.end(), 0.0) / 
                          returns.size();
        
        double variance = 0.0;
        for (double ret : returns) {
            variance += std::pow(ret - meanReturn, 2);
        }
        variance /= returns.size();

        return meanReturn / std::sqrt(variance);
    }

    double calculateMaxDrawdown() {
        double maxDrawdown = 0.0;
        double peak = config_.initialCapital;
        
        for (const auto& [_, equity] : equityCurve_) {
            if (equity > peak) {
                peak = equity;
            }
            double drawdown = (peak - equity) / peak;
            maxDrawdown = std::max(maxDrawdown, drawdown);
        }
        
        return maxDrawdown;
    }

    void calculateAverageWinLoss(BacktestResult& result) {
        double totalWins = 0.0;
        double totalLosses = 0.0;
        int winCount = 0;
        int lossCount = 0;

        for (const auto& trade : trades_) {
            double pnl = trade.price - trade.averagePrice;
            if (pnl > 0) {
                totalWins += pnl;
                winCount++;
            } else {
                totalLosses += std::abs(pnl);
                lossCount++;
            }
        }

        result.averageWin = winCount > 0 ? totalWins / winCount : 0.0;
        result.averageLoss = lossCount > 0 ? totalLosses / lossCount : 0.0;
    }

    BacktestConfig config_;
    std::vector<MarketData> marketData_;
    std::priority_queue<Event> eventQueue_;
    
    Timestamp currentTime_;
    double currentCapital_;
    double peakCapital_;
    
    std::map<std::string, double> lastPrice_;
    std::map<std::string, Position> positions_;
    std::vector<TradeUpdate> trades_;
    std::vector<std::pair<Timestamp, double>> equityCurve_;
};

} // namespace algorithm
} // namespace quant_hub
