import contextlib
import csv
import io
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.live_rsi_divergence_scan import Candle
from scripts.offline_historical_scanner_report import (
    build_parser,
    is_bullish_macd_cross,
    list_instrument_keys,
    load_label_map,
    load_candles,
    parse_weights,
    rank_results,
    run_report,
    scan_candles,
    validate_args,
    write_report,
)


class OfflineHistoricalScannerReportTests(unittest.TestCase):
    def create_database(self, path: Path, rows: dict[str, list[float]]) -> None:
        with contextlib.closing(sqlite3.connect(path)) as connection:
            connection.executescript(
                """
                CREATE TABLE candles (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    run_id TEXT NOT NULL,
                    instrument_key TEXT NOT NULL,
                    interval TEXT NOT NULL,
                    candle_at TEXT NOT NULL,
                    open REAL NOT NULL,
                    high REAL NOT NULL,
                    low REAL NOT NULL,
                    close REAL NOT NULL,
                    volume INTEGER NOT NULL
                );
                """
            )
            for instrument_key, closes in rows.items():
                for index, close in enumerate(closes):
                    connection.execute(
                        """
                        INSERT INTO candles(
                            run_id, instrument_key, interval, candle_at, open, high, low, close, volume
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                        """,
                        (
                            "test",
                            instrument_key,
                            "days:1",
                            f"2026-01-{index + 1:02d}T00:00:00+05:30",
                            close - 1,
                            close + 1,
                            close - 2,
                            close,
                            1000 + index,
                        ),
                    )
            connection.commit()

    def candles(self, closes: list[float]) -> list[Candle]:
        return [Candle(str(index), close - 1, close + 1, close - 2, close, 1000) for index, close in enumerate(closes)]

    def test_loads_instrument_keys_and_candles_from_sqlite(self):
        with tempfile.TemporaryDirectory() as directory:
            db_path = Path(directory) / "candles.sqlite3"
            self.create_database(db_path, {"NSE_EQ|B": [1, 2], "NSE_EQ|A": [3, 4]})
            with contextlib.closing(sqlite3.connect(db_path)) as connection:
                keys = list_instrument_keys(connection, "days:1")
                candles = load_candles(connection, "NSE_EQ|A", "days:1")

        self.assertEqual(keys, ["NSE_EQ|A", "NSE_EQ|B"])
        self.assertEqual([candle.close for candle in candles], [3, 4])

    def test_load_label_map_reads_download_summary_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.csv"
            path.write_text(
                "label,instrument_key,status,candle_count,error\n"
                "RELIANCE,NSE_EQ|INE002A01018,ok,247,\n",
                encoding="utf-8",
            )

            labels = load_label_map(path)

        self.assertEqual(labels, {"NSE_EQ|INE002A01018": "RELIANCE"})

    def test_load_label_map_rejects_missing_required_columns(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.csv"
            path.write_text("symbol,key\nRELIANCE,NSE_EQ|INE002A01018\n", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "instrument_key and label"):
                load_label_map(path)

    def test_detects_bullish_rsi_divergence_signal(self):
        result = scan_candles(
            instrument_key="NSE_EQ|RSI",
            candles=self.candles([10, 11, 9, 12, 8, 13, 7, 14, 15]),
            label_map={"NSE_EQ|RSI": "RSI_TEST"},
            weights={"rsi_bullish_divergence": 1.25, "macd_bullish_cross": 1.0},
            rsi_period=2,
            wing_size=1,
            macd_fast_period=3,
            macd_slow_period=6,
            macd_signal_period=3,
            min_candles=5,
        )

        self.assertTrue(result.bullish_rsi_divergence)
        self.assertEqual(result.symbol, "RSI_TEST")
        self.assertIn("rsi_bullish_divergence", result.strategies)
        self.assertAlmostEqual(result.score, 1.0)

    def test_detects_bullish_macd_cross(self):
        closes = [49.98, 51.14, 52.04, 51.29, 50.78, 52.06, 50.83, 51.11, 51.04, 49.41, 50.21, 51.17]

        detected, snapshot = is_bullish_macd_cross(closes, fast_period=3, slow_period=6, signal_period=3)

        self.assertTrue(detected)
        self.assertIsNotNone(snapshot)
        self.assertGreater(snapshot.histogram, 0)

    def test_rank_results_filters_and_sorts(self):
        low = scan_candles("NSE_EQ|LOW", self.candles([1, 2, 3, 4, 5, 6]), {}, {}, 2, 1, 3, 6, 3, 5)
        high = scan_candles(
            "NSE_EQ|HIGH",
            self.candles([10, 11, 9, 12, 8, 13, 7, 14, 15]),
            {},
            {"rsi_bullish_divergence": 2.0},
            2,
            1,
            3,
            6,
            3,
            5,
        )

        ranked = rank_results([low, high], minimum_score=0.01, top_n=1, include_all=False)

        self.assertEqual([item.instrument_key for item in ranked], ["NSE_EQ|HIGH"])

    def test_rank_results_applies_latest_close_filters(self):
        cheap = scan_candles(
            "NSE_EQ|CHEAP",
            self.candles([1, 2, 1, 3, 0.9, 4, 0.8, 2, 1.5]),
            {},
            {"rsi_bullish_divergence": 1.0},
            2,
            1,
            3,
            6,
            3,
            5,
        )
        normal = scan_candles(
            "NSE_EQ|NORMAL",
            self.candles([10, 11, 9, 12, 8, 13, 7, 14, 15]),
            {},
            {"rsi_bullish_divergence": 1.0},
            2,
            1,
            3,
            6,
            3,
            5,
        )

        ranked = rank_results([cheap, normal], minimum_score=0.01, top_n=0, include_all=False, min_latest_close=5.0)

        self.assertEqual([item.instrument_key for item in ranked], ["NSE_EQ|NORMAL"])

    def test_run_report_writes_ranked_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            db_path = Path(directory) / "candles.sqlite3"
            output = Path(directory) / "report.csv"
            self.create_database(
                db_path,
                {
                    "NSE_EQ|RSI": [10, 11, 9, 12, 8, 13, 7, 14, 15],
                    "NSE_EQ|FLAT": [10, 10, 10, 10, 10, 10, 10, 10, 10],
                },
            )
            labels = Path(directory) / "labels.csv"
            labels.write_text(
                "label,instrument_key,status,candle_count,error\n"
                "RSI_LABEL,NSE_EQ|RSI,ok,9,\n"
                "FLAT_LABEL,NSE_EQ|FLAT,ok,9,\n",
                encoding="utf-8",
            )

            ranked = run_report(
                sqlite_path=db_path,
                output_csv=output,
                labels_csv=labels,
                interval="days:1",
                rsi_period=2,
                wing_size=1,
                macd_fast_period=3,
                macd_slow_period=6,
                macd_signal_period=3,
                min_candles=5,
                minimum_score=0.01,
                min_latest_close=5.0,
                max_latest_close=0.0,
                top_n=10,
                limit=0,
                weights={"rsi_bullish_divergence": 1.0, "macd_bullish_cross": 1.0},
            )
            with output.open("r", encoding="utf-8") as file:
                rows = list(csv.DictReader(file))

        self.assertEqual([item.instrument_key for item in ranked], ["NSE_EQ|RSI"])
        self.assertEqual(ranked[0].symbol, "RSI_LABEL")
        self.assertEqual(rows[0]["rank"], "1")
        self.assertEqual(rows[0]["instrument_key"], "NSE_EQ|RSI")
        self.assertEqual(rows[0]["symbol"], "RSI_LABEL")
        self.assertEqual(rows[0]["bullish_rsi_divergence"], "true")

    def test_write_report_outputs_header_for_empty_results(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "empty.csv"

            write_report(output, [])

            self.assertEqual(output.read_text(encoding="utf-8").splitlines()[0].split(",")[0], "rank")

    def test_parse_weights_overrides_defaults(self):
        weights = parse_weights(["macd_bullish_cross=1.5"])

        self.assertEqual(weights["macd_bullish_cross"], 1.5)
        self.assertIn("rsi_bullish_divergence", weights)

    def test_validate_args_rejects_invalid_values(self):
        parser = build_parser()
        cases = [
            ["--rsi-period", "0"],
            ["--wing-size", "0"],
            ["--macd-fast-period", "6", "--macd-slow-period", "6"],
            ["--minimum-score", "-0.1"],
            ["--min-latest-close", "-1"],
            ["--max-latest-close", "-1"],
            ["--min-latest-close", "100", "--max-latest-close", "10"],
            ["--top-n", "-1"],
            ["--limit", "-1"],
            ["--strategy-weight", "bad"],
            ["--strategy-weight", "macd_bullish_cross=0"],
        ]
        for case in cases:
            with self.subTest(case=case):
                args = parser.parse_args(case)
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    validate_args(parser, args)


if __name__ == "__main__":
    unittest.main()
