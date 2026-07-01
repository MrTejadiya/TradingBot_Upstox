#include "tradingbot/persistence/audit_event_mapper.hpp"

namespace tradingbot::persistence {

StoredAuditEventRow map_audit_event_to_stored_row(const AuditEvent& event) {
    return {
        .run_id = event.run_id,
        .category = event.category,
        .message = event.message,
        .metadata = event.metadata,
        .created_at = event.created_at,
    };
}

AuditEvent map_stored_audit_event_row(const StoredAuditEventRow& row) {
    return {
        .run_id = row.run_id,
        .category = row.category,
        .message = row.message,
        .metadata = row.metadata,
        .created_at = row.created_at,
    };
}

}  // namespace tradingbot::persistence
