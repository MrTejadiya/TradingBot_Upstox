#include "tradingbot/infra/credentials.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void loads_token_from_configured_environment_name() {
    const std::map<std::string, std::string> env{{"UPSTOX_ACCESS_TOKEN", "secret-token-value"}};
    const auto result = tradingbot::infra::load_access_token_from_env(
        "UPSTOX_ACCESS_TOKEN", [&env](const std::string& name) {
            const auto it = env.find(name);
            return it == env.end() ? std::string{} : it->second;
        });

    require(result.ok, "token should load");
    require(result.token == "secret-token-value", "token value should be returned to caller");
    require(result.error.empty(), "successful load should not have error");
}

void fails_closed_when_env_name_is_missing() {
    const auto result = tradingbot::infra::load_access_token_from_env("", [](const std::string&) {
        return std::string{"secret-token-value"};
    });

    require(!result.ok, "empty environment variable name should fail");
    require(result.token.empty(), "failed load should not return token");
    require(result.error.find("environment variable name is required") != std::string::npos,
            "error should explain missing env name");
}

void fails_closed_when_token_is_missing() {
    const auto result = tradingbot::infra::load_access_token_from_env("UPSTOX_ACCESS_TOKEN", [](const std::string&) {
        return std::string{};
    });

    require(!result.ok, "missing token should fail");
    require(result.token.empty(), "missing token should not return token");
    require(result.error.find("UPSTOX_ACCESS_TOKEN") != std::string::npos, "error may name source but not token value");
}

void redacts_sensitive_key_value_pairs() {
    const auto redacted = tradingbot::infra::redact_secret(
        "access_token=secret-token api_key=another-secret normal=value");

    require(redacted.find("secret-token") == std::string::npos, "access token should be redacted");
    require(redacted.find("another-secret") == std::string::npos, "api key should be redacted");
    require(redacted.find("normal=value") != std::string::npos, "non-sensitive value should remain");
}

void redacts_bearer_tokens() {
    const auto redacted = tradingbot::infra::redact_secret("Authorization: Bearer secret-token-value");

    require(redacted.find("secret-token-value") == std::string::npos, "bearer token should be redacted");
    require(redacted.find("Bearer <redacted>") != std::string::npos, "bearer scheme should remain useful");
}

void authorization_header_is_always_redacted() {
    const auto redacted = tradingbot::infra::redact_authorization_header("Bearer secret-token-value");

    require(redacted == "<redacted>", "authorization header should be fully redacted");
}

void credential_source_description_does_not_include_secret() {
    const auto description = tradingbot::infra::describe_credential_source("UPSTOX_ACCESS_TOKEN");

    require(description.find("UPSTOX_ACCESS_TOKEN") != std::string::npos, "description should name env source");
    require(description.find("secret") == std::string::npos, "description should not imply secret value");
}

}  // namespace

int main() {
    loads_token_from_configured_environment_name();
    fails_closed_when_env_name_is_missing();
    fails_closed_when_token_is_missing();
    redacts_sensitive_key_value_pairs();
    redacts_bearer_tokens();
    authorization_header_is_always_redacted();
    credential_source_description_does_not_include_secret();
    return 0;
}

