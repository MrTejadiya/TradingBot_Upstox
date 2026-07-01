#include "tradingbot/infra/time_utils.hpp"

#include <cstdlib>
#include <ctime>
#include <iostream>
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

void parses_positive_offset() {
    const auto timestamp = tradingbot::infra::parse_iso_offset_timestamp("2026-06-30T15:29:30+05:30");

    require(timestamp.has_value(), "positive offset timestamp should parse");
    require(tradingbot::core::Clock::to_time_t(*timestamp) == utc_epoch(2026, 6, 30, 9, 59, 30),
            "positive offset should convert to UTC");
}

void parses_negative_offset() {
    const auto timestamp = tradingbot::infra::parse_iso_offset_timestamp("2026-06-30T09:15:00-04:00");

    require(timestamp.has_value(), "negative offset timestamp should parse");
    require(tradingbot::core::Clock::to_time_t(*timestamp) == utc_epoch(2026, 6, 30, 13, 15, 0),
            "negative offset should convert to UTC");
}

void rejects_malformed_inputs() {
    require(!tradingbot::infra::parse_iso_offset_timestamp("").has_value(), "empty timestamp should fail");
    require(!tradingbot::infra::parse_iso_offset_timestamp("not-a-time").has_value(), "text timestamp should fail");
    require(!tradingbot::infra::parse_iso_offset_timestamp("2026-13-30T09:15:00+05:30").has_value(),
            "invalid month should fail");
    require(!tradingbot::infra::parse_iso_offset_timestamp("2026-06-30T25:15:00+05:30").has_value(),
            "invalid hour should fail");
}

}  // namespace

int main() {
    parses_positive_offset();
    parses_negative_offset();
    rejects_malformed_inputs();
    return 0;
}
