#include "tradingbot/order/order_monitor.hpp"

#include <chrono>
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

std::string order_book_body() {
    return R"json({
        "status": "success",
        "data": [
            {
                "order_id": "ORDER-1",
                "status": "complete",
                "instrument_token": "NSE_EQ|INE002A01018",
                "transaction_type": "BUY",
                "order_type": "LIMIT",
                "quantity": 3,
                "price": 100.5,
                "filled_quantity": 3,
                "average_price": 100.25
            },
            {
                "order_id": "ORDER-2",
                "status": "open",
                "instrument_token": "NSE_EQ|INE467B01029",
                "transaction_type": "SELL",
                "order_type": "MARKET",
                "quantity": 2,
                "price": 0,
                "filled_quantity": 0
            }
        ]
    })json";
}

void maps_upstox_statuses() {
    require(tradingbot::order::map_upstox_order_status("complete") == tradingbot::core::OrderStatus::Filled,
            "complete should map to filled");
    require(tradingbot::order::map_upstox_order_status("cancelled") == tradingbot::core::OrderStatus::Cancelled,
            "cancelled should map to cancelled");
    require(tradingbot::order::map_upstox_order_status("rejected") == tradingbot::core::OrderStatus::Rejected,
            "rejected should map to rejected");
    require(tradingbot::order::map_upstox_order_status("open") == tradingbot::core::OrderStatus::Accepted,
            "open should map to accepted");
}

void identifies_terminal_statuses() {
    require(tradingbot::order::is_terminal_order_status(tradingbot::core::OrderStatus::Filled),
            "filled should be terminal");
    require(tradingbot::order::is_terminal_order_status(tradingbot::core::OrderStatus::Cancelled),
            "cancelled should be terminal");
    require(!tradingbot::order::is_terminal_order_status(tradingbot::core::OrderStatus::Accepted),
            "accepted should not be terminal");
}

tradingbot::core::OrderRecord accepted_order(tradingbot::core::TimePoint updated_at) {
    return {
        .request = {.instrument_key = {"NSE_EQ|INE002A01018"}, .quantity = 1, .price = 100.0},
        .broker_order_id = "ORDER-TIMEOUT",
        .status = tradingbot::core::OrderStatus::Accepted,
        .updated_at = updated_at,
    };
}

void classifies_non_terminal_order_timeout() {
    const auto now = tradingbot::core::Clock::now();
    const auto record = accepted_order(now - std::chrono::minutes{6});

    const auto result = tradingbot::order::classify_order_timeout(record, now, std::chrono::minutes{5});

    require(result.status == tradingbot::core::OrderStatus::TimedOut, "old accepted order should time out");
    require(result.rejection_reason == "order monitoring timeout", "timeout reason should be recorded");
    require(result.updated_at == now, "timeout should update timestamp");
}

void timeout_does_not_override_terminal_order() {
    const auto now = tradingbot::core::Clock::now();
    auto record = accepted_order(now - std::chrono::minutes{6});
    record.status = tradingbot::core::OrderStatus::Filled;

    const auto result = tradingbot::order::classify_order_timeout(record, now, std::chrono::minutes{5});

    require(result.status == tradingbot::core::OrderStatus::Filled, "terminal order should remain terminal");
}

void disabled_timeout_leaves_order_unchanged() {
    const auto now = tradingbot::core::Clock::now();
    const auto record = accepted_order(now - std::chrono::minutes{6});

    const auto result = tradingbot::order::classify_order_timeout(record, now, std::chrono::seconds{0});

    require(result.status == tradingbot::core::OrderStatus::Accepted, "disabled timeout should leave status unchanged");
}

void parses_order_book_response() {
    tradingbot::infra::ApiResult api_result;
    api_result.ok = true;
    api_result.response.status_code = 200;
    api_result.response.body = order_book_body();

    const auto result = tradingbot::order::parse_order_book_response(api_result);

    require(result.ok, "valid order book should parse");
    require(result.records.size() == 2, "two records should parse");
    require(result.records.front().broker_order_id == "ORDER-1", "order id should parse");
    require(result.records.front().status == tradingbot::core::OrderStatus::Filled, "filled status should parse");
    require(result.records.front().request.instrument_key.value == "NSE_EQ|INE002A01018",
            "instrument key should parse");
    require(result.records.front().filled_quantity == 3, "filled quantity should parse");
    require(result.records.front().average_fill_price == 100.25, "average price should parse");
    require(result.records.back().request.side == tradingbot::core::OrderSide::Sell, "sell side should parse");
    require(result.records.back().request.order_type == tradingbot::core::OrderType::Market,
            "market order type should parse");
}

void fetches_order_book_through_api_client() {
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 200, .body = order_book_body()}};
    auto client = std::make_shared<tradingbot::infra::UpstoxApiClient>("https://api.upstox.com", "token", transport);
    tradingbot::order::OrderMonitor monitor(client);

    const auto result = monitor.fetch_order_book();

    require(result.ok, "monitor should fetch order book");
    require(transport->requests.size() == 1, "monitor should send one request");
    require(transport->requests.front().url == "https://api.upstox.com/v2/order/retrieve-all",
            "monitor should call order book endpoint");
}

void rejects_unsuccessful_response() {
    tradingbot::infra::ApiResult api_result;
    api_result.ok = true;
    api_result.response.status_code = 200;
    api_result.response.body = R"json({"status":"error","data":[]})json";

    const auto result = tradingbot::order::parse_order_book_response(api_result);

    require(!result.ok, "error status should fail parse");
    require(result.error.find("success") != std::string::npos, "error should name status");
}

}  // namespace

int main() {
    maps_upstox_statuses();
    identifies_terminal_statuses();
    classifies_non_terminal_order_timeout();
    timeout_does_not_override_terminal_order();
    disabled_timeout_leaves_order_unchanged();
    parses_order_book_response();
    fetches_order_book_through_api_client();
    rejects_unsuccessful_response();
    return 0;
}
