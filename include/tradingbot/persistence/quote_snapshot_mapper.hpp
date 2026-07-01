#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>

namespace tradingbot::persistence {

struct StoredQuoteSnapshotRow {
    std::string run_id;
    std::string instrument_key;
    core::Money ltp{0.0};
    int stale{0};
    core::TimePoint captured_at{};
};

struct QuoteSnapshotMapResult {
    bool ok{false};
    core::QuoteSnapshot quote;
    std::string error;
};

QuoteSnapshotMapResult map_stored_quote_snapshot_row(const StoredQuoteSnapshotRow& row);
StoredQuoteSnapshotRow map_quote_snapshot_to_stored_row(const core::QuoteSnapshot& quote,
                                                        const std::string& run_id);
int stored_bool_value(bool value);
std::optional<bool> parse_stored_bool(int value);

}  // namespace tradingbot::persistence
