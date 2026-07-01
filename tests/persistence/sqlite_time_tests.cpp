#include "tradingbot/persistence/sqlite_time.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <optional>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

std::time_t utc_epoch(int year, int month, int day, int hour, int minute, int second) {
    std::tm utc{};
    utc.tm_year = year - 1900;
    utc.tm_mon = month - 1;
    utc.tm_mday = day;
    utc.tm_hour = hour;
    utc.tm_min = minute;
    utc.tm_sec = second;
#ifdef _WIN32
    return _mkgmtime(&utc);
#else
    return timegm(&utc);
#endif
}

tradingbot::core::TimePoint tp(int year, int month, int day, int hour, int minute, int second) {
    return tradingbot::core::Clock::from_time_t(utc_epoch(year, month, day, hour, minute, second));
}

void formats_epoch_as_utc_iso_timestamp() {
    const auto formatted = tradingbot::persistence::format_sqlite_timestamp(tradingbot::core::TimePoint{});

    require(formatted == "1970-01-01T00:00:00Z", "epoch should format as UTC ISO timestamp");
}

void formats_non_zero_timestamp() {
    const auto formatted = tradingbot::persistence::format_sqlite_timestamp(tp(2026, 7, 1, 9, 30, 5));

    require(formatted == "2026-07-01T09:30:05Z", "timestamp should format with UTC Z suffix");
}

void parses_utc_iso_timestamp() {
    const auto parsed = tradingbot::persistence::parse_sqlite_timestamp("2026-07-01T09:30:05Z");

    require(parsed.has_value(), "UTC timestamp should parse");
    require(tradingbot::core::Clock::to_time_t(*parsed) == utc_epoch(2026, 7, 1, 9, 30, 5),
            "parsed timestamp should match UTC epoch");
}

void round_trips_timestamp_at_second_precision() {
    const auto original = tp(2026, 7, 1, 9, 30, 5);
    const auto parsed = tradingbot::persistence::parse_sqlite_timestamp(
        tradingbot::persistence::format_sqlite_timestamp(original));

    require(parsed.has_value(), "formatted timestamp should parse");
    require(*parsed == original, "timestamp should round trip");
}

void formats_nullable_timestamps() {
    const std::optional<tradingbot::core::TimePoint> empty;
    const auto formatted_empty = tradingbot::persistence::format_optional_sqlite_timestamp(empty);
    const auto formatted_value = tradingbot::persistence::format_optional_sqlite_timestamp(tp(2026, 7, 1, 9, 30, 5));

    require(!formatted_empty, "empty optional timestamp should stay empty");
    require(formatted_value && *formatted_value == "2026-07-01T09:30:05Z", "optional timestamp should format");
}

void parses_nullable_timestamps() {
    const std::optional<std::string> empty;
    const auto parsed_empty = tradingbot::persistence::parse_optional_sqlite_timestamp(empty);
    const auto parsed_value = tradingbot::persistence::parse_optional_sqlite_timestamp("2026-07-01T09:30:05Z");

    require(!parsed_empty, "empty optional string should stay empty");
    require(parsed_value && tradingbot::core::Clock::to_time_t(*parsed_value) == utc_epoch(2026, 7, 1, 9, 30, 5),
            "optional timestamp should parse");
}

void rejects_malformed_timestamps() {
    require(!tradingbot::persistence::parse_sqlite_timestamp("").has_value(), "empty timestamp should fail");
    require(!tradingbot::persistence::parse_sqlite_timestamp("not-a-time").has_value(), "text timestamp should fail");
    require(!tradingbot::persistence::parse_sqlite_timestamp("2026-07-01T09:30:05+05:30").has_value(),
            "offset timestamp should fail");
    require(!tradingbot::persistence::parse_sqlite_timestamp("2026-13-01T09:30:05Z").has_value(),
            "invalid month should fail");
    require(!tradingbot::persistence::parse_sqlite_timestamp("2026-07-01T25:30:05Z").has_value(),
            "invalid hour should fail");
}

}  // namespace

int main() {
    formats_epoch_as_utc_iso_timestamp();
    formats_non_zero_timestamp();
    parses_utc_iso_timestamp();
    round_trips_timestamp_at_second_precision();
    formats_nullable_timestamps();
    parses_nullable_timestamps();
    rejects_malformed_timestamps();
    return 0;
}
