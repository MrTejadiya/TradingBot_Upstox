#pragma once

#include <map>
#include <memory>
#include <string>

namespace tradingbot::infra {

struct HttpRequest {
    std::string method;
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code{0};
    std::string body;
    std::map<std::string, std::string> headers;
};

class HttpTransport {
public:
    virtual ~HttpTransport() = default;
    virtual HttpResponse send(const HttpRequest& request) = 0;
};

struct RetryPolicy {
    int max_attempts{3};
};

struct ApiEvent {
    std::string method;
    std::string url;
    int status_code{0};
    int attempt_count{0};
    bool retried{false};
    std::string redacted_request_metadata;
};

struct ApiResult {
    bool ok{false};
    HttpResponse response;
    ApiEvent event;
    std::string error;
};

class UpstoxApiClient {
public:
    UpstoxApiClient(std::string base_url, std::string access_token, std::shared_ptr<HttpTransport> transport,
                    RetryPolicy retry_policy = {});

    ApiResult get(const std::string& path);
    ApiResult post(const std::string& path, const std::string& body);

private:
    ApiResult request(std::string method, const std::string& path, std::string body);

    std::string base_url_;
    std::string access_token_;
    std::shared_ptr<HttpTransport> transport_;
    RetryPolicy retry_policy_;
};

bool is_transient_http_status(int status_code);
bool is_retryable_http_status(int status_code);
std::string join_url(const std::string& base_url, const std::string& path);

}  // namespace tradingbot::infra

