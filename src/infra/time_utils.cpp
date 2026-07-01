#include "tradingbot/infra/time_utils.hpp"

#include <cctype>
#include <ctime>

namespace tradingbot::infra {
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

}  // namespace

std::optional<core::TimePoint> parse_iso_offset_timestamp(std::string_view value) {
    if (value.size() != 25 || value[4] != '-' || value[7] != '-' || value[10] != 'T' || value[13] != ':' ||
        value[16] != ':' || (value[19] != '+' && value[19] != '-') || value[22] != ':') {
        return std::nullopt;
    }

    const auto year = parse_four_digits(value.substr(0, 4));
    const auto month = parse_two_digits(value.substr(5, 2));
    const auto day = parse_two_digits(value.substr(8, 2));
    const auto hour = parse_two_digits(value.substr(11, 2));
    const auto minute = parse_two_digits(value.substr(14, 2));
    const auto second = parse_two_digits(value.substr(17, 2));
    const auto offset_hour = parse_two_digits(value.substr(20, 2));
    const auto offset_minute = parse_two_digits(value.substr(23, 2));
    if (!year || !month || !day || !hour || !minute || !second || !offset_hour || !offset_minute ||
        *month < 1 || *month > 12 || *day < 1 || *day > 31 || *hour > 23 || *minute > 59 || *second > 59 ||
        *offset_hour > 23 || *offset_minute > 59) {
        return std::nullopt;
    }

    std::tm utc{};
    utc.tm_year = *year - 1900;
    utc.tm_mon = *month - 1;
    utc.tm_mday = *day;
    utc.tm_hour = *hour;
    utc.tm_min = *minute;
    utc.tm_sec = *second;
#ifdef _WIN32
    auto epoch = _mkgmtime(&utc);
#else
    auto epoch = timegm(&utc);
#endif
    if (epoch == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    const auto offset_seconds = (*offset_hour * 60 + *offset_minute) * 60;
    epoch += value[19] == '+' ? -offset_seconds : offset_seconds;
    return core::Clock::from_time_t(epoch);
}

}  // namespace tradingbot::infra
