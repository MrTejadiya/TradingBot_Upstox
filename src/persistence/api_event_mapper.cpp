#include "tradingbot/persistence/api_event_mapper.hpp"

namespace tradingbot::persistence {

int stored_api_retried_value(bool value) {
    return value ? 1 : 0;
}

std::optional<bool> parse_stored_api_retried(int value) {
    if (value == 0) {
        return false;
    }
    if (value == 1) {
        return true;
    }
    return std::nullopt;
}

StoredApiEventRow map_api_event_to_stored_row(const infra::ApiEvent& event, const std::string& run_id,
                                              core::TimePoint created_at) {
    return {
        .run_id = run_id,
        .method = event.method,
        .url = event.url,
        .status_code = event.status_code,
        .attempt_count = event.attempt_count,
        .retried = stored_api_retried_value(event.retried),
        .redacted_request_metadata = event.redacted_request_metadata,
        .created_at = created_at,
    };
}

ApiEventMapResult map_stored_api_event_row(const StoredApiEventRow& row) {
    const auto retried = parse_stored_api_retried(row.retried);
    if (!retried) {
        return {.ok = false, .error = "stored API event has invalid retried flag"};
    }

    return {
        .ok = true,
        .event =
            {
                .method = row.method,
                .url = row.url,
                .status_code = row.status_code,
                .attempt_count = row.attempt_count,
                .retried = *retried,
                .redacted_request_metadata = row.redacted_request_metadata,
            },
    };
}

}  // namespace tradingbot::persistence
