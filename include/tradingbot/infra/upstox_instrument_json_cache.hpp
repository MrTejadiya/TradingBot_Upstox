#pragma once

#include "tradingbot/infra/upstox_api_client.hpp"

#include <memory>
#include <string>

namespace tradingbot::infra {

struct UpstoxInstrumentJsonCacheOptions {
    std::string url;
    std::string cache_path;
    bool refresh{false};
    bool allow_stale_cache{true};
    bool force_ipv4{false};
};

struct UpstoxInstrumentJsonCacheResult {
    bool ok{false};
    std::string json_text;
    bool from_cache{false};
    bool downloaded{false};
    std::string error;
};

class UpstoxInstrumentJsonCache {
public:
    explicit UpstoxInstrumentJsonCache(std::shared_ptr<HttpTransport> transport);

    UpstoxInstrumentJsonCacheResult load(const UpstoxInstrumentJsonCacheOptions& options) const;

private:
    std::shared_ptr<HttpTransport> transport_;
};

}  // namespace tradingbot::infra
