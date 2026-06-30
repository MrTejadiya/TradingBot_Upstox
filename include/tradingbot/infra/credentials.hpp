#pragma once

#include <functional>
#include <string>

namespace tradingbot::infra {

struct CredentialLoadResult {
    bool ok{false};
    std::string token;
    std::string error;
};

using EnvironmentLookup = std::function<std::string(const std::string&)>;

CredentialLoadResult load_access_token_from_env(const std::string& env_name);
CredentialLoadResult load_access_token_from_env(const std::string& env_name, const EnvironmentLookup& lookup);

std::string redact_secret(std::string text);
std::string redact_authorization_header(const std::string& header_value);
std::string describe_credential_source(const std::string& env_name);

}  // namespace tradingbot::infra

