#include "tradingbot/persistence/quote_snapshot_mapper.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::core::QuoteSnapshot quote() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .timestamp = tradingbot::core::TimePoint{std::chrono::seconds{1}},
        .ltp = 100.5,
        .stale = false,
    };
}

tradingbot::persistence::StoredQuoteSnapshotRow stored_row() {
    return {
        .run_id = "run-1",
        .instrument_key = "NSE_EQ|INE002A01018",
        .ltp = 100.5,
        .stale = 0,
        .captured_at = tradingbot::core::TimePoint{std::chrono::seconds{1}},
    };
}

void maps_quote_to_stored_row() {
    const auto row = tradingbot::persistence::map_quote_snapshot_to_stored_row(quote(), "run-1");

    require(row.run_id == "run-1", "run id should store");
    require(row.instrument_key == "NSE_EQ|INE002A01018", "instrument should store");
    require(row.ltp == 100.5, "ltp should store");
    require(row.stale == 0, "fresh stale flag should store as zero");
    require(row.captured_at == tradingbot::core::TimePoint{std::chrono::seconds{1}}, "timestamp should store");
}

void maps_stored_row_to_quote() {
    const auto result = tradingbot::persistence::map_stored_quote_snapshot_row(stored_row());

    require(result.ok, "valid stored quote should map");
    require(result.quote.instrument_key.value == "NSE_EQ|INE002A01018", "instrument should map");
    require(result.quote.timestamp == tradingbot::core::TimePoint{std::chrono::seconds{1}}, "timestamp should map");
    require(result.quote.ltp == 100.5, "ltp should map");
    require(!result.quote.stale, "fresh stale flag should map");
}

void maps_stale_quote() {
    auto row = stored_row();
    row.stale = 1;

    const auto result = tradingbot::persistence::map_stored_quote_snapshot_row(row);

    require(result.ok, "stale quote should map");
    require(result.quote.stale, "stale flag should map");
}

void round_trips_quote_through_stored_row() {
    auto original = quote();
    original.stale = true;
    const auto row = tradingbot::persistence::map_quote_snapshot_to_stored_row(original, "run-1");
    const auto result = tradingbot::persistence::map_stored_quote_snapshot_row(row);

    require(result.ok, "quote should round trip");
    require(result.quote.instrument_key.value == original.instrument_key.value, "instrument should round trip");
    require(result.quote.timestamp == original.timestamp, "timestamp should round trip");
    require(result.quote.ltp == original.ltp, "ltp should round trip");
    require(result.quote.stale == original.stale, "stale flag should round trip");
}

void parses_stored_bool_values() {
    require(tradingbot::persistence::stored_bool_value(false) == 0, "false should store as zero");
    require(tradingbot::persistence::stored_bool_value(true) == 1, "true should store as one");
    require(tradingbot::persistence::parse_stored_bool(0) == false, "zero should parse false");
    require(tradingbot::persistence::parse_stored_bool(1) == true, "one should parse true");
}

void invalid_stale_flag_fails_closed() {
    auto row = stored_row();
    row.stale = 2;

    const auto result = tradingbot::persistence::map_stored_quote_snapshot_row(row);

    require(!result.ok, "invalid stale flag should fail");
    require(result.error.find("invalid stale flag") != std::string::npos, "invalid stale error should be clear");
}

}  // namespace

int main() {
    maps_quote_to_stored_row();
    maps_stored_row_to_quote();
    maps_stale_quote();
    round_trips_quote_through_stored_row();
    parses_stored_bool_values();
    invalid_stale_flag_fails_closed();
    return 0;
}
