#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string_view>

namespace tradingbot::infra {

std::optional<core::TimePoint> parse_iso_offset_timestamp(std::string_view value);

}  // namespace tradingbot::infra
