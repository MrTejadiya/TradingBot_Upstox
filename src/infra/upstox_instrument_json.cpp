#include "tradingbot/infra/upstox_instrument_json.hpp"

#include "tradingbot/infra/instrument_universe.hpp"

#include <cctype>
#include <charconv>
#include <fstream>
#include <map>
#include <optional>
#include <set>
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

bool is_supported_equity_record(const JsonObject& object) {
    const auto segment = string_at(object, "segment").value_or("");
    const auto instrument_type = string_at(object, "instrument_type").value_or("");
    return (segment == "NSE_EQ" || segment == "BSE_EQ") && instrument_type == "EQ";
}

core::Exchange parse_exchange(const std::string& segment) {
    if (segment == "NSE_EQ") {
        return core::Exchange::NseEq;
    }
    if (segment == "BSE_EQ") {
        return core::Exchange::BseEq;
    }
    return core::Exchange::Unknown;
}

std::string display_symbol(const JsonObject& object) {
    auto symbol = string_at(object, "trading_symbol").value_or("");
    if (!symbol.empty()) {
        return symbol;
    }
    symbol = string_at(object, "short_name").value_or("");
    if (!symbol.empty()) {
        return symbol;
    }
    return string_at(object, "name").value_or("");
}

std::optional<core::Instrument> to_instrument(const JsonObject& object,
                                              const UpstoxInstrumentJsonLoadOptions& options,
                                              std::string& error) {
    if (!is_supported_equity_record(object)) {
        return std::nullopt;
    }

    core::Instrument instrument;
    instrument.key.value = string_at(object, "instrument_key").value_or("");
    instrument.symbol = display_symbol(object);
    instrument.exchange = parse_exchange(string_at(object, "segment").value_or(""));
    instrument.enabled = options.enabled;
    instrument.quantity = options.quantity;
    instrument.max_position_quantity = options.max_position_quantity;
    instrument.target_profit_pct = options.target_profit_pct;
    instrument.strategy_profile = options.strategy_profile;
    instrument.notes = options.notes;

    if (instrument.key.value.empty()) {
        error = "instrument_key is mandatory";
        return std::nullopt;
    }
    if (instrument.quantity <= 0) {
        error = "default quantity must be positive";
        return std::nullopt;
    }
    if (instrument.max_position_quantity <= 0) {
        error = "default max_position_quantity must be positive";
        return std::nullopt;
    }
    if (instrument.target_profit_pct < 0.0) {
        error = "default target_profit_pct must be non-negative";
        return std::nullopt;
    }
    if (instrument.exchange == core::Exchange::Unknown) {
        error = "unsupported equity segment";
        return std::nullopt;
    }

    return instrument;
}

}  // namespace

UpstoxInstrumentJsonLoadResult load_upstox_instruments_json_file(const std::string& path) {
    return load_upstox_instruments_json_file(path, {});
}

UpstoxInstrumentJsonLoadResult load_upstox_instruments_json_file(
    const std::string& path, const UpstoxInstrumentJsonLoadOptions& options) {
    std::ifstream file(path);
    if (!file) {
        return {.ok = false, .errors = {"unable to open Upstox instrument JSON file: " + path}};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return load_upstox_instruments_json_text(buffer.str(), options);
}

UpstoxInstrumentJsonLoadResult load_upstox_instruments_json_text(const std::string& json_text) {
    return load_upstox_instruments_json_text(json_text, {});
}

UpstoxInstrumentJsonLoadResult load_upstox_instruments_json_text(
    const std::string& json_text, const UpstoxInstrumentJsonLoadOptions& options) {
    UpstoxInstrumentJsonLoadResult result;

    JsonValue root_value{{}};
    try {
        root_value = JsonParser(json_text).parse();
    } catch (const std::exception& ex) {
        result.errors.push_back(std::string{"invalid Upstox instrument JSON: "} + ex.what());
        return result;
    }

    const auto* root_array = std::get_if<JsonArray>(&root_value.value);
    if (root_array == nullptr) {
        result.errors.push_back("Upstox instrument JSON root must be an array");
        return result;
    }

    std::set<std::string> seen_keys;
    for (std::size_t index = 0; index < root_array->size(); ++index) {
        const auto* object = std::get_if<JsonObject>(&(*root_array)[index].value);
        if (object == nullptr) {
            result.errors.push_back("record " + std::to_string(index + 1) + ": expected instrument object");
            continue;
        }

        std::string row_error;
        auto instrument = to_instrument(*object, options, row_error);
        if (!instrument) {
            if (row_error.empty()) {
                ++result.skipped_records;
            } else {
                result.errors.push_back("record " + std::to_string(index + 1) + ": " + row_error);
            }
            continue;
        }

        if (!seen_keys.insert(instrument->key.value).second) {
            result.errors.push_back("record " + std::to_string(index + 1) +
                                    ": duplicate instrument_key: " + instrument->key.value);
            continue;
        }

        result.instruments.push_back(std::move(*instrument));
    }

    result.instruments = prefer_nse_duplicate_listings(std::move(result.instruments));
    result.ok = result.errors.empty() && !result.instruments.empty();
    if (result.instruments.empty() && result.errors.empty()) {
        result.errors.push_back("Upstox instrument JSON did not contain supported NSE_EQ or BSE_EQ equity records");
    }
    return result;
}

}  // namespace tradingbot::infra
