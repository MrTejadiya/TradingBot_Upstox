import contextlib
import csv
import gzip
import io
import json
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.download_historical_candles import (
    DownloadResult,
    apply_schema,
    build_parser,
    download_all,
    interval_name,
    load_instruments_csv,
    load_instruments_from_args,
    load_upstox_instruments_json_file,
    parse_upstox_instruments_json,
    upsert_candles,
    validate_args,
    write_summary,
)
from scripts.live_rsi_divergence_scan import Candle, Instrument


class HistoricalCandleDownloadTests(unittest.TestCase):
    def write_csv(self, path: Path, text: str) -> None:
        path.write_text(text, encoding="utf-8")

    def sample_candles(self) -> list[Candle]:
        return [
            Candle("2026-07-01T00:00:00+05:30", 100, 105, 99, 104, 1000),
            Candle("2026-07-02T00:00:00+05:30", 104, 106, 102, 103, 1200),
        ]

    def test_load_instruments_csv_reads_enabled_rows_and_prefers_nse(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "instruments.csv"
            self.write_csv(
                path,
                "instrument_key,symbol,enabled,quantity,max_position_qty,target_profit_pct\n"
                "BSE_EQ|INE002A01018,RELIANCE_BSE,true,1,2,10\n"
                "NSE_EQ|INE002A01018,RELIANCE_NSE,true,1,2,10\n"
                "NSE_EQ|INE999A01010,DISABLED,false,1,2,10\n",
            )

            instruments = load_instruments_csv(path)

            self.assertEqual(len(instruments), 1)
            self.assertEqual(instruments[0].label, "RELIANCE_NSE")
            self.assertEqual(instruments[0].key, "NSE_EQ|INE002A01018")

    def test_load_instruments_csv_can_include_disabled_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "instruments.csv"
            self.write_csv(
                path,
                "instrument_key,symbol,enabled\n"
                "NSE_EQ|INE999A01010,DISABLED,false\n",
            )

            instruments = load_instruments_csv(path, enabled_only=False)

            self.assertEqual(len(instruments), 1)
            self.assertEqual(instruments[0].label, "DISABLED")

    def test_load_instruments_csv_rejects_missing_required_header(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "instruments.csv"
            self.write_csv(path, "symbol,enabled\nRELIANCE,true\n")

            with self.assertRaisesRegex(ValueError, "instrument_key"):
                load_instruments_csv(path)

    def test_parse_upstox_instruments_json_filters_equities_and_prefers_nse(self):
        payload = [
            {
                "segment": "BSE_EQ",
                "instrument_type": "EQ",
                "instrument_key": "BSE_EQ|INE002A01018",
                "trading_symbol": "RELIANCE_BSE",
            },
            {
                "segment": "NSE_EQ",
                "instrument_type": "EQ",
                "instrument_key": "NSE_EQ|INE002A01018",
                "trading_symbol": "RELIANCE_NSE",
            },
            {
                "segment": "NSE_FO",
                "instrument_type": "FUT",
                "instrument_key": "NSE_FO|123",
                "trading_symbol": "RELIANCE_FUT",
            },
            {
                "segment": "NSE_EQ",
                "instrument_type": "EQ",
                "instrument_key": "",
                "trading_symbol": "BAD",
            },
        ]

        instruments = parse_upstox_instruments_json(json.dumps(payload))

        self.assertEqual(len(instruments), 1)
        self.assertEqual(instruments[0].label, "RELIANCE_NSE")
        self.assertEqual(instruments[0].key, "NSE_EQ|INE002A01018")

    def test_parse_upstox_instruments_json_uses_label_fallbacks(self):
        payload = [
            {
                "segment": "NSE_EQ",
                "instrument_type": "EQ",
                "instrument_key": "NSE_EQ|INE111A01010",
                "short_name": "SHORT",
            },
            {
                "segment": "NSE_EQ",
                "instrument_type": "EQ",
                "instrument_key": "NSE_EQ|INE222A01010",
                "name": "NAMEONLY",
            },
        ]

        instruments = parse_upstox_instruments_json(json.dumps(payload))

        self.assertEqual([instrument.label for instrument in instruments], ["SHORT", "NAMEONLY"])

    def test_load_upstox_instruments_json_file_supports_gzip(self):
        payload = [
            {
                "segment": "NSE_EQ",
                "instrument_type": "EQ",
                "instrument_key": "NSE_EQ|INE111A01010",
                "trading_symbol": "ONE",
            }
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "complete.json.gz"
            path.write_bytes(gzip.compress(json.dumps(payload).encode("utf-8")))

            instruments = load_upstox_instruments_json_file(path)

        self.assertEqual(len(instruments), 1)
        self.assertEqual(instruments[0].label, "ONE")

    def test_load_instruments_from_args_accepts_upstox_json_file(self):
        payload = [
            {
                "segment": "NSE_EQ",
                "instrument_type": "EQ",
                "instrument_key": "NSE_EQ|INE111A01010",
                "trading_symbol": "ONE",
            }
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "complete.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            parser = build_parser()
            args = parser.parse_args(["--upstox-instruments-json", str(path)])

            instruments = load_instruments_from_args(args)

        self.assertEqual([instrument.key for instrument in instruments], ["NSE_EQ|INE111A01010"])

    def test_upsert_candles_creates_resumable_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            db_path = Path(directory) / "candles.sqlite3"
            with contextlib.closing(sqlite3.connect(db_path)) as connection:
                apply_schema(connection)
                upsert_candles(connection, "run-1", "NSE_EQ|INE002A01018", "days", 1, self.sample_candles())
                upsert_candles(connection, "run-2", "NSE_EQ|INE002A01018", "days", 1, self.sample_candles())
                count = connection.execute("SELECT COUNT(*) FROM candles").fetchone()[0]
                run_id = connection.execute("SELECT run_id FROM candles ORDER BY candle_at LIMIT 1").fetchone()[0]

            self.assertEqual(count, 2)
            self.assertEqual(run_id, "run-2")

    def test_download_all_persists_successes_and_continues_after_failure(self):
        calls: list[str] = []

        def fake_fetch(base_url, token, instrument_key, unit, interval, from_date, to_date):
            calls.append(instrument_key)
            if instrument_key.endswith("FAIL"):
                raise RuntimeError("boom")
            return self.sample_candles()

        with tempfile.TemporaryDirectory() as directory:
            db_path = Path(directory) / "candles.sqlite3"
            results = download_all(
                instruments=[Instrument("GOOD", "NSE_EQ|GOOD"), Instrument("BAD", "NSE_EQ|FAIL")],
                database_path=db_path,
                base_url="https://api.upstox.com",
                token="token",
                unit="days",
                interval=1,
                from_date="2026-07-01",
                to_date="2026-07-02",
                run_id="run-1",
                fetch=fake_fetch,
            )
            with contextlib.closing(sqlite3.connect(db_path)) as connection:
                count = connection.execute("SELECT COUNT(*) FROM candles").fetchone()[0]

        self.assertEqual(calls, ["NSE_EQ|GOOD", "NSE_EQ|FAIL"])
        self.assertEqual(count, 2)
        self.assertTrue(results[0].ok)
        self.assertFalse(results[1].ok)
        self.assertIn("boom", results[1].error)

    def test_download_all_respects_limit(self):
        calls: list[str] = []

        def fake_fetch(base_url, token, instrument_key, unit, interval, from_date, to_date):
            calls.append(instrument_key)
            return self.sample_candles()

        with tempfile.TemporaryDirectory() as directory:
            download_all(
                instruments=[Instrument("A", "NSE_EQ|A"), Instrument("B", "NSE_EQ|B")],
                database_path=Path(directory) / "candles.sqlite3",
                base_url="https://api.upstox.com",
                token="token",
                unit="days",
                interval=1,
                from_date="2026-07-01",
                to_date="2026-07-02",
                limit=1,
                fetch=fake_fetch,
            )

        self.assertEqual(calls, ["NSE_EQ|A"])

    def test_write_summary_outputs_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.csv"
            write_summary(
                path,
                [
                    DownloadResult("GOOD", "NSE_EQ|GOOD", True, 2),
                    DownloadResult("BAD", "NSE_EQ|BAD", False, 0, "boom"),
                ],
            )

            with path.open("r", encoding="utf-8") as file:
                rows = list(csv.DictReader(file))

        self.assertEqual(rows[0]["status"], "ok")
        self.assertEqual(rows[0]["candle_count"], "2")
        self.assertEqual(rows[1]["status"], "error")
        self.assertEqual(rows[1]["error"], "boom")

    def test_interval_name_matches_cpp_cache_format(self):
        self.assertEqual(interval_name("days", 1), "days:1")

    def test_validate_args_rejects_invalid_values(self):
        parser = build_parser()
        cases = [
            [],
            ["--instruments-csv", "x.csv", "--upstox-instruments-url", "https://example.test/complete.json.gz"],
            ["--instruments-csv", "x.csv", "--interval", "0"],
            ["--instruments-csv", "x.csv", "--lookback-days", "0"],
            ["--instruments-csv", "x.csv", "--throttle-seconds", "-1"],
            ["--instruments-csv", "x.csv", "--limit", "-1"],
            ["--instruments-csv", "x.csv", "--from-date", "2026-07-01"],
            ["--instruments-csv", "x.csv", "--from-date", "bad", "--to-date", "2026-07-01"],
            ["--instruments-csv", "x.csv", "--from-date", "2026-07-02", "--to-date", "2026-07-01"],
        ]
        for case in cases:
            with self.subTest(case=case):
                args = parser.parse_args(case)
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    validate_args(parser, args)


if __name__ == "__main__":
    unittest.main()
