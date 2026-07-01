#include "tradingbot/persistence/audit_event_mapper.hpp"

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

tradingbot::persistence::AuditEvent audit_event() {
    return {
        .run_id = "run-1",
        .category = "risk",
        .message = "order rejected",
        .metadata = R"({"reason":"max_position"})",
        .created_at = tradingbot::core::TimePoint{std::chrono::seconds{7}},
    };
}

tradingbot::persistence::StoredAuditEventRow stored_row() {
    return {
        .run_id = "run-1",
        .category = "risk",
        .message = "order rejected",
        .metadata = R"({"reason":"max_position"})",
        .created_at = tradingbot::core::TimePoint{std::chrono::seconds{7}},
    };
}

void maps_audit_event_to_stored_row() {
    const auto row = tradingbot::persistence::map_audit_event_to_stored_row(audit_event());

    require(row.run_id == "run-1", "run id should store");
    require(row.category == "risk", "category should store");
    require(row.message == "order rejected", "message should store");
    require(row.metadata == R"({"reason":"max_position"})", "metadata should store");
    require(row.created_at == tradingbot::core::TimePoint{std::chrono::seconds{7}}, "created time should store");
}

void maps_stored_row_to_audit_event() {
    const auto event = tradingbot::persistence::map_stored_audit_event_row(stored_row());

    require(event.run_id == "run-1", "run id should map");
    require(event.category == "risk", "category should map");
    require(event.message == "order rejected", "message should map");
    require(event.metadata == R"({"reason":"max_position"})", "metadata should map");
    require(event.created_at == tradingbot::core::TimePoint{std::chrono::seconds{7}}, "created time should map");
}

void maps_empty_metadata() {
    auto row = stored_row();
    row.metadata.clear();

    const auto event = tradingbot::persistence::map_stored_audit_event_row(row);

    require(event.metadata.empty(), "empty metadata should map");
}

void round_trips_audit_event_through_stored_row() {
    auto original = audit_event();
    original.category = "api";
    original.message = "retry attempted";

    const auto row = tradingbot::persistence::map_audit_event_to_stored_row(original);
    const auto mapped = tradingbot::persistence::map_stored_audit_event_row(row);

    require(mapped.run_id == original.run_id, "run id should round trip");
    require(mapped.category == original.category, "category should round trip");
    require(mapped.message == original.message, "message should round trip");
    require(mapped.metadata == original.metadata, "metadata should round trip");
    require(mapped.created_at == original.created_at, "created time should round trip");
}

}  // namespace

int main() {
    maps_audit_event_to_stored_row();
    maps_stored_row_to_audit_event();
    maps_empty_metadata();
    round_trips_audit_event_through_stored_row();
    return 0;
}
