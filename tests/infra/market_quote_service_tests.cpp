#include "tradingbot/infra/market_quote_service.hpp"

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

std::string valid_ltp_body() {
    return R"json({
        "status": "success",
        "data": {
            "NSE_EQ:INE002A01018": {
                "last_price": 303.9,
                "instrument_token": "NSE_EQ|INE002A01018",
                "timestamp": "2026-06-30T15:29:30+05:30",
                "ltq": 75,
                "volume": 170325,
                "cp": 29.0
            }
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

void builds_ltp_v3_path_with_encoded_instrument_key() {
    const auto path = tradingbot::infra::ltp_quote_path({"NSE_EQ|INE002A01018"});

    require(path == "/v3/market-quote/ltp?instrument_key=NSE_EQ%7CINE002A01018", "path should match Upstox LTP V3");
}

void parses_valid_ltp_response() {
    tradingbot::infra::ApiResult api_result;
    api_result.ok = true;
    api_result.response.status_code = 200;
    api_result.response.body = valid_ltp_body();

    const auto result = tradingbot::infra::parse_ltp_response({"NSE_EQ|INE002A01018"}, api_result);

    require(result.ok, "valid LTP response should parse");
    require(result.quote.instrument_key.value == "NSE_EQ|INE002A01018", "instrument key should be retained");
    require(result.quote.ltp == 303.9, "last_price should become ltp");
    require(!result.quote.stale, "fresh response should not be stale");
    require(tradingbot::core::Clock::to_time_t(result.quote.timestamp) == utc_epoch(2026, 6, 30, 9, 59, 30),
            "timestamp should convert from +05:30 to UTC");
}

void parses_ltp_without_timestamp_using_parse_time() {
    tradingbot::infra::ApiResult api_result;
    api_result.ok = true;
    api_result.response.status_code = 200;
    api_result.response.body = R"json({
        "status": "success",
        "data": {
            "NSE_EQ:INE002A01018": {
                "last_price": 303.9
            }
        }
    })json";

    const auto result = tradingbot::infra::parse_ltp_response({"NSE_EQ|INE002A01018"}, api_result);

    require(result.ok, "LTP response without timestamp should remain supported");
    require(result.quote.ltp == 303.9, "last_price should parse without timestamp");
}

void rejects_missing_last_price() {
    tradingbot::infra::ApiResult api_result;
    api_result.ok = true;
    api_result.response.status_code = 200;
    api_result.response.body = R"json({"status":"success","data":{"x":{"ltq":75}}})json";

    const auto result = tradingbot::infra::parse_ltp_response({"NSE_EQ|INE002A01018"}, api_result);

    require(!result.ok, "missing last_price should fail");
    require(result.error.find("last_price") != std::string::npos, "error should name last_price");
}

void rejects_malformed_timestamp() {
    tradingbot::infra::ApiResult api_result;
    api_result.ok = true;
    api_result.response.status_code = 200;
    api_result.response.body = R"json({"status":"success","data":{"x":{"last_price":303.9,"timestamp":"bad-time"}}})json";

    const auto result = tradingbot::infra::parse_ltp_response({"NSE_EQ|INE002A01018"}, api_result);

    require(!result.ok, "malformed timestamp should fail");
    require(result.error.find("timestamp") != std::string::npos, "error should mention timestamp");
}

void rejects_unsuccessful_status() {
    tradingbot::infra::ApiResult api_result;
    api_result.ok = true;
    api_result.response.status_code = 200;
    api_result.response.body = R"json({"status":"error","data":{}})json";

    const auto result = tradingbot::infra::parse_ltp_response({"NSE_EQ|INE002A01018"}, api_result);

    require(!result.ok, "non-success status should fail");
}

void fetches_ltp_through_api_client() {
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 200, .body = valid_ltp_body()}};
    auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api.upstox.com", "token", transport);
    tradingbot::infra::MarketQuoteService service(client);

    const auto result = service.fetch_ltp({"NSE_EQ|INE002A01018"});

    require(result.ok, "service fetch should succeed");
    require(result.quote.ltp == 303.9, "service should parse LTP");
    require(transport->requests.front().url ==
                "https://api.upstox.com/v3/market-quote/ltp?instrument_key=NSE_EQ%7CINE002A01018",
            "service should call Upstox LTP V3 endpoint");
}

}  // namespace

int main() {
    builds_ltp_v3_path_with_encoded_instrument_key();
    parses_valid_ltp_response();
    parses_ltp_without_timestamp_using_parse_time();
    rejects_missing_last_price();
    rejects_malformed_timestamp();
    rejects_unsuccessful_status();
    fetches_ltp_through_api_client();
    return 0;
}
