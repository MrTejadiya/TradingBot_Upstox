#include "tradingbot/persistence/bot_run_mapper.hpp"

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

tradingbot::core::BotRun bot_run() {
    return {
        .run_id = "run-1",
        .started_at = tradingbot::core::TimePoint{std::chrono::seconds{7}},
        .ended_at = tradingbot::core::TimePoint{std::chrono::seconds{17}},
        .mode = "paper",
        .config_hash = "sha256:abc123",
    };
}

tradingbot::persistence::StoredBotRunRow stored_row() {
    return {
        .run_id = "run-1",
        .started_at = tradingbot::core::TimePoint{std::chrono::seconds{7}},
        .ended_at = tradingbot::core::TimePoint{std::chrono::seconds{17}},
        .mode = "paper",
        .config_hash = "sha256:abc123",
    };
}

void maps_bot_run_to_stored_row() {
    const auto row = tradingbot::persistence::map_bot_run_to_stored_row(bot_run());

    require(row.run_id == "run-1", "run id should store");
    require(row.started_at == tradingbot::core::TimePoint{std::chrono::seconds{7}}, "started time should store");
    require(row.ended_at && *row.ended_at == tradingbot::core::TimePoint{std::chrono::seconds{17}},
            "ended time should store");
    require(row.mode == "paper", "mode should store");
    require(row.config_hash == "sha256:abc123", "config hash should store");
}

void maps_stored_row_to_bot_run() {
    const auto run = tradingbot::persistence::map_stored_bot_run_row(stored_row());

    require(run.run_id == "run-1", "run id should map");
    require(run.started_at == tradingbot::core::TimePoint{std::chrono::seconds{7}}, "started time should map");
    require(run.ended_at && *run.ended_at == tradingbot::core::TimePoint{std::chrono::seconds{17}},
            "ended time should map");
    require(run.mode == "paper", "mode should map");
    require(run.config_hash == "sha256:abc123", "config hash should map");
}

void maps_running_bot_run_without_end_time() {
    auto row = stored_row();
    row.ended_at.reset();

    const auto run = tradingbot::persistence::map_stored_bot_run_row(row);

    require(!run.ended_at, "empty ended time should map");
}

void round_trips_bot_run_through_stored_row() {
    auto original = bot_run();
    original.mode = "live";
    original.ended_at.reset();

    const auto row = tradingbot::persistence::map_bot_run_to_stored_row(original);
    const auto mapped = tradingbot::persistence::map_stored_bot_run_row(row);

    require(mapped.run_id == original.run_id, "run id should round trip");
    require(mapped.started_at == original.started_at, "started time should round trip");
    require(mapped.ended_at == original.ended_at, "ended time should round trip");
    require(mapped.mode == original.mode, "mode should round trip");
    require(mapped.config_hash == original.config_hash, "config hash should round trip");
}

}  // namespace

int main() {
    maps_bot_run_to_stored_row();
    maps_stored_row_to_bot_run();
    maps_running_bot_run_without_end_time();
    round_trips_bot_run_through_stored_row();
    return 0;
}
