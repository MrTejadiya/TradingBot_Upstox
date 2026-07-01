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

void detects_pivot_lows_and_highs() {
    const std::vector<double> values{10.0, 8.0, 11.0, 7.0, 12.0, 9.0, 13.0};

    const auto lows = tradingbot::strategy::pivot_lows(values);
    const auto highs = tradingbot::strategy::pivot_highs(values);

    require(lows.size() == 3, "pivot low scan should find local lows");
    require(lows[0].index == 1 && lows[0].value == 8.0, "first pivot low should include index and value");
    require(lows[1].index == 3 && lows[1].value == 7.0, "second pivot low should include index and value");
    require(highs.size() == 2, "pivot high scan should find local highs");
    require(highs[0].index == 2 && highs[0].value == 11.0, "first pivot high should include index and value");
    require(highs[1].index == 4 && highs[1].value == 12.0, "second pivot high should include index and value");
}

void pivot_scans_fail_closed_for_invalid_windows() {
    require(tradingbot::strategy::pivot_lows({10.0, 9.0}, 1).empty(), "pivot lows should require wing data");
    require(tradingbot::strategy::pivot_highs({10.0, 11.0, 10.0}, 0).empty(),
            "pivot highs should reject non-positive wing size");
}

void detects_bullish_and_bearish_divergence() {
    require(tradingbot::strategy::has_bullish_divergence({10.0, 8.0, 11.0, 7.0, 12.0},
                                                         {50.0, 30.0, 55.0, 35.0, 60.0}),
            "bullish divergence should detect lower price low with higher oscillator low");
    require(tradingbot::strategy::has_bearish_divergence({10.0, 12.0, 9.0, 13.0, 8.0},
                                                         {50.0, 70.0, 45.0, 65.0, 40.0}),
            "bearish divergence should detect higher price high with lower oscillator high");
    require(!tradingbot::strategy::has_bullish_divergence({10.0, 8.0, 11.0}, {50.0, 30.0}),
            "divergence should reject mismatched price and oscillator series");
}

void builds_support_and_resistance_trendlines() {
    const std::vector<double> values{10.0, 8.0, 11.0, 7.0, 12.0, 9.0, 13.0};

    const auto support = tradingbot::strategy::support_trendline(values);
    const auto resistance = tradingbot::strategy::resistance_trendline(values);

    require(support.has_value(), "support trendline should use last two pivot lows");
    require(support->start_index == 3 && support->end_index == 5, "support trendline should preserve pivot range");
    require_near(support->slope, 1.0, 0.0001, "support trendline should calculate slope");
    require_near(tradingbot::strategy::trendline_value_at(*support, 6), 10.0, 0.0001,
                 "support trendline should project value at requested index");

    require(resistance.has_value(), "resistance trendline should use last two pivot highs");
    require(resistance->start_index == 2 && resistance->end_index == 4,
            "resistance trendline should preserve pivot range");
    require_near(resistance->slope, 0.5, 0.0001, "resistance trendline should calculate slope");
    require_near(tradingbot::strategy::trendline_value_at(*resistance, 6), 13.0, 0.0001,
                 "resistance trendline should project value at requested index");
}

}  // namespace

int main() {
    extracts_close_prices_from_candles();
    calculates_simple_moving_average();
    calculates_exponential_moving_average();
    returns_nullopt_when_data_is_insufficient();
    calculates_relative_strength_index();
    calculates_macd_value();
    detects_pivot_lows_and_highs();
    pivot_scans_fail_closed_for_invalid_windows();
    detects_bullish_and_bearish_divergence();
    builds_support_and_resistance_trendlines();
    return 0;
}
