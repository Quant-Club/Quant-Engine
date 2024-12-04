#include "algorithm/strategies/market_maker.hpp"
#include "execution/execution_engine.hpp"
#include "common/config.hpp"
#include "common/logger.hpp"

using namespace quant_hub;
using namespace algorithm;

int main() {
    try {
        // Initialize execution engine
        auto executionEngine = std::make_shared<execution::ExecutionEngine>();

        // Configure market maker strategy
        MarketMakerConfig config;
        config.spreadPercentage = 0.001;     // 0.1% base spread
        config.inventoryLimit = 1.0;         // Maximum 1 BTC inventory
        config.orderSize = 0.1;              // 0.1 BTC per order
        config.minSpread = 0.0005;          // Minimum 0.05% spread
        config.maxSpread = 0.005;           // Maximum 0.5% spread
        config.priceQueueSize = 100;        // Keep last 100 prices for volatility
        config.volMultiplier = 2.0;         // Volatility impact on spread
        config.skewFactor = 0.0005;         // Inventory skew factor

        // Create and initialize strategy
        auto strategy = std::make_shared<MarketMaker>("BTC-USDT-MM", executionEngine, config);

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
        }

        // Stop strategy and execution engine
        strategy->stop();
        executionEngine->stop();

        return 0;

    } catch (const std::exception& e) {
        LOG_ERROR("Strategy error: ", e.what());
        return 1;
    }
}
