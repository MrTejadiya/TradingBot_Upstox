#include "tradingbot/app/cli.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeOrderHistoryReader final : public tradingbot::app::OrderHistoryReader {
public:
    tradingbot::app::OrderHistoryLoadResult result;

    tradingbot::app::OrderHistoryLoadResult load_orders() override {
        ++calls;
        return result;
    }

    int calls{0};
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::core::OrderRecord order_record() {
    return {
        .request =
            {
                .instrument_key = {"NSE_EQ|INE002A01018"},
                .side = tradingbot::core::OrderSide::Buy,
                .quantity = 1,
                .price = 100.0,
                .run_id = "run-1",
            },
        .broker_order_id = "ORDER-1",
        .status = tradingbot::core::OrderStatus::Accepted,
        .updated_at = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

void parses_default_dry_run_mode() {
    const auto result = tradingbot::app::parse_cli({});
    require(result.ok, "empty arguments should parse");
    require(result.options.mode == tradingbot::app::Mode::DryRun, "dry-run should be default");
}

void parses_all_declared_modes() {
    const std::vector<std::pair<std::string, tradingbot::app::Mode>> modes{
        {"validate", tradingbot::app::Mode::Validate},
        {"dry-run", tradingbot::app::Mode::DryRun},
        {"paper", tradingbot::app::Mode::Paper},
        {"live", tradingbot::app::Mode::Live},
        {"show-orders", tradingbot::app::Mode::ShowOrders},
    };

    for (const auto& [name, mode] : modes) {
        const auto result = tradingbot::app::parse_cli({"--mode", name});
        require(result.ok, "mode should parse: " + name);
        require(result.options.mode == mode, "mode enum should match: " + name);
    }
}

void rejects_unknown_mode() {
    const auto result = tradingbot::app::parse_cli({"--mode", "intraday"});
    require(!result.ok, "unknown mode should fail");
    require(result.error.find("unsupported mode") != std::string::npos, "unknown mode error should be clear");
}

void parses_live_gate_flags() {
    const auto result = tradingbot::app::parse_cli(
        {"--mode=live", "--live-trading-enabled", "--confirm-live-trading"});

    require(result.ok, "live gate flags should parse");
    require(result.options.mode == tradingbot::app::Mode::Live, "live mode should parse with equals syntax");
    require(result.options.live_trading_enabled, "live trading enabled flag should parse");
    require(result.options.live_trading_confirmed, "live trading confirmation flag should parse");
}

void parses_config_path() {
    const auto result = tradingbot::app::parse_cli({"--config", "config.json"});

    require(result.ok, "config path should parse");
    require(result.options.config_path == "config.json", "config path should store");
}

void parses_config_path_with_equals() {
    const auto result = tradingbot::app::parse_cli({"--config=config.json"});

    require(result.ok, "config path with equals should parse");
    require(result.options.config_path == "config.json", "config path should store with equals syntax");
}

void rejects_missing_config_path() {
    const auto result = tradingbot::app::parse_cli({"--config"});

    require(!result.ok, "missing config path should fail");
    require(result.error.find("--config requires a value") != std::string::npos, "missing config error should be clear");
}

void blocks_live_mode_in_scaffold() {
    std::ostringstream out;
    std::ostringstream err;
    tradingbot::app::CliOptions options;
    options.mode = tradingbot::app::Mode::Live;
    const auto code = tradingbot::app::run_cli(options, out, err);
    require(code != 0, "live mode should not run without gates");
    require(err.str().find("live_trading_enabled=true") != std::string::npos, "live mode should explain missing gate");
}

void allows_live_mode_after_explicit_gates() {
    std::ostringstream out;
    std::ostringstream err;
    tradingbot::app::CliOptions options;
    options.mode = tradingbot::app::Mode::Live;
    options.live_trading_enabled = true;
    options.live_trading_confirmed = true;

    const auto code = tradingbot::app::run_cli(options, out, err);
    require(code == 0, "live mode should run after explicit gates");
    require(out.str().find("gates are satisfied") != std::string::npos, "live mode should confirm gates");
}

void allows_paper_mode_without_live_gates() {
    std::ostringstream out;
    std::ostringstream err;
    tradingbot::app::CliOptions options;
    options.mode = tradingbot::app::Mode::Paper;

    const auto code = tradingbot::app::run_cli(options, out, err);

    require(code == 0, "paper mode should run without live gates");
    require(err.str().empty(), "paper mode should not emit errors");
    require(out.str().find("local simulator and backtest components are available") != std::string::npos,
            "paper mode should describe simulator readiness");
    require(out.str().find("broker order placement is disabled") != std::string::npos,
            "paper mode should confirm broker orders are disabled");
}

void show_orders_prints_empty_state() {
    std::ostringstream out;
    std::ostringstream err;
    tradingbot::app::CliOptions options;
    options.mode = tradingbot::app::Mode::ShowOrders;

    const auto code = tradingbot::app::run_cli(options, out, err);

    require(code == 0, "show-orders should run");
    require(out.str().find("No orders found") != std::string::npos, "show-orders should print empty state");
}

void show_orders_renders_loaded_history() {
    std::ostringstream out;
    std::ostringstream err;
    tradingbot::app::CliOptions options;
    options.mode = tradingbot::app::Mode::ShowOrders;
    FakeOrderHistoryReader reader;
    reader.result = {.ok = true, .orders = {order_record()}};

    const auto code = tradingbot::app::run_cli(options, out, err, &reader);

    require(code == 0, "show-orders should run with history reader");
    require(reader.calls == 1, "show-orders should load order history");
    require(err.str().empty(), "successful show-orders should not write errors");
    require(out.str().find("ORDER-1") != std::string::npos, "show-orders should render loaded order");
    require(out.str().find("No orders found") == std::string::npos, "loaded history should not print empty state");
}

void show_orders_reports_history_reader_failure() {
    std::ostringstream out;
    std::ostringstream err;
    tradingbot::app::CliOptions options;
    options.mode = tradingbot::app::Mode::ShowOrders;
    FakeOrderHistoryReader reader;
    reader.result = {.ok = false, .error = "database unavailable"};

    const auto code = tradingbot::app::run_cli(options, out, err, &reader);

    require(code != 0, "show-orders should fail when history reader fails");
    require(reader.calls == 1, "show-orders should call failing reader once");
    require(out.str().empty(), "failed show-orders should not print table");
    require(err.str().find("database unavailable") != std::string::npos, "reader error should be reported");
}

}  // namespace

int main() {
    parses_default_dry_run_mode();
    parses_all_declared_modes();
    rejects_unknown_mode();
    parses_live_gate_flags();
    parses_config_path();
    parses_config_path_with_equals();
    rejects_missing_config_path();
    blocks_live_mode_in_scaffold();
    allows_live_mode_after_explicit_gates();
    allows_paper_mode_without_live_gates();
    show_orders_prints_empty_state();
    show_orders_renders_loaded_history();
    show_orders_reports_history_reader_failure();
    return 0;
}
