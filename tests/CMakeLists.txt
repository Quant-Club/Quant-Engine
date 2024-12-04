find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(TEST_SOURCES
    exchange/base_exchange_test.cpp
    exchange/binance_exchange_test.cpp
)

# Create test executable
add_executable(quant_hub_tests ${TEST_SOURCES})

# Link dependencies
target_link_libraries(quant_hub_tests
    PRIVATE
    quant_hub_lib
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    pthread
)

# Register tests
gtest_discover_tests(quant_hub_tests)
