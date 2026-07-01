#include "tradingbot/infra/upstox_api_client.hpp"

#include "tradingbot/infra/credentials.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace tradingbot::infra {
namespace {

std::string request_metadata(const HttpRequest& request) {
    std::ostringstream out;
    out << "method=" << request.method << " url=" << request.url;
    const auto auth = request.headers.find("Authorization");
    if (auth != request.headers.end()) {
        out << " Authorization=" << redact_authorization_header(auth->second);
    }
    return out.str();
}

}  // namespace

UpstoxApiClient::UpstoxApiClient(std::string base_url, std::string access_token,
                                 std::shared_ptr<HttpTransport> transport, RetryPolicy retry_policy,
                                 UpstoxApiClientOptions options)
    : base_url_(std::move(base_url)),
      access_token_(std::move(access_token)),
      transport_(std::move(transport)),
      retry_policy_(retry_policy),
      options_(options) {}

ApiResult UpstoxApiClient::get(const std::string& path) {
    return request("GET", path, {});
}

ApiResult UpstoxApiClient::post(const std::string& path, const std::string& body) {
    return request("POST", path, body);
}

ApiResult UpstoxApiClient::request(std::string method, const std::string& path, std::string body) {
    ApiResult result;
    if (!transport_) {
        result.error = "HTTP transport is required";
        return result;
    }

    HttpRequest http_request{
        .method = std::move(method),
        .url = join_url(base_url_, path),
        .headers = {{"Accept", "application/json"}, {"Authorization", "Bearer " + access_token_}},
        .body = std::move(body),
        .force_ipv4 = options_.force_ipv4,
    };
    if (!http_request.body.empty()) {
        http_request.headers.emplace("Content-Type", "application/json");
    }

    const auto attempts = retry_policy_.max_attempts <= 0 ? 1 : retry_policy_.max_attempts;
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        result.event.method = http_request.method;
        result.event.url = http_request.url;
        result.event.attempt_count = attempt;
        result.event.redacted_request_metadata = request_metadata(http_request);

        try {
            result.response = transport_->send(http_request);
        } catch (const std::exception& ex) {
            result.error = ex.what();
            result.event.status_code = 0;
            if (attempt < attempts) {
                result.event.retried = true;
                continue;
            }
            return result;
        }

        result.event.status_code = result.response.status_code;
        if (result.response.status_code >= 200 && result.response.status_code < 300) {
            result.ok = true;
            return result;
        }

        if (!is_retryable_http_status(result.response.status_code) || attempt == attempts) {
            result.error = "HTTP request failed with status " + std::to_string(result.response.status_code);
            return result;
        }
        result.event.retried = true;
    }

    result.error = "HTTP request failed";
    return result;
}

bool is_transient_http_status(int status_code) {
    return status_code == 408 || status_code == 429 || (status_code >= 500 && status_code <= 599);
}

bool is_retryable_http_status(int status_code) {
    return is_transient_http_status(status_code);
}

std::string join_url(const std::string& base_url, const std::string& path) {
    if (base_url.empty()) {
        return path;
    }
    if (path.empty()) {
        return base_url;
    }
    const auto base_has_slash = base_url.back() == '/';
    const auto path_has_slash = path.front() == '/';
    if (base_has_slash && path_has_slash) {
        return base_url + path.substr(1);
    }
    if (!base_has_slash && !path_has_slash) {
        return base_url + "/" + path;
    }
    return base_url + path;
}

}  // namespace tradingbot::infra
