cmake_minimum_required(VERSION 3.20)

project(TradingBotUpstox
    VERSION 0.1.0
    DESCRIPTION "C++ Upstox delivery trading bot"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TRADINGBOT_BUILD_TESTS "Build TradingBot Upstox tests" ON)

add_library(tradingbot_core
    src/core/domain.cpp
    src/app/cli.cpp
    src/app/operating_mode.cpp
    src/infra/config.cpp
    src/infra/credentials.cpp
    src/infra/instrument_csv.cpp
    src/infra/logger.cpp
    src/infra/upstox_api_client.cpp
)

target_include_directories(tradingbot_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(tradingbot_core PUBLIC cxx_std_20)

add_executable(tradingbot_upstox
    src/app/main.cpp
)

target_link_libraries(tradingbot_upstox PRIVATE tradingbot_core)

if(TRADINGBOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
