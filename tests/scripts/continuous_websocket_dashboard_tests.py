import contextlib
import csv
import datetime as dt
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.continuous_websocket_dashboard import (
    DashboardSnapshot,
    build_parser,
    has_fresh_live_signal,
    keep_fresh_live_strategies,
    load_instruments_from_summary,
    market_close_deadline,
    render_html,
    snapshot_to_dict,
    validate_args,
    write_json_snapshot,
)
from scripts.offline_historical_scanner_report import OfflineScannerResult


class ContinuousWebsocketDashboardTests(unittest.TestCase):
    def test_load_instruments_from_summary_reads_all_or_limit(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.csv"
            path.write_text(
                "label,instrument_key,status,candle_count,error\n"
                "AAA,NSE_EQ|AAA,ok,10,\n"
                "BBB,NSE_EQ|BBB,ok,10,\n",
                encoding="utf-8",
            )

            all_items = load_instruments_from_summary(path)
            limited = load_instruments_from_summary(path, limit=1)

        self.assertEqual([item.key for item in all_items], ["NSE_EQ|AAA", "NSE_EQ|BBB"])
        self.assertEqual([item.label for item in limited], ["AAA"])

    def test_market_close_deadline_uses_same_trading_day_when_open(self):
        now = dt.datetime(2026, 7, 2, 6, 0, tzinfo=dt.timezone.utc)

        deadline = market_close_deadline(now, "15:30")

        self.assertEqual(deadline.astimezone(dt.timezone(dt.timedelta(hours=5, minutes=30))).strftime("%H:%M"), "15:30")

    def test_market_close_deadline_returns_now_after_close(self):
        now = dt.datetime(2026, 7, 2, 11, 0, tzinfo=dt.timezone.utc)

        deadline = market_close_deadline(now, "15:30")

        self.assertEqual(deadline, now)

    def test_snapshot_to_dict_contains_ranked_results(self):
        snapshot = DashboardSnapshot(
            status="streaming",
            subscribed_instruments=2,
            quote_count=5,
            ranked=[
                OfflineScannerResult(
                    instrument_key="NSE_EQ|AAA",
                    symbol="AAA",
                    score=1.23,
                    signal_count=2,
                    strategies=["rsi_bullish_divergence"],
                    latest_close=100,
                )
            ],
        )

        data = snapshot_to_dict(snapshot)

        self.assertEqual(data["status"], "streaming")
        self.assertEqual(data["ranked"][0]["symbol"], "AAA")
        self.assertEqual(data["ranked"][0]["score"], 1.23)
        self.assertIn("latest_signal_age_candles", data["ranked"][0])

    def test_render_html_includes_top_result(self):
        snapshot = DashboardSnapshot(
            status="streaming",
            ranked=[OfflineScannerResult(instrument_key="NSE_EQ|AAA", symbol="AAA", score=1.0, latest_close=100)],
        )

        body = render_html(snapshot)

        self.assertIn("TradingBot Live Scanner", body)
        self.assertIn("AAA", body)
        self.assertIn("Signal Age", body)

    def test_has_fresh_live_signal_requires_live_quote_and_age_window(self):
        fresh = OfflineScannerResult(
            instrument_key="NSE_EQ|AAA",
            score=1.0,
            latest_signal_age_candles=1,
            latest_signal_timestamp="2026-07-02T00:00:00+05:30",
        )
        stale = OfflineScannerResult(
            instrument_key="NSE_EQ|AAA",
            score=1.0,
            latest_signal_age_candles=2,
            latest_signal_timestamp="2026-06-30T00:00:00+05:30",
        )
        missing_live = OfflineScannerResult(
            instrument_key="NSE_EQ|BBB",
            score=1.0,
            latest_signal_age_candles=0,
        )

        self.assertTrue(has_fresh_live_signal(fresh, "NSE_EQ|AAA", {"NSE_EQ|AAA": object()}, 1))
        self.assertFalse(has_fresh_live_signal(stale, "NSE_EQ|AAA", {"NSE_EQ|AAA": object()}, 1))
        self.assertEqual(stale.diagnostic, "stale signal age 2 candles")
        self.assertFalse(has_fresh_live_signal(missing_live, "NSE_EQ|BBB", {}, 1))
        self.assertEqual(missing_live.diagnostic, "waiting for live quote")

    def test_keep_fresh_live_strategies_removes_stale_strategy_contribution(self):
        result = OfflineScannerResult(
            instrument_key="NSE_EQ|EXCELSOFT",
            symbol="EXCELSOFT",
            score=1.66,
            signal_count=2,
            strategies=["macd_bullish_cross", "rsi_bullish_divergence"],
            candle_count=147,
            strategy_signal_indexes={
                "rsi_bullish_divergence": 144,
                "macd_bullish_cross": 146,
            },
            strategy_signal_timestamps={
                "rsi_bullish_divergence": "2026-06-30T00:00:00+05:30",
                "macd_bullish_cross": "2026-07-02T00:00:00+05:30",
            },
            latest_signal_age_candles=0,
            latest_signal_timestamp="2026-07-02T00:00:00+05:30",
        )

        kept = keep_fresh_live_strategies(result, "NSE_EQ|EXCELSOFT", {"NSE_EQ|EXCELSOFT": object()}, 1)

        self.assertTrue(kept)
        self.assertEqual(result.strategies, ["macd_bullish_cross"])
        self.assertEqual(result.signal_count, 1)
        self.assertAlmostEqual(result.score, 0.70)
        self.assertEqual(result.latest_signal_age_candles, 0)
        self.assertEqual(result.latest_signal_timestamp, "2026-07-02T00:00:00+05:30")
        self.assertIn("stale strategies removed", result.diagnostic)

    def test_keep_fresh_live_strategies_keeps_provisional_rsi_as_live_signal(self):
        result = OfflineScannerResult(
            instrument_key="NSE_EQ|AAA",
            symbol="AAA",
            score=0.585,
            signal_count=1,
            strategies=["rsi_bullish_divergence_provisional"],
            candle_count=20,
            strategy_signal_indexes={"rsi_bullish_divergence_provisional": 19},
            strategy_signal_timestamps={"rsi_bullish_divergence_provisional": "2026-07-02T00:00:00+05:30"},
        )

        kept = keep_fresh_live_strategies(result, "NSE_EQ|AAA", {"NSE_EQ|AAA": object()}, 1)

        self.assertTrue(kept)
        self.assertEqual(result.strategies, ["rsi_bullish_divergence_provisional"])
        self.assertEqual(result.latest_signal_age_candles, 0)
        self.assertAlmostEqual(result.score, 0.585)

    def test_write_json_snapshot_outputs_valid_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "snapshot.json"

            write_json_snapshot(path, DashboardSnapshot(status="complete", quote_count=10))

            data = json.loads(path.read_text(encoding="utf-8"))

        self.assertEqual(data["status"], "complete")
        self.assertEqual(data["quote_count"], 10)

    def test_validate_args_rejects_invalid_values(self):
        parser = build_parser()
        cases = [
            ["--instrument-limit", "-1"],
            ["--max-subscription-keys", "0"],
            ["--scan-interval-seconds", "0"],
            ["--duration-seconds", "-1"],
            ["--top-n", "0"],
            ["--min-latest-close", "-1"],
            ["--max-signal-age-candles", "-1"],
            ["--market-close", "bad"],
        ]
        for case in cases:
            with self.subTest(case=case):
                args = parser.parse_args(case)
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    validate_args(parser, args)


if __name__ == "__main__":
    unittest.main()
