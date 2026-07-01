#pragma once

#include "tradingbot/persistence/persistence_worker.hpp"

#include <string>

namespace tradingbot::persistence {

struct StoredAuditEventRow {
    std::string run_id;
    std::string category;
    std::string message;
    std::string metadata;
    core::TimePoint created_at{};
};

StoredAuditEventRow map_audit_event_to_stored_row(const AuditEvent& event);
AuditEvent map_stored_audit_event_row(const StoredAuditEventRow& row);

}  // namespace tradingbot::persistence
