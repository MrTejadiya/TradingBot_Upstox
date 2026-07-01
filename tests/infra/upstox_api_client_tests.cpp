#include "tradingbot/infra/upstox_api_client.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

class FakeTransport final : public tradingbot::infra::HttpTransport {
public:
    std::vector<tradingbot::infra::HttpResponse> responses;
    std::vector<tradingbot::infra::HttpRequest> requests;

    tradingbot::infra::HttpResponse send(const tradingbot::infra::HttpRequest& request) override {
        requests.push_back(request);
        if (responses.empty()) {
            return {.status_code = 200, .body = "{}"};
        }
        auto response = responses.front();
        responses.erase(responses.begin());
        return response;
    }
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void applies_bearer_auth_and_joins_url() {
    auto transport = std::make_shared<FakeTransport>();
    tradingbot::infra::UpstoxApiClient client("https://api.upstox.com/v2", "secret-token", transport);

    const auto result = client.get("/user/profile");

    require(result.ok, "GET should succeed");
    require(transport->requests.size() == 1, "one request should be sent");
    require(transport->requests.front().url == "https://api.upstox.com/v2/user/profile", "URL should join cleanly");
    require(transport->requests.front().headers.at("Authorization") == "Bearer secret-token", "auth header should be set");
}

void redacts_auth_metadata() {
    auto transport = std::make_shared<FakeTransport>();
    tradingbot::infra::UpstoxApiClient client("https://api.upstox.com/v2", "secret-token", transport);

    const auto result = client.get("orders");

    require(result.event.redacted_request_metadata.find("secret-token") == std::string::npos,
            "event metadata must not include token");
    require(result.event.redacted_request_metadata.find("<redacted>") != std::string::npos,
            "event metadata should include redaction marker");
}

void retries_transient_status() {
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 500, .body = "temporary"}, {.status_code = 200, .body = "{}"}};
    tradingbot::infra::UpstoxApiClient client("https://api.upstox.com/v2", "token", transport, {.max_attempts = 2});

    const auto result = client.get("orders");

    require(result.ok, "transient failure should retry and succeed");
    require(transport->requests.size() == 2, "two attempts should be sent");
    require(result.event.retried, "event should record retry");
    require(result.event.attempt_count == 2, "event should record final attempt count");
}

void does_not_retry_validation_or_auth_errors() {
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 401, .body = "unauthorized"}, {.status_code = 200, .body = "{}"}};
    tradingbot::infra::UpstoxApiClient client("https://api.upstox.com/v2", "token", transport, {.max_attempts = 3});

    const auto result = client.get("orders");

    require(!result.ok, "401 should fail");
    require(transport->requests.size() == 1, "401 should not be retried");
    require(result.error.find("401") != std::string::npos, "error should include status");
}

void post_sets_json_content_type() {
    auto transport = std::make_shared<FakeTransport>();
    tradingbot::infra::UpstoxApiClient client("https://api.upstox.com/v2", "token", transport);

    const auto result = client.post("orders", "{\"quantity\":1}");

    require(result.ok, "POST should succeed");
    require(transport->requests.front().method == "POST", "method should be POST");
    require(transport->requests.front().headers.at("Content-Type") == "application/json", "content type should be JSON");
}

void propagates_force_ipv4_option_to_transport_request() {
    auto transport = std::make_shared<FakeTransport>();
    tradingbot::infra::UpstoxApiClient client(
        "https://api-hft.upstox.com", "token", transport, {}, {.force_ipv4 = true});

    const auto result = client.post("/v3/order/place", "{\"quantity\":1}");

    require(result.ok, "POST should succeed");
    require(transport->requests.size() == 1, "one request should be sent");
    require(transport->requests.front().force_ipv4, "request should carry IPv4 egress requirement");
}

}  // namespace

int main() {
    applies_bearer_auth_and_joins_url();
    redacts_auth_metadata();
    retries_transient_status();
    does_not_retry_validation_or_auth_errors();
    post_sets_json_content_type();
    propagates_force_ipv4_option_to_transport_request();
    return 0;
}
