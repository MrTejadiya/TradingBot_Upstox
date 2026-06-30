#include "tradingbot/app/cli.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
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

void blocks_live_mode_in_scaffold() {
    std::ostringstream out;
    std::ostringstream err;
    const auto code = tradingbot::app::run_cli({tradingbot::app::Mode::Live, false}, out, err);
    require(code != 0, "live mode should not run in scaffold");
    require(err.str().find("blocked") != std::string::npos, "live mode should explain that it is blocked");
}

}  // namespace

int main() {
    parses_default_dry_run_mode();
    parses_all_declared_modes();
    rejects_unknown_mode();
    blocks_live_mode_in_scaffold();
    return 0;
}
