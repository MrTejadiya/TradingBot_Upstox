#pragma once

#include <iosfwd>
#include <map>
#include <string>

namespace tradingbot::infra {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
};

struct LogRecord {
    LogLevel level{LogLevel::Info};
    std::string event;
    std::string message;
    std::map<std::string, std::string> fields;
};

struct LoggerConfig {
    LogLevel minimum_level{LogLevel::Info};
    bool redact_secrets{true};
};

std::string to_string(LogLevel level);
std::string format_log_record(const LogRecord& record, const LoggerConfig& config);
bool should_log(LogLevel level, LogLevel minimum_level);

class StreamLogger {
public:
    StreamLogger(std::ostream& output, LoggerConfig config);

    void log(const LogRecord& record);
    void startup_summary(const std::string& mode, const std::string& config_hash);
    void csv_validation_summary(int loaded_rows, int error_count);
    void risk_decision(const std::string& instrument_key, bool approved, const std::string& reason_code);
    void order_decision(const std::string& instrument_key, const std::string& side, const std::string& mode);

private:
    std::ostream& output_;
    LoggerConfig config_;
};

std::string daily_log_file_name(const std::string& directory, const std::string& yyyymmdd);
bool append_log_line(const std::string& path, const std::string& line);

}  // namespace tradingbot::infra

