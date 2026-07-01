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

find_file(SQLite3_RUNTIME_LIBRARY
    NAMES sqlite3.dll libsqlite3-0.dll
    HINTS ENV SQLite3_ROOT
    PATHS
        "C:/Users/Ravi Kumar/Downloads/python-3.13.13-embed-amd64"
        "C:/Users/Ravi Kumar/Downloads/python-3.10.1-embed-amd64"
        C:/msys64/ucrt64
    PATH_SUFFIXES bin
)

if(WIN32 AND NOT SQLite3_RUNTIME_LIBRARY)
    message(FATAL_ERROR "SQLite3 runtime DLL was not found. Install SQLite3 or set SQLite3_ROOT.")
endif()

add_library(tradingbot_core
    src/core/domain.cpp
    src/app/app_runner.cpp
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
    src/persistence/audit_event_mapper.cpp
    src/persistence/api_event_mapper.cpp
    src/persistence/bot_run_mapper.cpp
    src/persistence/candle_mapper.cpp
    src/persistence/decision_mapper.cpp
    src/persistence/instrument_mapper.cpp
    src/persistence/order_history_mapper.cpp
    src/persistence/persistence_worker.cpp
    src/persistence/quote_snapshot_mapper.cpp
    src/persistence/risk_event_mapper.cpp
    src/persistence/sqlite_candle_cache.cpp
    src/persistence/sqlite_database.cpp
    src/persistence/sqlite_instrument_store.cpp
    src/persistence/sqlite_migrations.cpp
    src/persistence/sqlite_order_history_reader.cpp
    src/persistence/sqlite_persistence_sink.cpp
    src/persistence/sqlite_time.cpp
    src/persistence/strategy_signal_mapper.cpp
    src/runtime/worker_group.cpp
    src/scan/instrument_partitioner.cpp
    src/scan/live_candle_aggregator.cpp
    src/scan/live_rsi_divergence_engine.cpp
    src/scan/live_scanner_decision_pipeline.cpp
    src/scan/live_scanner_persistence.cpp
    src/scan/live_scanner_ranking_pipeline.cpp
    src/scan/macd_crossover_signal_scanner.cpp
    src/scan/provisional_rsi_divergence_scanner.cpp
    src/scan/rsi_divergence_signal_mapper.cpp
    src/strategy/advanced_buy_strategies.cpp
    src/strategy/buy_strategies.cpp
    src/strategy/exit_engine.cpp
    src/strategy/indicators.cpp
    src/strategy/kill_switch.cpp
    src/strategy/market_session.cpp
    src/strategy/risk_manager.cpp
    src/strategy/scanner_candidate_selector.cpp
    src/strategy/scanner_signal_ranker.cpp
    src/strategy/sell_strategies.cpp
    src/strategy/signal_aggregation.cpp
    src/strategy/strategy.cpp
)

target_include_directories(tradingbot_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(tradingbot_core PUBLIC cxx_std_20)

function(copy_sqlite_runtime target_name)
    if(WIN32 AND SQLite3_RUNTIME_LIBRARY)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${SQLite3_RUNTIME_LIBRARY}
                    $<TARGET_FILE_DIR:${target_name}>
        )
    endif()
endfunction()

add_executable(tradingbot_upstox
    src/app/main.cpp
)

target_link_libraries(tradingbot_upstox PRIVATE tradingbot_core)
copy_sqlite_runtime(tradingbot_upstox)

if(TRADINGBOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
