#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace tradingbot::app {

int run_app(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

}  // namespace tradingbot::app
