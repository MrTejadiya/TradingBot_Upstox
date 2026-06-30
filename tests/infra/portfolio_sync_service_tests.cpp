#include "tradingbot/infra/portfolio_sync_service.hpp"

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

std::string valid_funds_body() {
    return R"json({
        "status": "success",
        "data": {
            "equity": {
                "used_margin": 0.8,
                "available_margin": 15507.46
            }
        }
    })json";
}

std::string valid_holdings_body() {
    return R"json({
        "status": "success",
        "data": [
            {
                "instrument_token": "NSE_EQ|INE528G01035",
                "quantity": 36,
                "average_price": 18.75,
                "trading_symbol": "YESBANK"
            },
            {
                "instrument_token": "NSE_EQ|INE036A01016",
                "quantity": 12,
                "average_price": 102.5,
                "trading_symbol": "RELIANCE"
            }
        ]
    })json";
}

void exposes_upstox_portfolio_paths() {
    require(tradingbot::infra::funds_and_margin_path() == "/v2/user/get-funds-and-margin?segment=SEC",
            "funds path should request equity segment");
    require(tradingbot::infra::long_term_holdings_path() == "/v2/portfolio/long-term-holdings",
            "holdings path should match Upstox endpoint");
}

void parses_successful_portfolio_sync_response() {
    tradingbot::infra::ApiResult funds;
    funds.ok = true;
    funds.response.status_code = 200;
    funds.response.body = valid_funds_body();
    tradingbot::infra::ApiResult holdings;
    holdings.ok = true;
    holdings.response.status_code = 200;
    holdings.response.body = valid_holdings_body();

    const auto result = tradingbot::infra::parse_portfolio_sync_response(funds, holdings);

    require(result.ok, "valid portfolio sync response should parse");
    require(result.portfolio.available_funds == 15507.46, "available funds should parse");
    require(result.portfolio.holdings.size() == 2, "holdings should parse");
    require(result.portfolio.holdings.front().instrument_key.value == "NSE_EQ|INE528G01035",
            "holding instrument key should parse");
    require(result.portfolio.holdings.front().quantity == 36, "holding quantity should parse");
    require(result.portfolio.holdings.front().average_buy_price == 18.75, "holding average price should parse");
}

void accepts_empty_holdings_portfolio() {
    tradingbot::infra::ApiResult funds;
    funds.ok = true;
    funds.response.status_code = 200;
    funds.response.body = valid_funds_body();
    tradingbot::infra::ApiResult holdings;
    holdings.ok = true;
    holdings.response.status_code = 200;
    holdings.response.body = R"json({"status":"success","data":[]})json";

    const auto result = tradingbot::infra::parse_portfolio_sync_response(funds, holdings);

    require(result.ok, "empty holdings response should still sync");
    require(result.portfolio.holdings.empty(), "empty holdings should be represented");
}

void rejects_missing_available_funds() {
    tradingbot::infra::ApiResult funds;
    funds.ok = true;
    funds.response.status_code = 200;
    funds.response.body = R"json({"status":"success","data":{"equity":{}}})json";
    tradingbot::infra::ApiResult holdings;
    holdings.ok = true;
    holdings.response.status_code = 200;
    holdings.response.body = valid_holdings_body();

    const auto result = tradingbot::infra::parse_portfolio_sync_response(funds, holdings);

    require(!result.ok, "missing available funds should fail");
    require(result.error.find("available funds") != std::string::npos, "error should name available funds");
}

void syncs_through_api_client() {
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 200, .body = valid_funds_body()},
                            {.status_code = 200, .body = valid_holdings_body()}};
    auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api.upstox.com", "token", transport);
    tradingbot::infra::PortfolioSyncService service(client);

    const auto result = service.sync();

    require(result.ok, "service sync should succeed: " + result.error);
    require(transport->requests.size() == 2, "service should call funds and holdings endpoints");
    require(transport->requests[0].url == "https://api.upstox.com/v2/user/get-funds-and-margin?segment=SEC",
            "first request should sync funds");
    require(transport->requests[1].url == "https://api.upstox.com/v2/portfolio/long-term-holdings",
            "second request should sync holdings");
}

}  // namespace

int main() {
    exposes_upstox_portfolio_paths();
    parses_successful_portfolio_sync_response();
    accepts_empty_holdings_portfolio();
    rejects_missing_available_funds();
    syncs_through_api_client();
    return 0;
}
