#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>

namespace tradingbot::persistence {

struct StoredInstrumentRow {
    std::string instrument_key;
    std::string symbol;
    std::string exchange;
    int enabled{0};
    core::Quantity quantity{0};
    core::Quantity max_position_quantity{0};
    std::optional<core::Money> manual_buy_price;
    std::optional<core::Money> manual_target_price;
    std::optional<core::Percent> stop_loss_pct;
    core::Percent target_profit_pct{0.0};
    std::optional<core::Percent> trailing_stop_pct;
    std::string strategy_profile;
    std::string notes;
    core::TimePoint updated_at{};
};

struct InstrumentMapResult {
    bool ok{false};
    core::Instrument instrument;
    std::string error;
};

InstrumentMapResult map_stored_instrument_row(const StoredInstrumentRow& row);
StoredInstrumentRow map_instrument_to_stored_row(const core::Instrument& instrument, core::TimePoint updated_at);
std::string stored_exchange_value(core::Exchange exchange);
std::optional<core::Exchange> parse_stored_exchange(const std::string& value);
int stored_instrument_enabled_value(bool value);
std::optional<bool> parse_stored_instrument_enabled(int value);

}  // namespace tradingbot::persistence
