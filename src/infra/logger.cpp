#include "tradingbot/infra/logger.hpp"

#include "tradingbot/infra/credentials.hpp"

#include <fstream>
#include <ostream>
#include <sstream>
#include <string_view>

namespace tradingbot::infra {
namespace {

std::string escape_value(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const auto ch : value) {
        if (ch == '\\' || ch == '"') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

int level_rank(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return 0;
        case LogLevel::Info:
            return 1;
        case LogLevel::Warn:
            return 2;
        case LogLevel::Error:
            return 3;
    }
    return 1;
}

std::string maybe_redact(const std::string& value, const LoggerConfig& config) {
    return config.redact_secrets ? redact_secret(value) : value;
}

std::string lower_copy(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const auto ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            out.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

bool is_sensitive_field_name(std::string_view key) {
    const auto lower_key = lower_copy(key);
    return lower_key.find("token") != std::string::npos || lower_key.find("secret") != std::string::npos ||
           lower_key.find("authorization") != std::string::npos || lower_key.find("api_key") != std::string::npos ||
           lower_key.find("apikey") != std::string::npos;
}

std::string maybe_redact_field(const std::string& key, const std::string& value, const LoggerConfig& config) {
    if (!config.redact_secrets) {
        return value;
    }
    if (is_sensitive_field_name(key)) {
        return "<redacted>";
    }
    return redact_secret(value);
}

}  // namespace

std::string to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "debug";
        case LogLevel::Info:
            return "info";
        case LogLevel::Warn:
            return "warn";
        case LogLevel::Error:
            return "error";
    }
    return "info";
}

bool should_log(LogLevel level, LogLevel minimum_level) {
    return level_rank(level) >= level_rank(minimum_level);
}

std::string format_log_record(const LogRecord& record, const LoggerConfig& config) {
    std::ostringstream out;
    out << "{\"level\":\"" << to_string(record.level) << "\",\"event\":\"" << escape_value(record.event)
        << "\",\"message\":\"" << escape_value(maybe_redact(record.message, config)) << "\"";
    for (const auto& [key, value] : record.fields) {
        out << ",\"" << escape_value(key) << "\":\"" << escape_value(maybe_redact_field(key, value, config)) << "\"";
    }
    out << "}";
    return out.str();
}

StreamLogger::StreamLogger(std::ostream& output, LoggerConfig config) : output_(output), config_(config) {}

void StreamLogger::log(const LogRecord& record) {
    if (!should_log(record.level, config_.minimum_level)) {
        return;
    }
    output_ << format_log_record(record, config_) << '\n';
}

void StreamLogger::startup_summary(const std::string& mode, const std::string& config_hash) {
    log({.level = LogLevel::Info,
         .event = "startup_summary",
         .message = "startup configuration summary",
         .fields = {{"mode", mode}, {"config_hash", config_hash}}});
}

void StreamLogger::csv_validation_summary(int loaded_rows, int error_count) {
    log({.level = error_count == 0 ? LogLevel::Info : LogLevel::Warn,
         .event = "csv_validation_summary",
         .message = "CSV validation complete",
         .fields = {{"loaded_rows", std::to_string(loaded_rows)}, {"error_count", std::to_string(error_count)}}});
}

void StreamLogger::risk_decision(const std::string& instrument_key, bool approved, const std::string& reason_code) {
    log({.level = approved ? LogLevel::Info : LogLevel::Warn,
         .event = "risk_decision",
         .message = approved ? "risk approved" : "risk rejected",
         .fields = {{"instrument_key", instrument_key}, {"approved", approved ? "true" : "false"}, {"reason_code", reason_code}}});
}

void StreamLogger::order_decision(const std::string& instrument_key, const std::string& side, const std::string& mode) {
    log({.level = LogLevel::Info,
         .event = "order_decision",
         .message = "order decision recorded",
         .fields = {{"instrument_key", instrument_key}, {"side", side}, {"mode", mode}}});
}

std::string daily_log_file_name(const std::string& directory, const std::string& yyyymmdd) {
    if (directory.empty()) {
        return "tradingbot-" + yyyymmdd + ".log";
    }
    const auto separator = directory.back() == '/' || directory.back() == '\\' ? "" : "/";
    return directory + separator + "tradingbot-" + yyyymmdd + ".log";
}

bool append_log_line(const std::string& path, const std::string& line) {
    std::ofstream file(path, std::ios::app);
    if (!file) {
        return false;
    }
    file << line << '\n';
    return static_cast<bool>(file);
}

}  // namespace tradingbot::infra
