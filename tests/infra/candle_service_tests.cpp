#include "tradingbot/infra/candle_service.hpp"

#include <chrono>
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

class FakeTransport final : public tradingbot::infra::HttpTransport {
public:
    std::vector<tradingbot::infra::HttpResponse> responses;
    std::vector<tradingbot::infra::HttpRequest> requests;

    tradingbot::infra::HttpResponse send(const tradingbot::infra::HttpRequest& request) override {
        requests.push_back(request);
        if (responses.empty()) {
            return {.status_code = 500, .body = "missing fake response"};
        }
        auto response = responses.front();
        responses.erase(responses.begin());
        return response;
    }
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

tradingbot::infra::CandleQuery query() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .unit = "days",
        .interval = 1,
        .from_date = "2026-06-01",
        .to_date = "2026-06-30",
    };
}

std::string valid_candle_body() {
    return R"json({
        "status": "success",
        "data": {
            "candles": [
                ["2026-06-30T00:00:00+05:30", 100.0, 110.0, 95.0, 105.0, 12345],
                ["2026-06-29T00:00:00+05:30", 90.0, 100.0, 85.0, 95.0, 54321]
            ]
        }
    })json";
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

void builds_historical_candle_path() {
    const auto path = tradingbot::infra::historical_candle_path(query());

    require(path == "/v3/historical-candle/NSE_EQ%7CINE002A01018/days/1/2026-06-30/2026-06-01",
            "path should match historical candle V3 shape");
}

void parses_ohlcv_candles() {
    tradingbot::infra::ApiResult api_result;
    api_result.ok = true;
    api_result.response.status_code = 200;
    api_result.response.body = valid_candle_body();

    const auto result = tradingbot::infra::parse_historical_candle_response(query(), api_result);

    require(result.ok, "valid candle response should parse");
    require(result.candles.size() == 2, "two candles should parse");
    require(result.candles.front().open == 100.0, "open should parse");
    require(result.candles.front().high == 110.0, "high should parse");
    require(result.candles.front().low == 95.0, "low should parse");
    require(result.candles.front().close == 105.0, "close should parse");
    require(result.candles.front().volume == 12345, "volume should parse");
    require(tradingbot::core::Clock::to_time_t(result.candles.front().timestamp) ==
                utc_epoch(2026, 6, 29, 18, 30, 0),
            "timestamp should convert from +05:30 to UTC");
}

void rejects_missing_candles() {
    tradingbot::infra::ApiResult api_result;
    api_result.ok = true;
    api_result.response.status_code = 200;
    api_result.response.body = R"json({"status":"success","data":{}})json";

    const auto result = tradingbot::infra::parse_historical_candle_response(query(), api_result);

    require(!result.ok, "missing candles should fail");
    require(result.error.find("candles") != std::string::npos, "error should mention candles");
}

void rejects_malformed_candle_timestamp() {
    tradingbot::infra::ApiResult api_result;
    api_result.ok = true;
    api_result.response.status_code = 200;
    api_result.response.body = R"json({
        "status": "success",
        "data": {
            "candles": [
                ["not-a-time", 100.0, 110.0, 95.0, 105.0, 12345]
            ]
        }
    })json";

    const auto result = tradingbot::infra::parse_historical_candle_response(query(), api_result);

    require(!result.ok, "malformed candle timestamp should fail");
    require(result.error.find("timestamp") != std::string::npos, "error should mention timestamp");
}

void uses_cache_after_first_fetch() {
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 200, .body = valid_candle_body()}};
    auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api.upstox.com", "token", transport);
    auto cache = std::make_shared<tradingbot::infra::InMemoryCandleCache>();
    tradingbot::infra::CandleService service(client, cache);

    const auto first = service.fetch_candles(query());
    const auto second = service.fetch_candles(query());

    require(first.ok, "first fetch should succeed");
    require(!first.cache_hit, "first fetch should not be cache hit");
    require(second.ok, "second fetch should succeed");
    require(second.cache_hit, "second fetch should be cache hit");
    require(transport->requests.size() == 1, "cache should avoid second API call");
}

}  // namespace

int main() {
    builds_historical_candle_path();
    parses_ohlcv_candles();
    rejects_missing_candles();
    rejects_malformed_candle_timestamp();
    uses_cache_after_first_fetch();
    return 0;
}
