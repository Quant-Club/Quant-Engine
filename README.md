# QuantHub Trading System

A high-performance, cross-platform automated trading system written in C++.

## Core Components

1. Exchange API Layer (based on CCXT)
   - Unified interface for multiple exchanges
   - Real-time market data subscription
   - Order management and execution

2. Algorithm Engine (inspired by QuantLib)
   - Strategy management
   - Backtesting engine
   - Real-time trading engine

3. Execution Engine (based on Disruptor pattern)
   - Event-driven architecture
   - High-performance message passing
   - Buffer management

4. Model Engine (CUDA/OpenCL)
   - Parallel computing for intensive calculations
   - GPU acceleration
   - Cross-platform computation

## Project Structure

```
quant_hub/
├── src/
│   ├── exchange/       # Exchange API implementations
│   ├── algorithm/      # Trading algorithms and strategies
│   ├── execution/      # Event processing and execution
│   ├── model/         # Computation models and GPU acceleration
│   ├── common/        # Common utilities and data structures
│   └── main.cpp       # Main entry point
├── include/           # Public headers
├── tests/            # Unit tests and integration tests
├── examples/         # Example strategies and usage
└── docs/            # Documentation
```

## Build Requirements

- C++17 or later
- CMake 3.15+
- CUDA Toolkit (optional, for GPU acceleration)
- OpenCL (optional, for GPU acceleration)
- Boost
- OpenSSL
- RapidJSON
- GoogleTest (for testing)

## Features

- Low-latency event processing
- Cross-platform compatibility
- Multiple exchange support
- Real-time and historical data processing
- GPU-accelerated computations
- Flexible strategy implementation
- Comprehensive backtesting capabilities
