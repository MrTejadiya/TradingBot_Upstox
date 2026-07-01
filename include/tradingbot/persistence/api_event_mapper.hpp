#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/infra/upstox_api_client.hpp"

#include <optional>
#include <string>

namespace tradingbot::persistence {

struct StoredApiEventRow {
    std::string run_id;
    std::string method;
    std::string url;
    int status_code{0};
    int attempt_count{0};
    int retried{0};
    std::string redacted_request_metadata;
    core::TimePoint created_at{};
};

struct ApiEventMapResult {
    bool ok{false};
    infra::ApiEvent event;
    std::string error;
};

ApiEventMapResult map_stored_api_event_row(const StoredApiEventRow& row);
StoredApiEventRow map_api_event_to_stored_row(const infra::ApiEvent& event, const std::string& run_id,
                                              core::TimePoint created_at);
int stored_api_retried_value(bool value);
std::optional<bool> parse_stored_api_retried(int value);

}  // namespace tradingbot::persistence
