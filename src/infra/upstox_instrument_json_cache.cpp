#include "tradingbot/infra/upstox_instrument_json_cache.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace tradingbot::infra {
namespace {

std::string read_text_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool write_cache_file(const std::string& path, const std::string& text, std::string& error) {
    try {
        const std::filesystem::path cache_path{path};
        if (cache_path.has_parent_path()) {
            std::filesystem::create_directories(cache_path.parent_path());
        }

        const auto temp_path = cache_path.string() + ".tmp";
        {
            std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
            if (!file) {
                error = "unable to open temporary cache file: " + temp_path;
                return false;
            }
            file << text;
            if (!file) {
                error = "unable to write temporary cache file: " + temp_path;
                return false;
            }
        }
        std::filesystem::rename(temp_path, cache_path);
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

UpstoxInstrumentJsonCacheResult cache_result(const std::string& cache_path) {
    UpstoxInstrumentJsonCacheResult result;
    result.json_text = read_text_file(cache_path);
    if (result.json_text.empty()) {
        result.error = "unable to read Upstox instrument JSON cache: " + cache_path;
        return result;
    }
    result.ok = true;
    result.from_cache = true;
    return result;
}

}  // namespace

UpstoxInstrumentJsonCache::UpstoxInstrumentJsonCache(std::shared_ptr<HttpTransport> transport)
    : transport_(std::move(transport)) {}

UpstoxInstrumentJsonCacheResult UpstoxInstrumentJsonCache::load(
    const UpstoxInstrumentJsonCacheOptions& options) const {
    if (options.url.empty()) {
        return cache_result(options.cache_path);
    }
    if (options.cache_path.empty()) {
        return {.error = "Upstox instrument JSON cache path is required when URL fetch is enabled"};
    }

    if (!options.refresh) {
        const auto cached = cache_result(options.cache_path);
        if (cached.ok) {
            return cached;
        }
    }

    if (!transport_) {
        if (options.allow_stale_cache) {
            const auto cached = cache_result(options.cache_path);
            if (cached.ok) {
                return cached;
            }
        }
        return {.error = "HTTP transport is required for Upstox instrument JSON download"};
    }

    HttpRequest request{
        .method = "GET",
        .url = options.url,
        .headers = {{"Accept", "application/json"}},
        .force_ipv4 = options.force_ipv4,
    };

    HttpResponse response;
    try {
        response = transport_->send(request);
    } catch (const std::exception& ex) {
        if (options.allow_stale_cache) {
            const auto cached = cache_result(options.cache_path);
            if (cached.ok) {
                return cached;
            }
        }
        return {.error = std::string{"Upstox instrument JSON download failed: "} + ex.what()};
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        if (options.allow_stale_cache) {
            const auto cached = cache_result(options.cache_path);
            if (cached.ok) {
                return cached;
            }
        }
        return {.error = "Upstox instrument JSON download failed with status " + std::to_string(response.status_code)};
    }

    if (response.body.empty()) {
        if (options.allow_stale_cache) {
            const auto cached = cache_result(options.cache_path);
            if (cached.ok) {
                return cached;
            }
        }
        return {.error = "Upstox instrument JSON download returned an empty body"};
    }

    std::string write_error;
    if (!write_cache_file(options.cache_path, response.body, write_error)) {
        return {.error = "unable to write Upstox instrument JSON cache: " + write_error};
    }

    return {
        .ok = true,
        .json_text = response.body,
        .from_cache = false,
        .downloaded = true,
    };
}

}  // namespace tradingbot::infra
