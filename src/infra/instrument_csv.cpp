#include "tradingbot/infra/instrument_csv.hpp"

#include <cctype>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace tradingbot::infra {
namespace {

std::string trim(std::string_view value) {
    auto begin = value.begin();
    auto end = value.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin)) != 0) {
        ++begin;
    }
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
        --end;
    }
    return std::string(begin, end);
}

std::string lower_copy(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const auto ch : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

std::vector<std::string> parse_csv_record(std::string_view line) {
    std::vector<std::string> fields;
    std::string current;
    bool in_quotes = false;

    for (std::size_t index = 0; index < line.size(); ++index) {
        const auto ch = line[index];
        if (ch == '"') {
            if (in_quotes && index + 1 < line.size() && line[index + 1] == '"') {
                current.push_back('"');
                ++index;
            } else {
                in_quotes = !in_quotes;
            }
            continue;
        }
        if (ch == ',' && !in_quotes) {
            fields.push_back(trim(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    fields.push_back(trim(current));
    return fields;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!trim(line).empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::map<std::string, std::size_t> build_header_index(const std::vector<std::string>& headers) {
    std::map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < headers.size(); ++i) {
        index.emplace(lower_copy(trim(headers[i])), i);
    }
    return index;
}

std::optional<std::string> field_at(const std::vector<std::string>& row, const std::map<std::string, std::size_t>& headers,
                                    const std::string& name) {
    const auto it = headers.find(name);
    if (it == headers.end() || it->second >= row.size()) {
        return std::nullopt;
    }
    return trim(row[it->second]);
}

std::optional<bool> parse_bool(const std::string& value) {
    const auto normalized = lower_copy(trim(value));
    if (normalized.empty()) {
        return std::nullopt;
    }
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "y") {
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "n") {
        return false;
    }
    return std::nullopt;
}

core::Quantity parse_quantity(const std::string& value) {
    if (trim(value).empty()) {
        return 0;
    }
    return static_cast<core::Quantity>(std::stoll(value));
}

std::optional<core::Money> parse_optional_money(const std::string& value) {
    if (trim(value).empty()) {
        return std::nullopt;
    }
    return std::stod(value);
}

std::optional<core::Percent> parse_optional_percent(const std::string& value) {
    if (trim(value).empty()) {
        return std::nullopt;
    }
    return std::stod(value);
}

core::Exchange parse_exchange(const std::string& value) {
    const auto normalized = lower_copy(trim(value));
    if (normalized == "nse" || normalized == "nse_eq") {
        return core::Exchange::NseEq;
    }
    if (normalized == "bse" || normalized == "bse_eq") {
        return core::Exchange::BseEq;
    }
    return core::Exchange::Unknown;
}

}  // namespace

InstrumentCsvLoadResult load_instruments_csv_file(const std::string& path) {
    return load_instruments_csv_file(path, {});
}

InstrumentCsvLoadResult load_instruments_csv_file(const std::string& path, const InstrumentCsvLoadOptions& options) {
    std::ifstream file(path);
    if (!file) {
        return {.ok = false, .errors = {"unable to open instrument CSV file: " + path}};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return load_instruments_csv_text(buffer.str(), options);
}

InstrumentCsvLoadResult load_instruments_csv_text(const std::string& csv_text) {
    return load_instruments_csv_text(csv_text, {});
}

InstrumentCsvLoadResult load_instruments_csv_text(const std::string& csv_text, const InstrumentCsvLoadOptions& options) {
    InstrumentCsvLoadResult result;
    const auto lines = split_lines(csv_text);
    if (lines.empty()) {
        result.errors.push_back("instrument CSV is empty");
        return result;
    }

    const auto headers = parse_csv_record(lines.front());
    const auto header_index = build_header_index(headers);
    const std::vector<std::string> required_headers{
        "instrument_key", "symbol", "enabled", "quantity", "max_position_qty", "target_profit_pct"};
    for (const auto& header : required_headers) {
        if (header_index.find(header) == header_index.end()) {
            result.errors.push_back("instrument CSV must contain " + header + " column");
        }
    }
    if (!result.errors.empty()) {
        return result;
    }

    std::set<std::string> seen_keys;
    for (std::size_t line_index = 1; line_index < lines.size(); ++line_index) {
        const auto row = parse_csv_record(lines[line_index]);
        core::Instrument instrument;
        std::vector<std::string> row_errors;
        try {
            instrument.key.value = field_at(row, header_index, "instrument_key").value_or("");
            instrument.symbol = field_at(row, header_index, "symbol").value_or("");
            instrument.exchange = parse_exchange(field_at(row, header_index, "exchange").value_or(""));
            const auto enabled_value = field_at(row, header_index, "enabled").value_or("");
            const auto parsed_enabled = parse_bool(enabled_value);
            if (!parsed_enabled) {
                row_errors.push_back("enabled must be parseable as true or false");
            } else {
                instrument.enabled = *parsed_enabled;
            }
            instrument.quantity = parse_quantity(field_at(row, header_index, "quantity").value_or(""));
            instrument.max_position_quantity = parse_quantity(field_at(row, header_index, "max_position_qty").value_or(""));
            instrument.manual_buy_price = parse_optional_money(field_at(row, header_index, "manual_buy_price").value_or(""));
            instrument.manual_target_price =
                parse_optional_money(field_at(row, header_index, "manual_target_price").value_or(""));
            instrument.stop_loss_pct = parse_optional_percent(field_at(row, header_index, "stop_loss_pct").value_or(""));
            instrument.target_profit_pct =
                parse_optional_percent(field_at(row, header_index, "target_profit_pct").value_or("")).value_or(10.0);
            instrument.trailing_stop_pct =
                parse_optional_percent(field_at(row, header_index, "trailing_stop_pct").value_or(""));
            instrument.strategy_profile = field_at(row, header_index, "strategy_profile").value_or("");
            instrument.notes = field_at(row, header_index, "notes").value_or("");
        } catch (const std::exception& ex) {
            row_errors.push_back(std::string{"could not be parsed: "} + ex.what());
        }

        if (instrument.key.value.empty()) {
            row_errors.push_back("instrument_key is mandatory");
        } else if (!seen_keys.insert(instrument.key.value).second) {
            row_errors.push_back("duplicate instrument_key: " + instrument.key.value);
        }
        if (instrument.quantity <= 0) {
            row_errors.push_back("quantity must be a positive integer");
        }
        if (instrument.max_position_quantity <= 0) {
            row_errors.push_back("max_position_qty must be a positive integer");
        }
        if (instrument.target_profit_pct < 0.0) {
            row_errors.push_back("target_profit_pct must be non-negative");
        }
        if (instrument.stop_loss_pct && *instrument.stop_loss_pct < 0.0) {
            row_errors.push_back("stop_loss_pct must be non-negative");
        }
        if (instrument.manual_buy_price && *instrument.manual_buy_price <= 0.0) {
            row_errors.push_back("manual_buy_price must be positive");
        }
        if (instrument.manual_target_price && *instrument.manual_target_price <= 0.0) {
            row_errors.push_back("manual_target_price must be positive");
        }
        if (!instrument.strategy_profile.empty() && !options.strategy_profiles.empty() &&
            options.strategy_profiles.find(instrument.strategy_profile) == options.strategy_profiles.end()) {
            row_errors.push_back("strategy_profile does not exist: " + instrument.strategy_profile);
        }

        if (!row_errors.empty()) {
            for (const auto& error : row_errors) {
                result.errors.push_back("row " + std::to_string(line_index + 1) + ": " + error);
            }
            if (!options.skip_invalid_rows) {
                continue;
            }
            continue;
        }

        result.instruments.push_back(std::move(instrument));
    }

    result.ok = options.skip_invalid_rows ? !result.instruments.empty() : result.errors.empty();
    return result;
}

}  // namespace tradingbot::infra
