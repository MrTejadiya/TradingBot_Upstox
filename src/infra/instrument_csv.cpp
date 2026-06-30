#include "tradingbot/infra/instrument_csv.hpp"

#include <cctype>
#include <fstream>
#include <map>
#include <optional>
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

bool parse_bool(const std::string& value, bool default_value) {
    const auto normalized = lower_copy(trim(value));
    if (normalized.empty()) {
        return default_value;
    }
    return normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "y";
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
    std::ifstream file(path);
    if (!file) {
        return {.ok = false, .errors = {"unable to open instrument CSV file: " + path}};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return load_instruments_csv_text(buffer.str());
}

InstrumentCsvLoadResult load_instruments_csv_text(const std::string& csv_text) {
    InstrumentCsvLoadResult result;
    const auto lines = split_lines(csv_text);
    if (lines.empty()) {
        result.errors.push_back("instrument CSV is empty");
        return result;
    }

    const auto headers = parse_csv_record(lines.front());
    const auto header_index = build_header_index(headers);
    if (header_index.find("instrument_key") == header_index.end()) {
        result.errors.push_back("instrument CSV must contain instrument_key column");
        return result;
    }

    for (std::size_t line_index = 1; line_index < lines.size(); ++line_index) {
        const auto row = parse_csv_record(lines[line_index]);
        core::Instrument instrument;
        try {
            instrument.key.value = field_at(row, header_index, "instrument_key").value_or("");
            instrument.symbol = field_at(row, header_index, "symbol").value_or("");
            instrument.exchange = parse_exchange(field_at(row, header_index, "exchange").value_or(""));
            instrument.enabled = parse_bool(field_at(row, header_index, "enabled").value_or("true"), true);
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
            result.errors.push_back("row " + std::to_string(line_index + 1) + " could not be parsed: " + ex.what());
            continue;
        }

        result.instruments.push_back(std::move(instrument));
    }

    result.ok = result.errors.empty();
    return result;
}

}  // namespace tradingbot::infra
