#include "tradingbot/persistence/quote_snapshot_mapper.hpp"

namespace tradingbot::persistence {

int stored_bool_value(bool value) {
    return value ? 1 : 0;
}

std::optional<bool> parse_stored_bool(int value) {
    if (value == 0) {
        return false;
    }
    if (value == 1) {
        return true;
    }
    return std::nullopt;
}

StoredQuoteSnapshotRow map_quote_snapshot_to_stored_row(const core::QuoteSnapshot& quote,
                                                        const std::string& run_id) {
    return {
        .run_id = run_id,
        .instrument_key = quote.instrument_key.value,
        .ltp = quote.ltp,
        .stale = stored_bool_value(quote.stale),
        .captured_at = quote.timestamp,
    };
}

QuoteSnapshotMapResult map_stored_quote_snapshot_row(const StoredQuoteSnapshotRow& row) {
    const auto stale = parse_stored_bool(row.stale);
    if (!stale) {
        return {.ok = false, .error = "stored quote snapshot has invalid stale flag"};
    }

    return {
        .ok = true,
        .quote =
            {
                .instrument_key = {row.instrument_key},
                .timestamp = row.captured_at,
                .ltp = row.ltp,
                .stale = *stale,
            },
    };
}

}  // namespace tradingbot::persistence
