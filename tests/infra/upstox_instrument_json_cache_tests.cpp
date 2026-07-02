#include "tradingbot/infra/upstox_instrument_json_cache.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class FakeTransport final : public tradingbot::infra::HttpTransport {
public:
    std::vector<tradingbot::infra::HttpResponse> responses;
    std::vector<tradingbot::infra::HttpRequest> requests;
    bool throw_on_send{false};

    tradingbot::infra::HttpResponse send(const tradingbot::infra::HttpRequest& request) override {
        requests.push_back(request);
        if (throw_on_send) {
            throw std::runtime_error("network down");
        }
        if (responses.empty()) {
            return {.status_code = 200, .body = "[]"};
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

std::filesystem::path test_path(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove(path);
    std::filesystem::remove(path.string() + ".tmp");
    return path;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary);
    file << text;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return text;
}

void reads_existing_cache_when_refresh_is_disabled() {
    const auto path = test_path("tradingbot_upstox_cache_existing.json");
    write_text(path, "[{\"instrument_key\":\"NSE_EQ|CACHE\"}]");
    auto transport = std::make_shared<FakeTransport>();
    tradingbot::infra::UpstoxInstrumentJsonCache cache(transport);

    const auto result = cache.load({
        .url = "https://assets.upstox.com/complete.json.gz",
        .cache_path = path.string(),
        .refresh = false,
    });

    require(result.ok, "existing cache should load");
    require(result.from_cache, "result should report cache source");
    require(!result.downloaded, "cache hit should not download");
    require(result.json_text.find("CACHE") != std::string::npos, "cache text should be returned");
    require(transport->requests.empty(), "transport should not be called on cache hit without refresh");
    std::filesystem::remove(path);
}

void downloads_and_writes_cache_when_refresh_is_enabled() {
    const auto path = test_path("tradingbot_upstox_cache_download.json");
    const auto metadata_path = std::filesystem::path{path.string() + ".metadata.json"};
    std::filesystem::remove(metadata_path);
    const std::string body = "[{\"instrument_key\":\"NSE_EQ|LIVE\"}]";
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 200, .body = body}};
    tradingbot::infra::UpstoxInstrumentJsonCache cache(transport);

    const auto result = cache.load({
        .url = "https://assets.upstox.com/complete.json.gz",
        .cache_path = path.string(),
        .refresh = true,
        .force_ipv4 = true,
    });

    require(result.ok, "download should succeed");
    require(result.downloaded, "result should report download");
    require(!result.from_cache, "download should not report cache source");
    require(result.json_text.find("LIVE") != std::string::npos, "download text should be returned");
    require(read_text(path).find("LIVE") != std::string::npos, "download should be written to cache");
    require(result.metadata_path == metadata_path.string(), "metadata path should be reported");
    const auto metadata = read_text(metadata_path);
    require(metadata.find("\"source_url\": \"https://assets.upstox.com/complete.json.gz\"") != std::string::npos,
            "metadata should include source URL");
    require(metadata.find(path.filename().string()) != std::string::npos, "metadata should include cache path");
    require(metadata.find("\"status_code\": 200") != std::string::npos, "metadata should include status code");
    require(metadata.find("\"bytes\": " + std::to_string(body.size())) != std::string::npos,
            "metadata should include byte count");
    require(metadata.find("\"refreshed_at_utc\": \"") != std::string::npos, "metadata should include refresh time");
    require(transport->requests.size() == 1, "one request should be sent");
    require(transport->requests.front().method == "GET", "request should use GET");
    require(transport->requests.front().url == "https://assets.upstox.com/complete.json.gz", "URL should pass through");
    require(transport->requests.front().force_ipv4, "force IPv4 should pass to transport");
    std::filesystem::remove(metadata_path);
    std::filesystem::remove(path);
}

void cache_hit_reports_metadata_path_without_rewriting_metadata() {
    const auto path = test_path("tradingbot_upstox_cache_metadata_hit.json");
    const auto metadata_path = std::filesystem::path{path.string() + ".metadata.json"};
    write_text(path, "[{\"instrument_key\":\"NSE_EQ|CACHE\"}]");
    write_text(metadata_path, "existing metadata");
    auto transport = std::make_shared<FakeTransport>();
    tradingbot::infra::UpstoxInstrumentJsonCache cache(transport);

    const auto result = cache.load({
        .url = "https://assets.upstox.com/complete.json.gz",
        .cache_path = path.string(),
        .refresh = false,
    });

    require(result.ok, "cache hit should succeed");
    require(result.from_cache, "cache hit should report cache");
    require(result.metadata_path == metadata_path.string(), "cache hit should report metadata path");
    require(read_text(metadata_path) == "existing metadata", "cache hit should not rewrite metadata");
    require(transport->requests.empty(), "cache hit should not call transport");
    std::filesystem::remove(metadata_path);
    std::filesystem::remove(path);
}

void falls_back_to_cache_when_download_fails() {
    const auto path = test_path("tradingbot_upstox_cache_fallback.json");
    write_text(path, "[{\"instrument_key\":\"NSE_EQ|STALE\"}]");
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 500, .body = "temporary"}};
    tradingbot::infra::UpstoxInstrumentJsonCache cache(transport);

    const auto result = cache.load({
        .url = "https://assets.upstox.com/complete.json.gz",
        .cache_path = path.string(),
        .refresh = true,
        .allow_stale_cache = true,
    });

    require(result.ok, "stale cache fallback should succeed");
    require(result.from_cache, "fallback should report cache source");
    require(result.json_text.find("STALE") != std::string::npos, "fallback cache text should be returned");
    std::filesystem::remove(path);
}

void fails_when_download_fails_and_no_cache_exists() {
    const auto path = test_path("tradingbot_upstox_cache_missing.json");
    auto transport = std::make_shared<FakeTransport>();
    transport->responses = {{.status_code = 500, .body = "temporary"}};
    tradingbot::infra::UpstoxInstrumentJsonCache cache(transport);

    const auto result = cache.load({
        .url = "https://assets.upstox.com/complete.json.gz",
        .cache_path = path.string(),
        .refresh = true,
        .allow_stale_cache = true,
    });

    require(!result.ok, "missing cache plus failed download should fail");
    require(result.error.find("status 500") != std::string::npos, "error should include HTTP status");
}

void local_cache_mode_reads_cache_without_transport() {
    const auto path = test_path("tradingbot_upstox_cache_local.json");
    write_text(path, "[{\"instrument_key\":\"NSE_EQ|LOCAL\"}]");
    tradingbot::infra::UpstoxInstrumentJsonCache cache(nullptr);

    const auto result = cache.load({
        .cache_path = path.string(),
    });

    require(result.ok, "local cache mode should read file without transport");
    require(result.from_cache, "local cache mode should report cache");
    require(result.json_text.find("LOCAL") != std::string::npos, "local file text should be returned");
    std::filesystem::remove(path);
}

void transport_exception_uses_stale_cache() {
    const auto path = test_path("tradingbot_upstox_cache_exception.json");
    write_text(path, "[{\"instrument_key\":\"NSE_EQ|EXISTING\"}]");
    auto transport = std::make_shared<FakeTransport>();
    transport->throw_on_send = true;
    tradingbot::infra::UpstoxInstrumentJsonCache cache(transport);

    const auto result = cache.load({
        .url = "https://assets.upstox.com/complete.json.gz",
        .cache_path = path.string(),
        .refresh = true,
        .allow_stale_cache = true,
    });

    require(result.ok, "transport exception should fall back to cache when allowed");
    require(result.from_cache, "exception fallback should report cache");
    require(result.json_text.find("EXISTING") != std::string::npos, "existing cache should be returned");
    std::filesystem::remove(path);
}

}  // namespace

int main() {
    reads_existing_cache_when_refresh_is_disabled();
    downloads_and_writes_cache_when_refresh_is_enabled();
    cache_hit_reports_metadata_path_without_rewriting_metadata();
    falls_back_to_cache_when_download_fails();
    fails_when_download_fails_and_no_cache_exists();
    local_cache_mode_reads_cache_without_transport();
    transport_exception_uses_stale_cache();
    return 0;
}
