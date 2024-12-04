#pragma once

#include <string>
#include <vector>
#include <memory>
#include "compute_engine.hpp"
#include "common/types.hpp"

namespace quant_hub {
namespace model {

class ComputeKernels {
public:
    static std::shared_ptr<ComputeKernels> create(std::shared_ptr<ComputeEngine> engine) {
        return std::shared_ptr<ComputeKernels>(new ComputeKernels(engine));
    }

    // Technical Analysis
    void movingAverage(const std::vector<double>& prices,
                      std::vector<double>& result,
                      int period) {
        size_t size = prices.size();
        if (size < period) return;

        try {
            engine_->allocateMemory(size * sizeof(double) * 2);
            engine_->copyToDevice(prices.data(), size * sizeof(double));

            std::vector<void*> args = {&prices, &result, &period};
            std::vector<size_t> globalSize = {size - period + 1, 1, 1};
            std::vector<size_t> localSize = {256, 1, 1};

            engine_->executeKernel("moving_average", args, globalSize, localSize);
            engine_->copyFromDevice(result.data(), (size - period + 1) * sizeof(double));

        } catch (const std::exception& e) {
            throw std::runtime_error("Moving average calculation failed: " + std::string(e.what()));
        }
    }

    void exponentialMovingAverage(const std::vector<double>& prices,
                                std::vector<double>& result,
                                double alpha) {
        size_t size = prices.size();
        if (size == 0) return;

        try {
            engine_->allocateMemory(size * sizeof(double) * 2);
            engine_->copyToDevice(prices.data(), size * sizeof(double));

            std::vector<void*> args = {&prices, &result, &alpha};
            std::vector<size_t> globalSize = {size, 1, 1};
            std::vector<size_t> localSize = {256, 1, 1};

            engine_->executeKernel("exponential_moving_average", args, globalSize, localSize);
            engine_->copyFromDevice(result.data(), size * sizeof(double));

        } catch (const std::exception& e) {
            throw std::runtime_error("EMA calculation failed: " + std::string(e.what()));
        }
    }

    void bollingerBands(const std::vector<double>& prices,
                       std::vector<double>& upperBand,
                       std::vector<double>& middleBand,
                       std::vector<double>& lowerBand,
                       int period,
                       double numStdDev) {
        size_t size = prices.size();
        if (size < period) return;

        try {
            engine_->allocateMemory(size * sizeof(double) * 4);
            engine_->copyToDevice(prices.data(), size * sizeof(double));

            std::vector<void*> args = {&prices, &upperBand, &middleBand, &lowerBand,
                                     &period, &numStdDev};
            std::vector<size_t> globalSize = {size - period + 1, 1, 1};
            std::vector<size_t> localSize = {256, 1, 1};

            engine_->executeKernel("bollinger_bands", args, globalSize, localSize);
            
            size_t resultSize = (size - period + 1) * sizeof(double);
            engine_->copyFromDevice(upperBand.data(), resultSize);
            engine_->copyFromDevice(middleBand.data(), resultSize);
            engine_->copyFromDevice(lowerBand.data(), resultSize);

        } catch (const std::exception& e) {
            throw std::runtime_error("Bollinger Bands calculation failed: " + std::string(e.what()));
        }
    }

    void relativeStrengthIndex(const std::vector<double>& prices,
                              std::vector<double>& rsi,
                              int period) {
        size_t size = prices.size();
        if (size < period + 1) return;

        try {
            engine_->allocateMemory(size * sizeof(double) * 2);
            engine_->copyToDevice(prices.data(), size * sizeof(double));

            std::vector<void*> args = {&prices, &rsi, &period};
            std::vector<size_t> globalSize = {size - period, 1, 1};
            std::vector<size_t> localSize = {256, 1, 1};

            engine_->executeKernel("rsi", args, globalSize, localSize);
            engine_->copyFromDevice(rsi.data(), (size - period) * sizeof(double));

        } catch (const std::exception& e) {
            throw std::runtime_error("RSI calculation failed: " + std::string(e.what()));
        }
    }

    // Options Pricing
    void blackScholes(const std::vector<OptionData>& options,
                     std::vector<double>& callPrices,
                     std::vector<double>& putPrices) {
        size_t size = options.size();
        if (size == 0) return;

        try {
            engine_->allocateMemory(size * (sizeof(OptionData) + sizeof(double) * 2));
            engine_->copyToDevice(options.data(), size * sizeof(OptionData));

            std::vector<void*> args = {&options, &callPrices, &putPrices};
            std::vector<size_t> globalSize = {size, 1, 1};
            std::vector<size_t> localSize = {256, 1, 1};

            engine_->executeKernel("black_scholes", args, globalSize, localSize);
            
            engine_->copyFromDevice(callPrices.data(), size * sizeof(double));
            engine_->copyFromDevice(putPrices.data(), size * sizeof(double));

        } catch (const std::exception& e) {
            throw std::runtime_error("Black-Scholes calculation failed: " + std::string(e.what()));
        }
    }

    void monteCarloSimulation(const std::vector<SimulationParams>& params,
                            std::vector<std::vector<double>>& paths,
                            int numPaths,
                            int numSteps) {
        size_t size = params.size();
        if (size == 0) return;

        try {
            size_t totalSize = size * numPaths * numSteps;
            engine_->allocateMemory(totalSize * sizeof(double) + size * sizeof(SimulationParams));
            engine_->copyToDevice(params.data(), size * sizeof(SimulationParams));

            std::vector<void*> args = {&params, &paths, &numPaths, &numSteps};
            std::vector<size_t> globalSize = {size * numPaths, 1, 1};
            std::vector<size_t> localSize = {256, 1, 1};

            engine_->executeKernel("monte_carlo", args, globalSize, localSize);
            
            for (size_t i = 0; i < size; ++i) {
                engine_->copyFromDevice(paths[i].data(), numPaths * numSteps * sizeof(double));
            }

        } catch (const std::exception& e) {
            throw std::runtime_error("Monte Carlo simulation failed: " + std::string(e.what()));
        }
    }

    // Portfolio Analysis
    void portfolioOptimization(const std::vector<double>& returns,
                             const std::vector<double>& covariance,
                             std::vector<double>& weights,
                             double riskFreeRate,
                             double targetReturn) {
        size_t numAssets = weights.size();
        if (numAssets == 0) return;

        try {
            size_t covSize = numAssets * numAssets;
            engine_->allocateMemory((covSize + numAssets * 2) * sizeof(double));
            engine_->copyToDevice(returns.data(), numAssets * sizeof(double));
            engine_->copyToDevice(covariance.data(), covSize * sizeof(double));

            std::vector<void*> args = {&returns, &covariance, &weights,
                                     &riskFreeRate, &targetReturn, &numAssets};
            std::vector<size_t> globalSize = {numAssets, 1, 1};
            std::vector<size_t> localSize = {256, 1, 1};

            engine_->executeKernel("portfolio_optimization", args, globalSize, localSize);
            engine_->copyFromDevice(weights.data(), numAssets * sizeof(double));

        } catch (const std::exception& e) {
            throw std::runtime_error("Portfolio optimization failed: " + std::string(e.what()));
        }
    }

    void valueAtRisk(const std::vector<double>& returns,
                    const std::vector<double>& weights,
                    double& var,
                    double confidence,
                    int horizon) {
        size_t size = returns.size();
        if (size == 0) return;

        try {
            engine_->allocateMemory(size * sizeof(double) * 2);
            engine_->copyToDevice(returns.data(), size * sizeof(double));
            engine_->copyToDevice(weights.data(), weights.size() * sizeof(double));

            std::vector<void*> args = {&returns, &weights, &var, &confidence, &horizon};
            std::vector<size_t> globalSize = {size, 1, 1};
            std::vector<size_t> localSize = {256, 1, 1};

            engine_->executeKernel("value_at_risk", args, globalSize, localSize);
            engine_->copyFromDevice(&var, sizeof(double));

        } catch (const std::exception& e) {
            throw std::runtime_error("VaR calculation failed: " + std::string(e.what()));
        }
    }

private:
    ComputeKernels(std::shared_ptr<ComputeEngine> engine) : engine_(engine) {
        if (!engine) {
            throw std::runtime_error("Null compute engine provided");
        }
    }

    std::shared_ptr<ComputeEngine> engine_;
};

} // namespace model
} // namespace quant_hub
