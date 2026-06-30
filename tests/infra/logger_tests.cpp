#include "tradingbot/infra/logger.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void formats_structured_log_record() {
    const auto line = tradingbot::infra::format_log_record(
        {.level = tradingbot::infra::LogLevel::Info,
         .event = "startup",
         .message = "ready",
         .fields = {{"mode", "dry-run"}}},
        {});

    require(line.find("\"level\":\"info\"") != std::string::npos, "level should be present");
    require(line.find("\"event\":\"startup\"") != std::string::npos, "event should be present");
    require(line.find("\"mode\":\"dry-run\"") != std::string::npos, "field should be present");
}

void redacts_secrets_in_message_and_fields() {
    const auto line = tradingbot::infra::format_log_record(
        {.level = tradingbot::infra::LogLevel::Info,
         .event = "api_event",
         .message = "Authorization: Bearer secret-token",
         .fields = {{"access_token", "secret-token"}, {"normal", "value"}}},
        {});

    require(line.find("secret-token") == std::string::npos, "secret token should be redacted");
    require(line.find("<redacted>") != std::string::npos, "redaction marker should be present");
    require(line.find("\"normal\":\"value\"") != std::string::npos, "normal fields should remain");
}

void filters_below_minimum_level() {
    std::ostringstream output;
    tradingbot::infra::StreamLogger logger(output, {.minimum_level = tradingbot::infra::LogLevel::Warn});

    logger.log({.level = tradingbot::infra::LogLevel::Info, .event = "info", .message = "hidden"});
    logger.log({.level = tradingbot::infra::LogLevel::Error, .event = "error", .message = "visible"});

    require(output.str().find("hidden") == std::string::npos, "info should be filtered");
    require(output.str().find("visible") != std::string::npos, "error should be logged");
}

void emits_expected_event_helpers() {
    std::ostringstream output;
    tradingbot::infra::StreamLogger logger(output, {});

    logger.startup_summary("dry-run", "abc123");
    logger.csv_validation_summary(2, 1);
    logger.risk_decision("NSE_EQ|INE002A01018", false, "DUPLICATE_OPEN_ORDER");
    logger.order_decision("NSE_EQ|INE002A01018", "buy", "dry-run");

    const auto text = output.str();
    require(text.find("startup_summary") != std::string::npos, "startup helper should log");
    require(text.find("csv_validation_summary") != std::string::npos, "CSV helper should log");
    require(text.find("risk_decision") != std::string::npos, "risk helper should log");
    require(text.find("order_decision") != std::string::npos, "order helper should log");
}

void creates_daily_log_file_name() {
    const auto name = tradingbot::infra::daily_log_file_name("logs", "20260630");

    require(name.find("logs") != std::string::npos, "directory should be included");
    require(name.find("tradingbot-20260630.log") != std::string::npos, "date should be included");
}

void appends_log_line_to_file() {
    const auto path = std::string{"logger_test_tmp.log"};
    const auto ok = tradingbot::infra::append_log_line(path, "hello");
    std::string line;
    {
        std::ifstream file(path);
        std::getline(file, line);
    }
    std::remove(path.c_str());

    require(ok, "append should succeed");
    require(line == "hello", "line should be written");
}

}  // namespace

int main() {
    formats_structured_log_record();
    redacts_secrets_in_message_and_fields();
    filters_below_minimum_level();
    emits_expected_event_helpers();
    creates_daily_log_file_name();
    appends_log_line_to_file();
    return 0;
}
