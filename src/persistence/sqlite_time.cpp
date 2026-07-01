#include "tradingbot/persistence/sqlite_time.hpp"

#include <array>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>

namespace tradingbot::persistence {
namespace {

std::optional<int> parse_two_digits(std::string_view value) {
    if (value.size() != 2 || std::isdigit(static_cast<unsigned char>(value[0])) == 0 ||
        std::isdigit(static_cast<unsigned char>(value[1])) == 0) {
        return std::nullopt;
    }
    return (value[0] - '0') * 10 + (value[1] - '0');
}

std::optional<int> parse_four_digits(std::string_view value) {
    if (value.size() != 4) {
        return std::nullopt;
    }

    auto result = 0;
    for (const auto ch : value) {
        if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
            return std::nullopt;
        }
        result = result * 10 + (ch - '0');
    }
    return result;
}

std::optional<std::tm> parse_utc_tm(std::string_view value) {
    if (value.size() != 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' || value[13] != ':' ||
        value[16] != ':' || value[19] != 'Z') {
        return std::nullopt;
    }

    const auto year = parse_four_digits(value.substr(0, 4));
    const auto month = parse_two_digits(value.substr(5, 2));
    const auto day = parse_two_digits(value.substr(8, 2));
    const auto hour = parse_two_digits(value.substr(11, 2));
    const auto minute = parse_two_digits(value.substr(14, 2));
    const auto second = parse_two_digits(value.substr(17, 2));
    if (!year || !month || !day || !hour || !minute || !second || *month < 1 || *month > 12 || *day < 1 ||
        *day > 31 || *hour > 23 || *minute > 59 || *second > 59) {
        return std::nullopt;
    }

    std::tm utc{};
    utc.tm_year = *year - 1900;
    utc.tm_mon = *month - 1;
    utc.tm_mday = *day;
    utc.tm_hour = *hour;
    utc.tm_min = *minute;
    utc.tm_sec = *second;
    return utc;
}

}  // namespace

std::string format_sqlite_timestamp(core::TimePoint timestamp) {
    const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(timestamp);
    const auto epoch = core::Clock::to_time_t(seconds);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &epoch);
#else
    gmtime_r(&epoch, &utc);
#endif

    std::array<char, 21> output{};
    std::snprintf(output.data(), output.size(), "%04d-%02d-%02dT%02d:%02d:%02dZ", utc.tm_year + 1900,
                  utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
    return output.data();
}

std::optional<core::TimePoint> parse_sqlite_timestamp(std::string_view value) {
    auto utc = parse_utc_tm(value);
    if (!utc) {
        return std::nullopt;
    }

#ifdef _WIN32
    const auto epoch = _mkgmtime(&*utc);
#else
    const auto epoch = timegm(&*utc);
#endif
    if (epoch == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    return core::Clock::from_time_t(epoch);
}

std::optional<std::string> format_optional_sqlite_timestamp(std::optional<core::TimePoint> timestamp) {
    if (!timestamp) {
        return std::nullopt;
    }
    return format_sqlite_timestamp(*timestamp);
}

std::optional<core::TimePoint> parse_optional_sqlite_timestamp(const std::optional<std::string>& value) {
    if (!value) {
        return std::nullopt;
    }
    return parse_sqlite_timestamp(*value);
}

}  // namespace tradingbot::persistence
