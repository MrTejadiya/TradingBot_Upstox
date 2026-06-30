#pragma once

#include "tradingbot/core/domain.hpp"
#include "tradingbot/infra/upstox_api_client.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tradingbot::infra {

struct CandleQuery {
    core::InstrumentKey instrument_key;
    std::string unit;
    int interval{1};
    std::string from_date;
    std::string to_date;
};

struct CandleResult {
    bool ok{false};
    std::vector<core::Candle> candles;
    ApiEvent api_event;
    bool cache_hit{false};
    std::string error;
};

class CandleCache {
public:
    virtual ~CandleCache() = default;
    virtual std::optional<std::vector<core::Candle>> get(const CandleQuery& query) = 0;
    virtual void put(const CandleQuery& query, const std::vector<core::Candle>& candles) = 0;
};

class InMemoryCandleCache final : public CandleCache {
public:
    std::optional<std::vector<core::Candle>> get(const CandleQuery& query) override;
    void put(const CandleQuery& query, const std::vector<core::Candle>& candles) override;

private:
    std::map<std::string, std::vector<core::Candle>> entries_;
};

class CandleService {
public:
    CandleService(std::shared_ptr<UpstoxApiClient> api_client, std::shared_ptr<CandleCache> cache);

    CandleResult fetch_candles(const CandleQuery& query);

private:
    std::shared_ptr<UpstoxApiClient> api_client_;
    std::shared_ptr<CandleCache> cache_;
};

std::string historical_candle_path(const CandleQuery& query);
CandleResult parse_historical_candle_response(const CandleQuery& query, const ApiResult& api_result);
std::string candle_cache_key(const CandleQuery& query);

}  // namespace tradingbot::infra

