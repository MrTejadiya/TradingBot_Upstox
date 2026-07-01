#include "tradingbot/infra/candle_service.hpp"

#include <cctype>
#include <chrono>
#include <ctime>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace tradingbot::infra {
namespace {

std::string url_encode_instrument_key(std::string_view value) {
    std::ostringstream out;
    for (const auto ch : value) {
        if (ch == '|') {
            out << "%7C";
        } else if (ch == ' ') {
            out << "%20";
        } else {
            out << ch;
        }
    }
    return out.str();
}

bool contains_success_status(const std::string& body) {
    const auto status_pos = body.find("\"status\"");
    if (status_pos == std::string::npos) {
        return false;
    }
    return body.find("\"success\"", status_pos) != std::string::npos;
}

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

std::optional<core::TimePoint> parse_offset_timestamp(std::string_view value) {
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

std::optional<std::string_view> parse_first_string_from_array(std::string_view array_text) {
    const auto quote_start = array_text.find('"');
    if (quote_start == std::string_view::npos) {
        return std::nullopt;
    }
    const auto quote_end = array_text.find('"', quote_start + 1);
    if (quote_end == std::string_view::npos || quote_end == quote_start + 1) {
        return std::nullopt;
    }
    return array_text.substr(quote_start + 1, quote_end - quote_start - 1);
}

std::vector<double> parse_numbers_from_array(std::string_view array_text) {
    std::vector<double> numbers;
    for (std::size_t index = 0; index < array_text.size(); ++index) {
        const auto ch = array_text[index];
        if ((std::isdigit(static_cast<unsigned char>(ch)) == 0 && ch != '-' && ch != '.') ||
            (ch == '-' && index + 1 < array_text.size() &&
             std::isdigit(static_cast<unsigned char>(array_text[index + 1])) == 0)) {
            continue;
        }
        auto end = index + 1;
        while (end < array_text.size() &&
               (std::isdigit(static_cast<unsigned char>(array_text[end])) != 0 || array_text[end] == '.' ||
                array_text[end] == '-')) {
            ++end;
        }
        try {
            numbers.push_back(std::stod(std::string{array_text.substr(index, end - index)}));
        } catch (...) {
        }
        index = end;
    }
    return numbers;
}

}  // namespace

std::optional<std::vector<core::Candle>> InMemoryCandleCache::get(const CandleQuery& query) {
    const auto it = entries_.find(candle_cache_key(query));
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void InMemoryCandleCache::put(const CandleQuery& query, const std::vector<core::Candle>& candles) {
    entries_[candle_cache_key(query)] = candles;
}

CandleService::CandleService(std::shared_ptr<UpstoxApiClient> api_client, std::shared_ptr<CandleCache> cache)
    : api_client_(std::move(api_client)), cache_(std::move(cache)) {}

CandleResult CandleService::fetch_candles(const CandleQuery& query) {
    if (cache_) {
        const auto cached = cache_->get(query);
        if (cached) {
            return {.ok = true, .candles = *cached, .cache_hit = true};
        }
    }
    if (!api_client_) {
        return {.ok = false, .error = "Upstox API client is required"};
    }

    auto result = parse_historical_candle_response(query, api_client_->get(historical_candle_path(query)));
    if (result.ok && cache_) {
        cache_->put(query, result.candles);
    }
    return result;
}

std::string historical_candle_path(const CandleQuery& query) {
    return "/v3/historical-candle/" + url_encode_instrument_key(query.instrument_key.value) + "/" + query.unit + "/" +
           std::to_string(query.interval) + "/" + query.to_date + "/" + query.from_date;
}

CandleResult parse_historical_candle_response(const CandleQuery& query, const ApiResult& api_result) {
    CandleResult result;
    result.api_event = api_result.event;
    if (!api_result.ok) {
        result.error = api_result.error.empty() ? "historical candle request failed" : api_result.error;
        return result;
    }
    if (!contains_success_status(api_result.response.body)) {
        result.error = "historical candle response status is not success";
        return result;
    }

    const auto candles_pos = api_result.response.body.find("\"candles\"");
    if (candles_pos == std::string::npos) {
        result.error = "historical candle response is missing candles";
        return result;
    }

    auto search_from = api_result.response.body.find('[', candles_pos);
    while (search_from != std::string::npos) {
        const auto row_start = api_result.response.body.find('[', search_from + 1);
        if (row_start == std::string::npos) {
            break;
        }
        const auto row_end = api_result.response.body.find(']', row_start + 1);
        if (row_end == std::string::npos) {
            break;
        }
        auto row_text = std::string_view{api_result.response.body}.substr(row_start, row_end - row_start);
        const auto timestamp_text = parse_first_string_from_array(row_text);
        if (!timestamp_text) {
            result.error = "historical candle row is missing timestamp";
            return result;
        }
        const auto timestamp = parse_offset_timestamp(*timestamp_text);
        if (!timestamp) {
            result.error = "historical candle row has malformed timestamp";
            return result;
        }
        const auto first_comma = row_text.find(',');
        if (first_comma != std::string_view::npos) {
            row_text.remove_prefix(first_comma + 1);
        }
        const auto numbers = parse_numbers_from_array(row_text);
        if (numbers.size() >= 5) {
            result.candles.push_back({
                .instrument_key = query.instrument_key,
                .timestamp = *timestamp,
                .open = numbers[0],
                .high = numbers[1],
                .low = numbers[2],
                .close = numbers[3],
                .volume = static_cast<core::Quantity>(numbers[4]),
                .interval = query.unit + ":" + std::to_string(query.interval),
            });
        }
        search_from = row_end + 1;
    }

    if (result.candles.empty()) {
        result.error = "historical candle response does not contain parseable OHLCV candles";
        return result;
    }
    result.ok = true;
    return result;
}

std::string candle_cache_key(const CandleQuery& query) {
    return query.instrument_key.value + "|" + query.unit + "|" + std::to_string(query.interval) + "|" + query.from_date +
           "|" + query.to_date;
}

}  // namespace tradingbot::infra
