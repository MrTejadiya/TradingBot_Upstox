add_executable(tradingbot_cli_tests
    app/cli_tests.cpp
)

target_link_libraries(tradingbot_cli_tests PRIVATE tradingbot_core)

add_test(NAME tradingbot_cli_tests COMMAND tradingbot_cli_tests)

add_executable(tradingbot_operating_mode_tests
    app/operating_mode_tests.cpp
)

target_link_libraries(tradingbot_operating_mode_tests PRIVATE tradingbot_core)

add_test(NAME tradingbot_operating_mode_tests COMMAND tradingbot_operating_mode_tests)

add_executable(tradingbot_domain_tests
    core/domain_tests.cpp
)

target_link_libraries(tradingbot_domain_tests PRIVATE tradingbot_core)

add_test(NAME tradingbot_domain_tests COMMAND tradingbot_domain_tests)

add_executable(tradingbot_config_tests
    infra/config_tests.cpp
)

target_link_libraries(tradingbot_config_tests PRIVATE tradingbot_core)

add_test(NAME tradingbot_config_tests COMMAND tradingbot_config_tests)

add_executable(tradingbot_credentials_tests
    infra/credentials_tests.cpp
)

target_link_libraries(tradingbot_credentials_tests PRIVATE tradingbot_core)

add_test(NAME tradingbot_credentials_tests COMMAND tradingbot_credentials_tests)

add_executable(tradingbot_instrument_csv_tests
    infra/instrument_csv_tests.cpp
)

target_link_libraries(tradingbot_instrument_csv_tests PRIVATE tradingbot_core)

add_test(NAME tradingbot_instrument_csv_tests COMMAND tradingbot_instrument_csv_tests)

add_executable(tradingbot_logger_tests
    infra/logger_tests.cpp
)

target_link_libraries(tradingbot_logger_tests PRIVATE tradingbot_core)

add_test(NAME tradingbot_logger_tests COMMAND tradingbot_logger_tests)
