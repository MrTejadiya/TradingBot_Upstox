#pragma once

#include <iosfwd>
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
    bool help_requested{false};
};

struct CliResult {
    bool ok{false};
    CliOptions options{};
    std::string error{};
};

std::string to_string(Mode mode);
CliResult parse_cli(std::vector<std::string> args);
int run_cli(const CliOptions& options, std::ostream& out, std::ostream& err);
void print_usage(std::ostream& out);

}  // namespace tradingbot::app

