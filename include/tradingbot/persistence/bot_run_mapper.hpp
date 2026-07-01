#pragma once

#include "tradingbot/core/domain.hpp"

#include <optional>
#include <string>

namespace tradingbot::persistence {

struct StoredBotRunRow {
    std::string run_id;
    core::TimePoint started_at{};
    std::optional<core::TimePoint> ended_at;
    std::string mode;
    std::string config_hash;
};

StoredBotRunRow map_bot_run_to_stored_row(const core::BotRun& run);
core::BotRun map_stored_bot_run_row(const StoredBotRunRow& row);

}  // namespace tradingbot::persistence
