#include "algorithm/strategies/stat_arbitrage.hpp"
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

        // Configure statistical arbitrage strategy
        StatArbitrageConfig config;
        config.lookbackPeriod = 100;       // 100-period lookback
        config.entryZScore = 2.0;          // Enter at 2 standard deviations
        config.exitZScore = 0.5;           // Exit at 0.5 standard deviations
        config.positionSize = 0.1;         // Base position size 0.1 BTC
        config.maxPositionSize = 1.0;      // Maximum position 1 BTC
        config.minObservations = 50;       // Minimum data points required
        config.corrThreshold = 0.7;        // Minimum correlation threshold
        config.maxSpreadValue = 0.1;       // Maximum spread value
        config.stopLossZScore = 3.0;       // Stop loss at 3 standard deviations

        // Create and initialize strategy
        auto strategy = std::make_shared<StatArbitrage>(
            "BTC-ETH-ARB",
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

            // Example: Print current positions
            if (input == "positions") {
                auto btcPos = strategy->getPosition("BTC-USDT");
                auto ethPos = strategy->getPosition("ETH-USDT");
                std::cout << "BTC Position: " << btcPos.volume << std::endl;
                std::cout << "ETH Position: " << ethPos.volume << std::endl;
            }

            // Example: Print pair statistics
            if (input == "stats") {
                auto stats = strategy->getPairStats("BTC-ETH");
                std::cout << "Correlation: " << stats.correlation << std::endl;
                std::cout << "Current Z-Score: " << stats.zScore << std::endl;
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
