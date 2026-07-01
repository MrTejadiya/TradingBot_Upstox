#include "tradingbot/persistence/api_event_mapper.hpp"

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

tradingbot::infra::ApiEvent api_event() {
    return {
        .method = "GET",
        .url = "https://api.upstox.com/v2/user/get-funds-and-margin",
        .status_code = 200,
        .attempt_count = 1,
        .retried = false,
        .redacted_request_metadata = R"({"authorization":"redacted"})",
    };
}

tradingbot::persistence::StoredApiEventRow stored_row() {
    return {
        .run_id = "run-1",
        .method = "GET",
        .url = "https://api.upstox.com/v2/user/get-funds-and-margin",
        .status_code = 200,
        .attempt_count = 1,
        .retried = 0,
        .redacted_request_metadata = R"({"authorization":"redacted"})",
        .created_at = tradingbot::core::TimePoint{std::chrono::seconds{7}},
    };
}

void maps_api_event_to_stored_row() {
    const auto row = tradingbot::persistence::map_api_event_to_stored_row(
        api_event(), "run-1", tradingbot::core::TimePoint{std::chrono::seconds{7}});

    require(row.run_id == "run-1", "run id should store");
    require(row.method == "GET", "method should store");
    require(row.url == "https://api.upstox.com/v2/user/get-funds-and-margin", "url should store");
    require(row.status_code == 200, "status code should store");
    require(row.attempt_count == 1, "attempt count should store");
    require(row.retried == 0, "non-retried event should store as zero");
    require(row.redacted_request_metadata == R"({"authorization":"redacted"})", "redacted metadata should store");
    require(row.created_at == tradingbot::core::TimePoint{std::chrono::seconds{7}}, "created time should store");
}

void maps_stored_row_to_api_event() {
    const auto result = tradingbot::persistence::map_stored_api_event_row(stored_row());

    require(result.ok, "valid stored API event should map");
    require(result.event.method == "GET", "method should map");
    require(result.event.url == "https://api.upstox.com/v2/user/get-funds-and-margin", "url should map");
    require(result.event.status_code == 200, "status code should map");
    require(result.event.attempt_count == 1, "attempt count should map");
    require(!result.event.retried, "retried flag should map");
    require(result.event.redacted_request_metadata == R"({"authorization":"redacted"})", "redacted metadata should map");
}

void maps_retried_api_event() {
    auto row = stored_row();
    row.status_code = 503;
    row.attempt_count = 3;
    row.retried = 1;

    const auto result = tradingbot::persistence::map_stored_api_event_row(row);

    require(result.ok, "retried API event should map");
    require(result.event.status_code == 503, "retried status code should map");
    require(result.event.attempt_count == 3, "retried attempt count should map");
    require(result.event.retried, "retried flag should map");
}

void round_trips_api_event_through_stored_row() {
    auto original = api_event();
    original.method = "POST";
    original.status_code = 429;
    original.attempt_count = 2;
    original.retried = true;
    const auto created_at = tradingbot::core::TimePoint{std::chrono::seconds{11}};

    const auto row = tradingbot::persistence::map_api_event_to_stored_row(original, "run-2", created_at);
    const auto result = tradingbot::persistence::map_stored_api_event_row(row);

    require(result.ok, "API event should round trip");
    require(row.run_id == "run-2", "run id should remain on stored row");
    require(row.created_at == created_at, "created time should remain on stored row");
    require(result.event.method == original.method, "method should round trip");
    require(result.event.url == original.url, "url should round trip");
    require(result.event.status_code == original.status_code, "status code should round trip");
    require(result.event.attempt_count == original.attempt_count, "attempt count should round trip");
    require(result.event.retried == original.retried, "retried flag should round trip");
    require(result.event.redacted_request_metadata == original.redacted_request_metadata,
            "redacted metadata should round trip");
}

void parses_retried_values() {
    require(tradingbot::persistence::stored_api_retried_value(false) == 0, "false should store as zero");
    require(tradingbot::persistence::stored_api_retried_value(true) == 1, "true should store as one");
    require(tradingbot::persistence::parse_stored_api_retried(0) == false, "zero should parse false");
    require(tradingbot::persistence::parse_stored_api_retried(1) == true, "one should parse true");
}

void invalid_retried_flag_fails_closed() {
    auto row = stored_row();
    row.retried = 2;

    const auto result = tradingbot::persistence::map_stored_api_event_row(row);

    require(!result.ok, "invalid retried flag should fail");
    require(result.error.find("invalid retried flag") != std::string::npos, "invalid retried error should be clear");
}

}  // namespace

int main() {
    maps_api_event_to_stored_row();
    maps_stored_row_to_api_event();
    maps_retried_api_event();
    round_trips_api_event_through_stored_row();
    parses_retried_values();
    invalid_retried_flag_fails_closed();
    return 0;
}
