#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace tradingbot::persistence {

std::string format_sqlite_timestamp(core::TimePoint timestamp);
std::optional<core::TimePoint> parse_sqlite_timestamp(std::string_view value);
std::optional<std::string> format_optional_sqlite_timestamp(std::optional<core::TimePoint> timestamp);
std::optional<core::TimePoint> parse_optional_sqlite_timestamp(const std::optional<std::string>& value);

}  // namespace tradingbot::persistence
