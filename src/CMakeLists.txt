set(SOURCES
    exchange/base_exchange.cpp
    exchange/binance_exchange.cpp
)

# Create library
add_library(quant_hub_lib ${SOURCES})

# Link dependencies
target_link_libraries(quant_hub_lib
    PRIVATE
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

# Set include directories
target_include_directories(quant_hub_lib
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
