#include "tradingbot/order/live_order_dispatcher.hpp"

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
            return {.status_code = 200, .body = R"json({"status":"success","data":{"order_id":"ORDER-1"}})json"};
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

tradingbot::core::OrderRequest valid_request() {
    return {
        .instrument_key = {"NSE_EQ|INE002A01018"},
        .side = tradingbot::core::OrderSide::Buy,
        .quantity = 3,
        .price = 100.0,
        .tag = "live-test",
        .source_strategy = "manual_buy",
        .run_id = "run-1",
    };
}

tradingbot::order::LiveOrderSafetyGates open_gates() {
    return {
        .live_trading_enabled = true,
        .risk_event = {
            .instrument_key = {"NSE_EQ|INE002A01018"},
            .decision = tradingbot::core::RiskDecision::Approved,
            .reason_code = "APPROVED",
            .detail = "ok",
            .timestamp = tradingbot::core::Clock::now(),
        },
        .market_session = {
            .market_open = true,
            .order_allowed = true,
            .reason_code = "ORDER_ALLOWED",
            .detail = "ok",
        },
        .rate_limit_time = tradingbot::order::RateLimitTimePoint{std::chrono::milliseconds{0}},
    };
}

void builds_upstox_place_order_payload() {
    const auto payload = tradingbot::order::build_upstox_place_order_v3_payload(valid_request());

    require(payload.find("\"quantity\":3") != std::string::npos, "payload should include quantity");
    require(payload.find("\"product\":\"D\"") != std::string::npos, "payload should include delivery product");
    require(payload.find("\"instrument_token\":\"NSE_EQ|INE002A01018\"") != std::string::npos,
            "payload should include instrument token");
    require(payload.find("\"order_type\":\"LIMIT\"") != std::string::npos, "payload should include order type");
    require(payload.find("\"transaction_type\":\"BUY\"") != std::string::npos, "payload should include side");
    require(payload.find("\"slice\":true") != std::string::npos, "payload should enable slicing");
}

void places_live_order_when_all_gates_pass() {
    auto transport = std::make_shared<FakeTransport>();
    auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api-hft.upstox.com", "token", transport);
    tradingbot::order::LiveOrderDispatcher dispatcher(client, {.max_executions = 2});

    const auto result = dispatcher.dispatch(valid_request(), open_gates(), tradingbot::core::Clock::now());

    require(result.accepted, "live order should be accepted when gates pass");
    require(result.record.broker_order_id == "ORDER-1", "broker order id should parse");
    require(result.record.status == tradingbot::core::OrderStatus::Accepted, "record status should be accepted");
    require(transport->requests.size() == 1, "one API request should be sent");
    require(transport->requests.front().method == "POST", "request should be POST");
    require(transport->requests.front().url == "https://api-hft.upstox.com/v3/order/place",
            "request should use Upstox V3 place order path");
}

void live_disabled_blocks_api_call() {
    auto transport = std::make_shared<FakeTransport>();
    auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api-hft.upstox.com", "token", transport);
    tradingbot::order::LiveOrderDispatcher dispatcher(client);
    auto gates = open_gates();
    gates.live_trading_enabled = false;

    const auto result = dispatcher.dispatch(valid_request(), gates, tradingbot::core::Clock::now());

    require(!result.accepted, "disabled live trading should reject");
    require(transport->requests.empty(), "disabled live trading should not call API");
    require(result.record.rejection_reason.find("disabled") != std::string::npos, "rejection should name disabled gate");
}

void risk_gate_blocks_api_call() {
    auto transport = std::make_shared<FakeTransport>();
    auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api-hft.upstox.com", "token", transport);
    tradingbot::order::LiveOrderDispatcher dispatcher(client);
    auto gates = open_gates();
    gates.risk_event.decision = tradingbot::core::RiskDecision::Rejected;
    gates.risk_event.reason_code = "INSUFFICIENT_FUNDS";

    const auto result = dispatcher.dispatch(valid_request(), gates, tradingbot::core::Clock::now());

    require(!result.accepted, "risk rejection should reject live order");
    require(transport->requests.empty(), "risk rejection should not call API");
    require(result.record.rejection_reason.find("INSUFFICIENT_FUNDS") != std::string::npos,
            "rejection should include risk reason");
}

void market_session_gate_blocks_api_call() {
    auto transport = std::make_shared<FakeTransport>();
    auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api-hft.upstox.com", "token", transport);
    tradingbot::order::LiveOrderDispatcher dispatcher(client);
    auto gates = open_gates();
    gates.market_session.order_allowed = false;
    gates.market_session.reason_code = "AFTER_CLOSE";

    const auto result = dispatcher.dispatch(valid_request(), gates, tradingbot::core::Clock::now());

    require(!result.accepted, "closed order window should reject live order");
    require(transport->requests.empty(), "closed order window should not call API");
    require(result.record.rejection_reason.find("AFTER_CLOSE") != std::string::npos,
            "rejection should include session reason");
}

void rate_limit_blocks_second_api_call() {
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 200, .body = R"json({"status":"success","data":{"order_id":"ORDER-1"}})json"},
                            {.status_code = 200, .body = R"json({"status":"success","data":{"order_id":"ORDER-2"}})json"}};
    auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api-hft.upstox.com", "token", transport);
    tradingbot::order::LiveOrderDispatcher dispatcher(client, {.max_executions = 1, .window = std::chrono::milliseconds{1000}});
    auto gates = open_gates();

    require(dispatcher.dispatch(valid_request(), gates, tradingbot::core::Clock::now()).accepted,
            "first request should pass");
    gates.rate_limit_time = tradingbot::order::RateLimitTimePoint{std::chrono::milliseconds{1}};
    const auto second = dispatcher.dispatch(valid_request(), gates, tradingbot::core::Clock::now());

    require(!second.accepted, "second request in rate window should reject");
    require(transport->requests.size() == 1, "rate-limited request should not call API");
}

void failed_api_response_rejects_record() {
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 400, .body = R"json({"status":"error"})json"}};
    auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api-hft.upstox.com", "token", transport);
    tradingbot::order::LiveOrderDispatcher dispatcher(client, {.max_executions = 2});

    const auto result = dispatcher.dispatch(valid_request(), open_gates(), tradingbot::core::Clock::now());

    require(!result.accepted, "API error should reject live record");
    require(result.record.status == tradingbot::core::OrderStatus::Rejected, "API error status should reject");
}

}  // namespace

int main() {
    builds_upstox_place_order_payload();
    places_live_order_when_all_gates_pass();
    live_disabled_blocks_api_call();
    risk_gate_blocks_api_call();
    market_session_gate_blocks_api_call();
    rate_limit_blocks_second_api_call();
    failed_api_response_rejects_record();
    return 0;
}

