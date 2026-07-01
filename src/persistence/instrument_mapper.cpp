#include "tradingbot/persistence/instrument_mapper.hpp"

namespace tradingbot::persistence {

std::string stored_exchange_value(core::Exchange exchange) {
    switch (exchange) {
        case core::Exchange::NseEq:
            return "NSE_EQ";
        case core::Exchange::BseEq:
            return "BSE_EQ";
        case core::Exchange::Unknown:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::optional<core::Exchange> parse_stored_exchange(const std::string& value) {
    if (value == "UNKNOWN") {
        return core::Exchange::Unknown;
    }
    if (value == "NSE_EQ") {
        return core::Exchange::NseEq;
    }
    if (value == "BSE_EQ") {
        return core::Exchange::BseEq;
    }
    return std::nullopt;
}

int stored_instrument_enabled_value(bool value) {
    return value ? 1 : 0;
}

std::optional<bool> parse_stored_instrument_enabled(int value) {
    if (value == 0) {
        return false;
    }
    if (value == 1) {
        return true;
    }
    return std::nullopt;
}

StoredInstrumentRow map_instrument_to_stored_row(const core::Instrument& instrument, core::TimePoint updated_at) {
    return {
        .instrument_key = instrument.key.value,
        .symbol = instrument.symbol,
        .exchange = stored_exchange_value(instrument.exchange),
        .enabled = stored_instrument_enabled_value(instrument.enabled),
        .quantity = instrument.quantity,
        .max_position_quantity = instrument.max_position_quantity,
        .manual_buy_price = instrument.manual_buy_price,
        .manual_target_price = instrument.manual_target_price,
        .stop_loss_pct = instrument.stop_loss_pct,
        .target_profit_pct = instrument.target_profit_pct,
        .trailing_stop_pct = instrument.trailing_stop_pct,
        .strategy_profile = instrument.strategy_profile,
        .notes = instrument.notes,
        .updated_at = updated_at,
    };
}

InstrumentMapResult map_stored_instrument_row(const StoredInstrumentRow& row) {
    const auto exchange = parse_stored_exchange(row.exchange);
    if (!exchange) {
        return {.ok = false, .error = "stored instrument has invalid exchange"};
    }

    const auto enabled = parse_stored_instrument_enabled(row.enabled);
    if (!enabled) {
        return {.ok = false, .error = "stored instrument has invalid enabled flag"};
    }

    return {
        .ok = true,
        .instrument =
            {
                .key = {row.instrument_key},
                .symbol = row.symbol,
                .exchange = *exchange,
                .enabled = *enabled,
                .quantity = row.quantity,
                .max_position_quantity = row.max_position_quantity,
                .manual_buy_price = row.manual_buy_price,
                .manual_target_price = row.manual_target_price,
                .stop_loss_pct = row.stop_loss_pct,
                .target_profit_pct = row.target_profit_pct,
                .trailing_stop_pct = row.trailing_stop_pct,
                .strategy_profile = row.strategy_profile,
                .notes = row.notes,
            },
    };
}

}  // namespace tradingbot::persistence
