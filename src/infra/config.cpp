#include "tradingbot/infra/config.hpp"

#include <cctype>
#include <charconv>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

namespace tradingbot::infra {
namespace {

struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    using Value = std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject>;
    Value value;
};

class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    JsonValue parse() {
        auto value = parse_value();
        skip_ws();
        if (pos_ != text_.size()) {
            fail("unexpected trailing content");
        }
        return value;
    }

private:
    JsonValue parse_value() {
        skip_ws();
        if (pos_ >= text_.size()) {
            fail("unexpected end of JSON");
        }

        const auto ch = text_[pos_];
        if (ch == '{') {
            return JsonValue{parse_object()};
        }
        if (ch == '[') {
            return JsonValue{parse_array()};
        }
        if (ch == '"') {
            return JsonValue{parse_string()};
        }
        if (ch == 't' || ch == 'f') {
            return JsonValue{parse_bool()};
        }
        if (ch == 'n') {
            parse_null();
            return JsonValue{nullptr};
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            return JsonValue{parse_number()};
        }

        fail("unexpected JSON token");
    }

    JsonObject parse_object() {
        expect('{');
        JsonObject object;
        skip_ws();
        if (peek('}')) {
            ++pos_;
            return object;
        }

        while (true) {
            skip_ws();
            if (!peek('"')) {
                fail("expected object key string");
            }
            auto key = parse_string();
            skip_ws();
            expect(':');
            object.emplace(std::move(key), parse_value());
            skip_ws();
            if (peek('}')) {
                ++pos_;
                return object;
            }
            expect(',');
        }
    }

    JsonArray parse_array() {
        expect('[');
        JsonArray array;
        skip_ws();
        if (peek(']')) {
            ++pos_;
            return array;
        }

        while (true) {
            array.push_back(parse_value());
            skip_ws();
            if (peek(']')) {
                ++pos_;
                return array;
            }
            expect(',');
        }
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                return out;
            }
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }
            if (pos_ >= text_.size()) {
                fail("unterminated escape sequence");
            }
            const char escaped = text_[pos_++];
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(escaped);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                default:
                    fail("unsupported string escape");
            }
        }
        fail("unterminated string");
    }

    bool parse_bool() {
        if (text_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return true;
        }
        if (text_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return false;
        }
        fail("invalid boolean");
    }

    void parse_null() {
        if (text_.substr(pos_, 4) != "null") {
            fail("invalid null");
        }
        pos_ += 4;
    }

    double parse_number() {
        const auto start = pos_;
        if (peek('-')) {
            ++pos_;
        }
        consume_digits();
        if (peek('.')) {
            ++pos_;
            consume_digits();
        }
        if (peek('e') || peek('E')) {
            ++pos_;
            if (peek('+') || peek('-')) {
                ++pos_;
            }
            consume_digits();
        }

        double value = 0.0;
        const auto number_text = text_.substr(start, pos_ - start);
        const auto* begin = number_text.data();
        const auto* end = begin + number_text.size();
        const auto parsed = std::from_chars(begin, end, value);
        if (parsed.ec != std::errc{} || parsed.ptr != end) {
            fail("invalid number");
        }
        return value;
    }

    void consume_digits() {
        const auto start = pos_;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
        if (start == pos_) {
            fail("expected digit");
        }
    }

    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }

    bool peek(char expected) const {
        return pos_ < text_.size() && text_[pos_] == expected;
    }

    void expect(char expected) {
        skip_ws();
        if (!peek(expected)) {
            fail(std::string("expected '") + expected + "'");
        }
        ++pos_;
    }

    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error(message + " at byte " + std::to_string(pos_));
    }

    std::string_view text_;
    std::size_t pos_{0};
};

const JsonObject* as_object(const JsonValue& value) {
    return std::get_if<JsonObject>(&value.value);
}

const JsonValue* find_key(const JsonObject& object, const std::string& key) {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::optional<std::string> string_at(const JsonObject& object, const std::string& key) {
    const auto* value = find_key(object, key);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* str = std::get_if<std::string>(&value->value)) {
        return *str;
    }
    return std::nullopt;
}

std::optional<bool> bool_at(const JsonObject& object, const std::string& key) {
    const auto* value = find_key(object, key);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* boolean = std::get_if<bool>(&value->value)) {
        return *boolean;
    }
    return std::nullopt;
}

std::optional<double> number_at(const JsonObject& object, const std::string& key) {
    const auto* value = find_key(object, key);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* number = std::get_if<double>(&value->value)) {
        return *number;
    }
    return std::nullopt;
}

std::optional<std::vector<std::string>> string_array_at(const JsonObject& object, const std::string& key) {
    const auto* value = find_key(object, key);
    if (value == nullptr) {
        return std::nullopt;
    }
    const auto* array = std::get_if<JsonArray>(&value->value);
    if (array == nullptr) {
        return std::nullopt;
    }

    std::vector<std::string> strings;
    for (const auto& item : *array) {
        const auto* str = std::get_if<std::string>(&item.value);
        if (str == nullptr) {
            return std::nullopt;
        }
        strings.push_back(*str);
    }
    return strings;
}

app::Mode parse_mode_or_default(const std::string& value, std::vector<std::string>& errors) {
    if (value == "validate") {
        return app::Mode::Validate;
    }
    if (value == "dry-run") {
        return app::Mode::DryRun;
    }
    if (value == "paper") {
        return app::Mode::Paper;
    }
    if (value == "live") {
        return app::Mode::Live;
    }
    if (value == "show-orders") {
        return app::Mode::ShowOrders;
    }
    errors.push_back("app.mode must be one of validate, dry-run, paper, live, show-orders");
    return app::Mode::DryRun;
}

const JsonObject* require_section(const JsonObject& root, const std::string& name, std::vector<std::string>& errors) {
    const auto* value = find_key(root, name);
    if (value == nullptr) {
        errors.push_back("missing required section: " + name);
        return nullptr;
    }
    const auto* object = as_object(*value);
    if (object == nullptr) {
        errors.push_back("section must be an object: " + name);
        return nullptr;
    }
    return object;
}

std::string require_string(const JsonObject& object, const std::string& path, const std::string& key,
                           std::vector<std::string>& errors) {
    auto value = string_at(object, key);
    if (!value || value->empty()) {
        errors.push_back(path + "." + key + " must be a non-empty string");
        return {};
    }
    return *value;
}

double require_non_negative_number(const JsonObject& object, const std::string& path, const std::string& key,
                                   std::vector<std::string>& errors) {
    auto value = number_at(object, key);
    if (!value || *value < 0.0) {
        errors.push_back(path + "." + key + " must be a non-negative number");
        return 0.0;
    }
    return *value;
}

int require_positive_int(const JsonObject& object, const std::string& path, const std::string& key,
                         std::vector<std::string>& errors) {
    auto value = number_at(object, key);
    if (!value || *value <= 0.0 || static_cast<int>(*value) != *value) {
        errors.push_back(path + "." + key + " must be a positive integer");
        return 0;
    }
    return static_cast<int>(*value);
}

}  // namespace

ConfigLoadResult load_config_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return {.ok = false, .errors = {"unable to open config file: " + path}};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return load_config_from_json(buffer.str());
}

ConfigLoadResult load_config_from_json(const std::string& json_text) {
    ConfigLoadResult result;

    JsonValue root_value{{}};
    try {
        root_value = JsonParser(json_text).parse();
    } catch (const std::exception& ex) {
        result.errors.push_back(std::string("invalid JSON: ") + ex.what());
        return result;
    }

    const auto* root = as_object(root_value);
    if (root == nullptr) {
        result.errors.push_back("config root must be a JSON object");
        return result;
    }

    const auto* app = require_section(*root, "app", result.errors);
    const auto* upstox = require_section(*root, "upstox", result.errors);
    const auto* input = require_section(*root, "input", result.errors);
    const auto* market_data = require_section(*root, "market_data", result.errors);
    const auto* strategies = require_section(*root, "strategies", result.errors);
    const auto* exit_rules = require_section(*root, "exit_rules", result.errors);
    const auto* risk = require_section(*root, "risk", result.errors);
    const auto* rate_limits = require_section(*root, "rate_limits", result.errors);
    const auto* storage = require_section(*root, "storage", result.errors);
    const auto* logging = require_section(*root, "logging", result.errors);

    if (app != nullptr) {
        const auto mode = require_string(*app, "app", "mode", result.errors);
        result.config.app.mode = parse_mode_or_default(mode, result.errors);
        const auto live_enabled = bool_at(*app, "live_trading_enabled");
        if (!live_enabled) {
            result.errors.push_back("app.live_trading_enabled must be a boolean");
        } else {
            result.config.app.live_trading_enabled = *live_enabled;
        }
    }

    if (upstox != nullptr) {
        result.config.upstox.access_token_env = require_string(*upstox, "upstox", "access_token_env", result.errors);
        result.config.upstox.force_ipv4 = bool_at(*upstox, "force_ipv4").value_or(false);
    }
    if (input != nullptr) {
        result.config.input.instruments_csv = require_string(*input, "input", "instruments_csv", result.errors);
    }
    if (market_data != nullptr) {
        const auto intervals = string_array_at(*market_data, "candle_intervals");
        if (!intervals || intervals->empty()) {
            result.errors.push_back("market_data.candle_intervals must be a non-empty string array");
        } else {
            result.config.market_data.candle_intervals = *intervals;
        }
    }
    if (strategies != nullptr) {
        result.config.strategies.buy_signal_mode = require_string(*strategies, "strategies", "buy_signal_mode", result.errors);
        result.config.strategies.sell_signal_mode = require_string(*strategies, "strategies", "sell_signal_mode", result.errors);
        result.config.strategies.min_buy_score =
            require_non_negative_number(*strategies, "strategies", "min_buy_score", result.errors);
    }
    if (exit_rules != nullptr) {
        result.config.exit_rules.default_target_profit_pct =
            require_non_negative_number(*exit_rules, "exit_rules", "default_target_profit_pct", result.errors);
        result.config.exit_rules.default_stop_loss_pct =
            require_non_negative_number(*exit_rules, "exit_rules", "default_stop_loss_pct", result.errors);
        result.config.exit_rules.max_holding_duration_hours =
            require_non_negative_number(*exit_rules, "exit_rules", "max_holding_duration_hours", result.errors);
    }
    if (risk != nullptr) {
        result.config.risk.max_orders_per_day =
            require_positive_int(*risk, "risk", "max_orders_per_day", result.errors);
        result.config.risk.max_daily_traded_value =
            require_non_negative_number(*risk, "risk", "max_daily_traded_value", result.errors);
    }
    if (rate_limits != nullptr) {
        const auto* order_api = require_section(*rate_limits, "order_api", result.errors);
        if (order_api != nullptr) {
            result.config.rate_limits.order_api_safe_requests_per_second =
                require_non_negative_number(*order_api, "rate_limits.order_api", "safe_requests_per_second", result.errors);
        }
    }
    if (storage != nullptr) {
        result.config.storage.sqlite_path = require_string(*storage, "storage", "sqlite_path", result.errors);
    }
    if (logging != nullptr) {
        result.config.logging.log_directory = require_string(*logging, "logging", "log_directory", result.errors);
    }

    result.ok = result.errors.empty();
    return result;
}

}  // namespace tradingbot::infra
