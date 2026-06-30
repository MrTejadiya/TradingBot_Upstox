#include "tradingbot/strategy/indicators.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

void require_near(double actual, double expected, double tolerance, const std::string& message) {
    require(std::fabs(actual - expected) <= tolerance, message);
}

std::vector<tradingbot::core::Candle> sample_candles() {
    return {
        {.close = 10.0},
        {.close = 11.0},
        {.close = 12.0},
        {.close = 13.0},
    };
}

void extracts_close_prices_from_candles() {
    const auto closes = tradingbot::strategy::close_prices(sample_candles());

    require(closes.size() == 4, "close count should match candle count");
    require(closes.front() == 10.0, "first close should parse");
    require(closes.back() == 13.0, "last close should parse");
}

void calculates_simple_moving_average() {
    const auto average = tradingbot::strategy::simple_moving_average({10.0, 11.0, 12.0, 13.0}, 3);

    require(average.has_value(), "SMA should calculate when enough data is present");
    require_near(*average, 12.0, 0.0001, "SMA should average trailing period");
}

void calculates_exponential_moving_average() {
    const auto average = tradingbot::strategy::exponential_moving_average({10.0, 11.0, 12.0, 13.0}, 3);

    require(average.has_value(), "EMA should calculate when enough data is present");
    require_near(*average, 12.0, 0.0001, "EMA should use SMA seed and multiplier smoothing");
}

void returns_nullopt_when_data_is_insufficient() {
    require(!tradingbot::strategy::simple_moving_average({10.0, 11.0}, 3), "SMA should require period data");
    require(!tradingbot::strategy::exponential_moving_average({10.0, 11.0}, 3), "EMA should require period data");
    require(!tradingbot::strategy::relative_strength_index({10.0, 11.0}, 3),
            "RSI should require period plus one prices");
    require(!tradingbot::strategy::moving_average_convergence_divergence({10.0, 11.0}, 2, 4, 3),
            "MACD should require slow and signal data");
}

void calculates_relative_strength_index() {
    const auto rsi = tradingbot::strategy::relative_strength_index({44.0, 44.15, 43.9, 44.35, 44.8, 44.7}, 5);

    require(rsi.has_value(), "RSI should calculate when enough data is present");
    require_near(*rsi, 75.0, 0.0001, "RSI should reflect average gains over losses");
}

void calculates_macd_value() {
    const std::vector<double> values{10.0, 10.5, 11.0, 11.8, 12.5, 12.1, 12.9, 13.6, 13.2, 14.0};

    const auto macd = tradingbot::strategy::moving_average_convergence_divergence(values, 3, 5, 3);

    require(macd.has_value(), "MACD should calculate when enough data is present");
    require_near(macd->macd, 0.3987, 0.0001, "MACD should be fast EMA minus slow EMA");
    require_near(macd->signal, 0.4258, 0.0001, "MACD signal should smooth MACD series");
    require_near(macd->histogram, -0.0271, 0.0001, "MACD histogram should be MACD minus signal");
}

}  // namespace

int main() {
    extracts_close_prices_from_candles();
    calculates_simple_moving_average();
    calculates_exponential_moving_average();
    returns_nullopt_when_data_is_insufficient();
    calculates_relative_strength_index();
    calculates_macd_value();
    return 0;
}
