#include "tradingbot/app/cli.hpp"

#include "tradingbot/app/operating_mode.hpp"
#include "tradingbot/app/order_display.hpp"

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace tradingbot::app {
namespace {

bool parse_mode_value(const std::string& value, Mode& mode) {
    if (value == "validate") {
        mode = Mode::Validate;
        return true;
    }
    if (value == "dry-run") {
        mode = Mode::DryRun;
        return true;
    }
    if (value == "paper") {
        mode = Mode::Paper;
        return true;
    }
    if (value == "live") {
        mode = Mode::Live;
        return true;
    }
    if (value == "show-orders") {
        mode = Mode::ShowOrders;
        return true;
    }
    return false;
}

}  // namespace

std::string to_string(Mode mode) {
    switch (mode) {
        case Mode::Validate:
            return "validate";
        case Mode::DryRun:
            return "dry-run";
        case Mode::Paper:
            return "paper";
        case Mode::Live:
            return "live";
        case Mode::ShowOrders:
            return "show-orders";
    }
    return "unknown";
}

CliResult parse_cli(std::vector<std::string> args) {
    CliResult result;
    result.ok = true;

    for (std::size_t index = 0; index < args.size(); ++index) {
        const auto& arg = args[index];
        if (arg == "--help" || arg == "-h") {
            result.options.help_requested = true;
            return result;
        }

        if (arg == "--live-trading-enabled") {
            result.options.live_trading_enabled = true;
            continue;
        }

        if (arg == "--confirm-live-trading") {
            result.options.live_trading_confirmed = true;
            continue;
        }

        if (arg == "--mode") {
            if (index + 1 >= args.size()) {
                result.ok = false;
                result.error = "--mode requires a value";
                return result;
            }
            Mode parsed_mode{};
            const auto& value = args[++index];
            if (!parse_mode_value(value, parsed_mode)) {
                result.ok = false;
                result.error = "unsupported mode: " + value;
                return result;
            }
            result.options.mode = parsed_mode;
            continue;
        }

        constexpr std::string_view mode_prefix{"--mode="};
        if (arg.rfind(mode_prefix, 0) == 0) {
            const auto value = arg.substr(mode_prefix.size());
            Mode parsed_mode{};
            if (!parse_mode_value(value, parsed_mode)) {
                result.ok = false;
                result.error = "unsupported mode: " + value;
                return result;
            }
            result.options.mode = parsed_mode;
            continue;
        }

        result.ok = false;
        result.error = "unknown argument: " + arg;
        return result;
    }

    return result;
}

int run_cli(const CliOptions& options, std::ostream& out, std::ostream& err) {
    if (options.help_requested) {
        print_usage(out);
        return 0;
    }

    const auto validation = validate_operating_mode({
        .mode = options.mode,
        .live_trading_enabled = options.live_trading_enabled,
        .live_trading_confirmed = options.live_trading_confirmed,
    });
    if (!validation.ok) {
        err << validation.error << "\n";
        return 2;
    }

    switch (options.mode) {
        case Mode::Validate:
            out << "validate mode selected: configuration and CSV validation scaffold is ready.\n";
            return 0;
        case Mode::DryRun:
            out << "dry-run mode selected: live order placement is disabled.\n";
            return 0;
        case Mode::Paper:
            out << "paper mode selected: local simulator and backtest components are available; broker order "
                   "placement is disabled.\n";
            return 0;
        case Mode::Live:
            out << "live mode selected: live order placement gates are satisfied.\n";
            return 0;
        case Mode::ShowOrders:
            print_orders({}, out);
            return 0;
    }

    err << "unhandled mode.\n";
    return 2;
}

void print_usage(std::ostream& out) {
    out << "Usage: tradingbot_upstox [--mode <validate|dry-run|paper|live|show-orders>]\n"
        << "       [--live-trading-enabled] [--confirm-live-trading]\n"
        << "\n"
        << "Default mode: dry-run\n"
        << "Live mode requires both --live-trading-enabled and --confirm-live-trading.\n";
}

}  // namespace tradingbot::app
