#pragma once

#include "tradingbot/core/domain.hpp"

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace tradingbot::app {

enum class Mode {
    Validate,
    DryRun,
    Paper,
    Live,
    ShowOrders,
};

struct CliOptions {
    Mode mode{Mode::DryRun};
    bool mode_overridden{false};
    bool live_trading_enabled{false};
    bool live_trading_enabled_overridden{false};
    bool live_trading_confirmed{false};
    bool help_requested{false};
    std::string config_path;
    std::optional<std::size_t> order_limit;
};

struct CliResult {
    bool ok{false};
    CliOptions options{};
    std::string error{};
};

struct OrderHistoryLoadResult {
    bool ok{false};
    std::vector<core::OrderRecord> orders;
    std::string error;
};

class OrderHistoryReader {
public:
    virtual ~OrderHistoryReader() = default;
    virtual OrderHistoryLoadResult load_orders() = 0;
};

std::string to_string(Mode mode);
CliResult parse_cli(std::vector<std::string> args);
int run_cli(const CliOptions& options, std::ostream& out, std::ostream& err,
            OrderHistoryReader* order_history_reader = nullptr);
void print_usage(std::ostream& out);

}  // namespace tradingbot::app
