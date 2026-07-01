#include "tradingbot/order/live_order_dispatcher.hpp"

#include <sstream>
#include <utility>

namespace tradingbot::order {
namespace {

std::string side_name(core::OrderSide side) {
    return side == core::OrderSide::Buy ? "BUY" : "SELL";
}

std::string order_type_name(core::OrderType type) {
    return type == core::OrderType::Market ? "MARKET" : "LIMIT";
}

std::string product_name(core::ProductType product) {
    return product == core::ProductType::Delivery ? "D" : "D";
}

std::string validity_name(core::OrderValidity validity) {
    return validity == core::OrderValidity::Day ? "DAY" : "DAY";
}

bool contains_success_status(const std::string& body) {
    const auto status_pos = body.find("\"status\"");
    return status_pos != std::string::npos && body.find("\"success\"", status_pos) != std::string::npos;
}

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (const auto ch : value) {
        if (ch == '"' || ch == '\\') {
            out << '\\';
        }
        out << ch;
    }
    return out.str();
}

}  // namespace

LiveOrderDispatcher::LiveOrderDispatcher(std::shared_ptr<infra::UpstoxApiClient> api_client,
                                         RateLimitConfig rate_limit_config)
    : api_client_(std::move(api_client)), executor_(rate_limit_config) {}

DispatchResult LiveOrderDispatcher::dispatch(const core::OrderRequest& request, const LiveOrderSafetyGates& gates,
                                             core::TimePoint dispatched_at) {
    if (!gates.live_trading_enabled) {
        return reject(request, "live trading is disabled", dispatched_at);
    }
    if (gates.risk_event.decision != core::RiskDecision::Approved) {
        return reject(request, "risk gate rejected: " + gates.risk_event.reason_code, dispatched_at);
    }
    if (!gates.market_session.order_allowed) {
        return reject(request, "market session gate rejected: " + gates.market_session.reason_code, dispatched_at);
    }
    if (!api_client_) {
        return reject(request, "Upstox API client is required", dispatched_at);
    }

    auto api_result = infra::ApiResult{};
    const auto execution = executor_.try_execute(gates.rate_limit_time, [&] {
        api_result = api_client_->post(upstox_place_order_v3_path(), build_upstox_place_order_v3_payload(request));
        return api_result.response.body;
    });
    if (!execution.executed) {
        return reject(request, "rate limit gate rejected: " + execution.rate_limit.reason_code, dispatched_at);
    }
    if (!api_result.ok) {
        return reject(request, api_result.error.empty() ? "live order placement failed" : api_result.error, dispatched_at);
    }
    if (!contains_success_status(api_result.response.body)) {
        return reject(request, "live order response status is not success", dispatched_at);
    }

    const auto order_id = parse_upstox_order_id(api_result.response.body);
    if (order_id.empty()) {
        return reject(request, "live order response is missing order_id", dispatched_at);
    }

    core::OrderRecord record{
        .request = request,
        .broker_order_id = order_id,
        .status = core::OrderStatus::Accepted,
        .filled_quantity = 0,
        .redacted_response_metadata = api_result.event.redacted_request_metadata,
        .updated_at = dispatched_at,
    };
    return {.accepted = true, .record = record};
}

DispatchResult LiveOrderDispatcher::reject(const core::OrderRequest& request, const std::string& reason,
                                           core::TimePoint dispatched_at) const {
    return {
        .accepted = false,
        .record = {
            .request = request,
            .status = core::OrderStatus::Rejected,
            .rejection_reason = reason,
            .updated_at = dispatched_at,
        },
    };
}

std::string upstox_place_order_v3_path() {
    return "/v3/order/place";
}

std::string build_upstox_place_order_v3_payload(const core::OrderRequest& request) {
    std::ostringstream out;
    out << "{";
    out << "\"quantity\":" << request.quantity << ",";
    out << "\"product\":\"" << product_name(request.product) << "\",";
    out << "\"validity\":\"" << validity_name(request.validity) << "\",";
    out << "\"price\":" << request.price << ",";
    out << "\"tag\":\"" << json_escape(request.tag) << "\",";
    out << "\"instrument_token\":\"" << json_escape(request.instrument_key.value) << "\",";
    out << "\"order_type\":\"" << order_type_name(request.order_type) << "\",";
    out << "\"transaction_type\":\"" << side_name(request.side) << "\",";
    out << "\"disclosed_quantity\":0,";
    out << "\"trigger_price\":0,";
    out << "\"is_amo\":false,";
    out << "\"slice\":true,";
    out << "\"market_protection\":0";
    out << "}";
    return out.str();
}

std::string parse_upstox_order_id(const std::string& response_body) {
    const auto key_pos = response_body.find("\"order_id\"");
    if (key_pos == std::string::npos) {
        return {};
    }
    const auto colon_pos = response_body.find(':', key_pos);
    if (colon_pos == std::string::npos) {
        return {};
    }
    const auto first_quote = response_body.find('"', colon_pos + 1);
    if (first_quote == std::string::npos) {
        return {};
    }
    const auto second_quote = response_body.find('"', first_quote + 1);
    if (second_quote == std::string::npos) {
        return {};
    }
    return response_body.substr(first_quote + 1, second_quote - first_quote - 1);
}

}  // namespace tradingbot::order

