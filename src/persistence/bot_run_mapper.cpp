#include "tradingbot/persistence/bot_run_mapper.hpp"

namespace tradingbot::persistence {

StoredBotRunRow map_bot_run_to_stored_row(const core::BotRun& run) {
    return {
        .run_id = run.run_id,
        .started_at = run.started_at,
        .ended_at = run.ended_at,
        .mode = run.mode,
        .config_hash = run.config_hash,
    };
}

core::BotRun map_stored_bot_run_row(const StoredBotRunRow& row) {
    return {
        .run_id = row.run_id,
        .started_at = row.started_at,
        .ended_at = row.ended_at,
        .mode = row.mode,
        .config_hash = row.config_hash,
    };
}

}  // namespace tradingbot::persistence
