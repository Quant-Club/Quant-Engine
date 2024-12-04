#include "algorithm/strategies/trend_follower.hpp"
#include "execution/execution_engine.hpp"
#include "model/cuda_engine.hpp"
#include "common/config.hpp"
#include "common/logger.hpp"

using namespace quant_hub;
using namespace algorithm;

int main() {
    try {
        // Initialize execution engine
        auto executionEngine = std::make_shared<execution::ExecutionEngine>();

        // Initialize compute engine (CUDA)
        auto computeEngine = model::CudaEngine::create();
        computeEngine->initialize();

        // Configure trend following strategy
        TrendFollowerConfig config;
        config.shortPeriod = 20;           // 20-period short MA
        config.longPeriod = 50;            // 50-period long MA
        config.positionSize = 0.1;         // Base position size 0.1 BTC
        config.maxPositionSize = 1.0;      // Maximum position 1 BTC
        config.stopLossPercent = 0.02;     // 2% stop loss
        config.takeProfitPercent = 0.05;   // 5% take profit
        config.atrPeriod = 14;             // 14-period ATR
        config.atrMultiplier = 1.5;        // ATR multiplier for position sizing

        // Create and initialize strategy
        auto strategy = std::make_shared<TrendFollower>(
            "BTC-USDT-TREND",
            executionEngine,
            computeEngine,
            config
        );

        // Start execution engine
        executionEngine->start();

        // Initialize and start strategy
        strategy->initialize();
        strategy->start();

        // Main loop
        std::string input;
        std::cout << "Strategy running. Press 'q' to quit." << std::endl;
        while (std::getline(std::cin, input)) {
            if (input == "q") break;

            // Example: Print strategy status
            if (input == "status") {
                std::cout << "Strategy Status: " 
                         << static_cast<int>(strategy->getStatus()) << std::endl;
            }

            // Example: Print current position
            if (input == "position") {
                auto position = strategy->getPosition("BTC-USDT");
                std::cout << "Current Position: " << position.volume << std::endl;
            }
        }

        // Stop strategy and engines
        strategy->stop();
        executionEngine->stop();
        computeEngine->shutdown();

        return 0;

    } catch (const std::exception& e) {
        LOG_ERROR("Strategy error: ", e.what());
        return 1;
    }
}
