#include "tradingbot/order/order_monitor.hpp"

#include <cctype>
#include <string_view>
#include <utility>

namespace tradingbot::order {
namespace {

std::string to_lower(std::string value) {
    for (auto& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool contains_success_status(const std::string& body) {
    const auto status_pos = body.find("\"status\"");
    return status_pos != std::string::npos && body.find("\"success\"", status_pos) != std::string::npos;
}

bool extract_string_after_key(std::string_view body, const std::string& key, std::string& value) {
    const auto key_pos = body.find("\"" + key + "\"");
    if (key_pos == std::string_view::npos) {
        return false;
    }
    const auto colon_pos = body.find(':', key_pos);
    if (colon_pos == std::string_view::npos) {
        return false;
    }
    const auto first_quote = body.find('"', colon_pos + 1);
    if (first_quote == std::string_view::npos) {
        return false;
    }
    const auto second_quote = body.find('"', first_quote + 1);
    if (second_quote == std::string_view::npos) {
        return false;
    }
    value = std::string{body.substr(first_quote + 1, second_quote - first_quote - 1)};
    return true;
}

bool extract_number_after_key(std::string_view body, const std::string& key, double& value) {
    const auto key_pos = body.find("\"" + key + "\"");
    if (key_pos == std::string_view::npos) {
        return false;
    }
    const auto colon_pos = body.find(':', key_pos);
    if (colon_pos == std::string_view::npos) {
        return false;
    }
    auto begin = colon_pos + 1;
    while (begin < body.size() && std::isspace(static_cast<unsigned char>(body[begin])) != 0) {
        ++begin;
    }
    auto end = begin;
    while (end < body.size() &&
           (std::isdigit(static_cast<unsigned char>(body[end])) != 0 || body[end] == '.' || body[end] == '-')) {
        ++end;
    }
    if (begin == end) {
        return false;
    }
    try {
        value = std::stod(std::string{body.substr(begin, end - begin)});
    } catch (...) {
        return false;
    }
    return true;
}

core::OrderSide parse_side(const std::string& value) {
    return to_lower(value) == "sell" ? core::OrderSide::Sell : core::OrderSide::Buy;
}

core::OrderType parse_order_type(const std::string& value) {
    return to_lower(value) == "market" ? core::OrderType::Market : core::OrderType::Limit;
}

core::OrderRecord parse_order_object(std::string_view object) {
    core::OrderRecord record;
    std::string instrument_key;
    std::string status;
    std::string order_id;
    std::string side;
    std::string order_type;
    double quantity = 0.0;
    double price = 0.0;
    double filled_quantity = 0.0;
    double average_price = 0.0;

    extract_string_after_key(object, "instrument_token", instrument_key) ||
        extract_string_after_key(object, "instrument_key", instrument_key);
    extract_string_after_key(object, "status", status);
    extract_string_after_key(object, "order_id", order_id);
    extract_string_after_key(object, "transaction_type", side);
    extract_string_after_key(object, "order_type", order_type);
    extract_number_after_key(object, "quantity", quantity);
    extract_number_after_key(object, "price", price);
    extract_number_after_key(object, "filled_quantity", filled_quantity);
    extract_number_after_key(object, "average_price", average_price);

    record.request.instrument_key.value = instrument_key;
    record.request.side = parse_side(side);
    record.request.quantity = static_cast<core::Quantity>(quantity);
    record.request.price = price;
    record.request.order_type = parse_order_type(order_type);
    record.broker_order_id = order_id;
    record.status = map_upstox_order_status(status);
    record.filled_quantity = static_cast<core::Quantity>(filled_quantity);
    if (average_price > 0.0) {
        record.average_fill_price = average_price;
    }
    record.redacted_response_metadata = "source=upstox_order_book";
    record.updated_at = core::Clock::now();
    return record;
}

}  // namespace

OrderMonitor::OrderMonitor(std::shared_ptr<infra::UpstoxApiClient> api_client) : api_client_(std::move(api_client)) {}

OrderTrackingResult OrderMonitor::fetch_order_book() {
    if (!api_client_) {
        return {.ok = false, .error = "Upstox API client is required"};
    }
    return parse_order_book_response(api_client_->get(order_book_path()));
}

std::string order_book_path() {
    return "/v2/order/retrieve-all";
}

core::OrderStatus map_upstox_order_status(const std::string& status) {
    const auto normalized = to_lower(status);
    if (normalized == "complete" || normalized == "completed") {
        return core::OrderStatus::Filled;
    }
    if (normalized == "cancelled" || normalized == "cancelled after market order") {
        return core::OrderStatus::Cancelled;
    }
    if (normalized == "rejected") {
        return core::OrderStatus::Rejected;
    }
    if (normalized == "open") {
        return core::OrderStatus::Accepted;
    }
    if (normalized == "partial" || normalized == "partially filled") {
        return core::OrderStatus::PartiallyFilled;
    }
    return core::OrderStatus::Pending;
}

bool is_terminal_order_status(core::OrderStatus status) {
    return status == core::OrderStatus::Rejected || status == core::OrderStatus::Filled ||
           status == core::OrderStatus::Cancelled || status == core::OrderStatus::TimedOut;
}

OrderTrackingResult parse_order_book_response(const infra::ApiResult& api_result) {
    OrderTrackingResult result;
    result.api_event = api_result.event;
    if (!api_result.ok) {
        result.error = api_result.error.empty() ? "order book request failed" : api_result.error;
        return result;
    }
    if (!contains_success_status(api_result.response.body)) {
        result.error = "order book response status is not success";
        return result;
    }

    const auto data_pos = api_result.response.body.find("\"data\"");
    if (data_pos == std::string::npos) {
        result.error = "order book response is missing data";
        return result;
    }

    auto search_from = api_result.response.body.find('{', data_pos);
    while (search_from != std::string::npos) {
        const auto object_end = api_result.response.body.find('}', search_from + 1);
        if (object_end == std::string::npos) {
            break;
        }
        const auto object = std::string_view{api_result.response.body}.substr(search_from, object_end - search_from + 1);
        if (object.find("\"order_id\"") != std::string_view::npos) {
            result.records.push_back(parse_order_object(object));
        }
        search_from = api_result.response.body.find('{', object_end + 1);
    }

    result.ok = true;
    return result;
}

}  // namespace tradingbot::order

