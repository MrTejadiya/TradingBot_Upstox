add_executable(tradingbot_cli_tests
    app/cli_tests.cpp
)

target_link_libraries(tradingbot_cli_tests PRIVATE tradingbot_core)

add_test(NAME tradingbot_cli_tests COMMAND tradingbot_cli_tests)

