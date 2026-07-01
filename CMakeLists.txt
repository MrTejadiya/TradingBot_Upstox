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
    src/app/order_display.cpp
    src/infra/config.cpp
    src/infra/credentials.cpp
    src/infra/candle_service.cpp
    src/infra/instrument_csv.cpp
    src/infra/logger.cpp
    src/infra/market_feed.cpp
    src/infra/market_quote_service.cpp
    src/infra/portfolio_sync_service.cpp
    src/infra/time_utils.cpp
    src/infra/upstox_api_client.cpp
    src/order/dry_run_dispatcher.cpp
    src/order/live_order_dispatcher.cpp
    src/order/order_monitor.cpp
    src/order/order_queue.cpp
    src/order/rate_limited_executor.cpp
    src/paper/backtest_runner.cpp
    src/paper/paper_portfolio_simulator.cpp
    src/persistence/api_event_mapper.cpp
    src/persistence/candle_mapper.cpp
    src/persistence/decision_mapper.cpp
    src/persistence/instrument_mapper.cpp
    src/persistence/order_history_mapper.cpp
    src/persistence/persistence_worker.cpp
    src/persistence/quote_snapshot_mapper.cpp
    src/persistence/risk_event_mapper.cpp
    src/persistence/sqlite_migrations.cpp
    src/persistence/strategy_signal_mapper.cpp
    src/runtime/worker_group.cpp
    src/strategy/advanced_buy_strategies.cpp
    src/strategy/buy_strategies.cpp
    src/strategy/exit_engine.cpp
    src/strategy/indicators.cpp
    src/strategy/kill_switch.cpp
    src/strategy/market_session.cpp
    src/strategy/risk_manager.cpp
    src/strategy/sell_strategies.cpp
    src/strategy/signal_aggregation.cpp
    src/strategy/strategy.cpp
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
