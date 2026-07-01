import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.live_rsi_divergence_scan import (
    Candle,
    analyze_candles,
    has_bearish_divergence,
    has_bullish_divergence,
    historical_candle_path,
    parse_candles,
    rsi_series,
)


class LiveRsiDivergenceScanTests(unittest.TestCase):
    def test_historical_candle_path_encodes_instrument_key(self):
        self.assertEqual(
            historical_candle_path("NSE_EQ|INE002A01018", "days", 1, "2026-01-01", "2026-07-01"),
            "/v3/historical-candle/NSE_EQ%7CINE002A01018/days/1/2026-07-01/2026-01-01",
        )

    def test_parse_candles_orders_oldest_first(self):
        body = json.dumps(
            {
                "status": "success",
                "data": {
                    "candles": [
                        ["2026-07-01T00:00:00+05:30", 102, 103, 101, 102.5, 2000],
                        ["2026-06-30T00:00:00+05:30", 100, 101, 99, 100.5, 1000],
                    ]
                },
            }
        )

        candles = parse_candles(body)

        self.assertEqual([c.close for c in candles], [100.5, 102.5])

    def test_rsi_series_matches_known_shape(self):
        values = [44, 44.15, 43.9, 44.35, 44.8, 45, 44.7, 45.4, 45.2, 45.8, 46.1, 46, 46.4, 46.8, 47.0, 46.6]

        rsi = rsi_series(values, 14)

        self.assertIsNone(rsi[13])
        self.assertIsNotNone(rsi[14])
        self.assertGreater(rsi[14], 50)

    def test_detects_bullish_divergence(self):
        prices = [10, 9, 10, 8, 9]
        oscillator = [40, 30, 45, 35, 50]

        detected, pivots = has_bullish_divergence(prices, oscillator)

        self.assertTrue(detected)
        self.assertEqual(pivots, 2)

    def test_detects_bearish_divergence(self):
        prices = [10, 12, 10, 13, 11]
        oscillator = [50, 70, 45, 60, 40]

        detected, pivots = has_bearish_divergence(prices, oscillator)

        self.assertTrue(detected)
        self.assertEqual(pivots, 2)

    def test_analyze_candles_returns_latest_metrics(self):
        candles = [
            Candle(str(index), close - 1, close + 1, close - 2, close, 1000)
            for index, close in enumerate(
                [100, 101, 102, 101, 100, 99, 100, 101, 100, 99, 98, 99, 100, 99, 98, 97, 98, 99, 98, 97, 96, 97]
            )
        ]

        result = analyze_candles(candles, period=5)

        self.assertEqual(result.candle_count, len(candles))
        self.assertEqual(result.latest_close, 97)
        self.assertIsNotNone(result.latest_rsi)


if __name__ == "__main__":
    unittest.main()
