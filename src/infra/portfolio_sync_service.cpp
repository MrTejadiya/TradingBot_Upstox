#include "tradingbot/infra/portfolio_sync_service.hpp"

#include <cctype>
#include <chrono>
#include <string_view>
#include <utility>

namespace tradingbot::infra {
namespace {

bool contains_success_status(const std::string& body) {
    const auto status_pos = body.find("\"status\"");
    if (status_pos == std::string::npos) {
        return false;
    }
    return body.find("\"success\"", status_pos) != std::string::npos;
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

bool extract_quantity_after_key(std::string_view body, const std::string& key, core::Quantity& value) {
    double parsed = 0.0;
    if (!extract_number_after_key(body, key, parsed)) {
        return false;
    }
    value = static_cast<core::Quantity>(parsed);
    return true;
}

std::string_view data_section(const std::string& body) {
    const auto data_pos = body.find("\"data\"");
    if (data_pos == std::string::npos) {
        return {};
    }
    const auto section_start = body.find_first_of("[{", data_pos);
    if (section_start == std::string::npos) {
        return {};
    }
    return std::string_view{body}.substr(section_start);
}

bool parse_available_funds(const std::string& body, core::Money& available_funds) {
    const auto data = data_section(body);
    if (data.empty()) {
        return false;
    }

    return extract_number_after_key(data, "available_margin", available_funds) ||
           extract_number_after_key(data, "available_to_trade", available_funds) ||
           extract_number_after_key(data, "total", available_funds);
}

std::vector<core::Holding> parse_holdings(const std::string& body) {
    std::vector<core::Holding> holdings;
    const auto data = data_section(body);
    auto search_from = data.find('{');
    while (search_from != std::string_view::npos) {
        const auto object_end = data.find('}', search_from + 1);
        if (object_end == std::string_view::npos) {
            break;
        }
        const auto object = data.substr(search_from, object_end - search_from + 1);

        std::string instrument_key;
        core::Quantity quantity = 0;
        double average_price = 0.0;
        if ((extract_string_after_key(object, "instrument_token", instrument_key) ||
             extract_string_after_key(object, "instrument_key", instrument_key)) &&
            extract_quantity_after_key(object, "quantity", quantity) &&
            extract_number_after_key(object, "average_price", average_price) && quantity > 0 && average_price >= 0.0) {
            holdings.push_back({
                .instrument_key = {.value = instrument_key},
                .quantity = quantity,
                .average_buy_price = average_price,
                .acquired_at = core::Clock::now(),
            });
        }
        search_from = data.find('{', object_end + 1);
    }
    return holdings;
}

}  // namespace

PortfolioSyncService::PortfolioSyncService(std::shared_ptr<UpstoxApiClient> api_client)
    : api_client_(std::move(api_client)) {}

PortfolioSyncResult PortfolioSyncService::sync() {
    if (!api_client_) {
        return {.ok = false, .error = "Upstox API client is required"};
    }
    const auto funds_result = api_client_->get(funds_and_margin_path());
    const auto holdings_result = api_client_->get(long_term_holdings_path());
    return parse_portfolio_sync_response(funds_result, holdings_result);
}

std::string funds_and_margin_path() {
    return "/v2/user/get-funds-and-margin?segment=SEC";
}

std::string long_term_holdings_path() {
    return "/v2/portfolio/long-term-holdings";
}

PortfolioSyncResult parse_portfolio_sync_response(const ApiResult& funds_result, const ApiResult& holdings_result) {
    PortfolioSyncResult result;
    result.api_events = {funds_result.event, holdings_result.event};
    result.portfolio.updated_at = core::Clock::now();

    if (!funds_result.ok) {
        result.error = funds_result.error.empty() ? "funds and margin request failed" : funds_result.error;
        return result;
    }
    if (!holdings_result.ok) {
        result.error = holdings_result.error.empty() ? "holdings request failed" : holdings_result.error;
        return result;
    }
    if (!contains_success_status(funds_result.response.body)) {
        result.error = "funds and margin response status is not success";
        return result;
    }
    if (!contains_success_status(holdings_result.response.body)) {
        result.error = "holdings response status is not success";
        return result;
    }
    if (!parse_available_funds(funds_result.response.body, result.portfolio.available_funds)) {
        result.error = "funds and margin response is missing available funds";
        return result;
    }

    result.portfolio.holdings = parse_holdings(holdings_result.response.body);
    result.ok = true;
    return result;
}

}  // namespace tradingbot::infra
