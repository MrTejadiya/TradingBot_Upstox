#include "tradingbot/infra/credentials.hpp"

#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>

namespace tradingbot::infra {
namespace {

constexpr std::string_view kRedacted{"<redacted>"};

bool looks_sensitive_key(std::string_view key) {
    return key.find("token") != std::string_view::npos || key.find("secret") != std::string_view::npos ||
           key.find("api_key") != std::string_view::npos || key.find("apikey") != std::string_view::npos;
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

std::size_t find_case_insensitive(std::string_view text, std::string_view needle) {
    return lower_copy(text).find(lower_copy(needle));
}

}  // namespace

CredentialLoadResult load_access_token_from_env(const std::string& env_name) {
    return load_access_token_from_env(env_name, [](const std::string& name) {
#ifdef _WIN32
        char* value = nullptr;
        std::size_t length = 0;
        if (_dupenv_s(&value, &length, name.c_str()) != 0 || value == nullptr) {
            return std::string{};
        }
        std::unique_ptr<char, decltype(&std::free)> owned_value(value, &std::free);
        return std::string{owned_value.get(), length > 0 ? length - 1 : 0};
#else
        const auto* value = std::getenv(name.c_str());
        return value == nullptr ? std::string{} : std::string{value};
#endif
    });
}

CredentialLoadResult load_access_token_from_env(const std::string& env_name, const EnvironmentLookup& lookup) {
    if (env_name.empty()) {
        return {.ok = false, .error = "access token environment variable name is required"};
    }

    const auto token = lookup(env_name);
    if (token.empty()) {
        return {.ok = false, .error = "missing access token in environment variable: " + env_name};
    }

    return {.ok = true, .token = token};
}

std::string redact_secret(std::string text) {
    const auto bearer_pos = find_case_insensitive(text, "bearer ");
    if (bearer_pos != std::string::npos) {
        const auto value_start = bearer_pos + std::string_view{"bearer "}.size();
        auto value_end = value_start;
        while (value_end < text.size() && text[value_end] != ' ' && text[value_end] != ',' && text[value_end] != ';') {
            ++value_end;
        }
        text.replace(value_start, value_end - value_start, std::string{kRedacted});
    }

    std::size_t search_from = 0;
    while (search_from < text.size()) {
        const auto lower_text = lower_copy(text);
        auto separator = lower_text.find('=', search_from);
        auto colon = lower_text.find(':', search_from);
        if (colon != std::string::npos && (separator == std::string::npos || colon < separator)) {
            separator = colon;
        }
        if (separator == std::string::npos) {
            break;
        }

        auto key_start = text.rfind(' ', separator);
        key_start = key_start == std::string::npos ? 0 : key_start + 1;
        const auto key = lower_text.substr(key_start, separator - key_start);
        if (!looks_sensitive_key(key)) {
            search_from = separator + 1;
            continue;
        }

        auto value_start = separator + 1;
        while (value_start < text.size() && text[value_start] == ' ') {
            ++value_start;
        }
        auto value_end = value_start;
        while (value_end < text.size() && text[value_end] != ' ' && text[value_end] != ',' && text[value_end] != ';') {
            ++value_end;
        }

        if (text.substr(value_start, value_end - value_start).find(kRedacted) != std::string::npos) {
            search_from = value_end;
            continue;
        }

        text.replace(value_start, value_end - value_start, std::string{kRedacted});
        search_from = value_start + kRedacted.size();
    }

    return text;
}

std::string redact_authorization_header(const std::string& header_value) {
    if (header_value.empty()) {
        return {};
    }
    return std::string{kRedacted};
}

std::string describe_credential_source(const std::string& env_name) {
    return env_name.empty() ? "environment variable: <missing>" : "environment variable: " + env_name;
}

}  // namespace tradingbot::infra
