#include "tradingbot/app/cli.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }

    const auto parsed = tradingbot::app::parse_cli(args);
    if (!parsed.ok) {
        std::cerr << parsed.error << "\n";
        tradingbot::app::print_usage(std::cerr);
        return 2;
    }

    return tradingbot::app::run_cli(parsed.options, std::cout, std::cerr);
}

