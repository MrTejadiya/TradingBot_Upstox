#pragma once

#include "tradingbot/core/domain.hpp"

#include <string>
#include <vector>

namespace tradingbot::infra {

struct UpstoxInstrumentJsonLoadOptions {
    bool enabled{true};
    core::Quantity quantity{1};
    core::Quantity max_position_quantity{1};
    core::Percent target_profit_pct{10.0};
    std::string strategy_profile;
    std::string notes{"imported from Upstox instrument JSON"};
};

struct UpstoxInstrumentJsonLoadResult {
    bool ok{false};
    std::vector<core::Instrument> instruments;
    std::vector<std::string> errors;
    std::size_t skipped_records{0};
};

UpstoxInstrumentJsonLoadResult load_upstox_instruments_json_file(const std::string& path);
UpstoxInstrumentJsonLoadResult load_upstox_instruments_json_file(
    const std::string& path, const UpstoxInstrumentJsonLoadOptions& options);
UpstoxInstrumentJsonLoadResult load_upstox_instruments_json_text(const std::string& json_text);
UpstoxInstrumentJsonLoadResult load_upstox_instruments_json_text(
    const std::string& json_text, const UpstoxInstrumentJsonLoadOptions& options);

}  // namespace tradingbot::infra
