import contextlib
import csv
import datetime as dt
import io
import json
import sqlite3
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.live_websocket_scanner import (
    LiveQuote,
    build_parser,
    build_subscription_payload,
    decode_feed_response_quotes,
    merge_live_candle,
    scan_with_live_quotes,
    update_live_state,
    validate_args,
)
from scripts.live_rsi_divergence_scan import Candle, Instrument


def varint(value: int) -> bytes:
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def field_varint(number: int, value: int) -> bytes:
    return varint((number << 3) | 0) + varint(value)


def field_double(number: int, value: float) -> bytes:
    return varint((number << 3) | 1) + struct.pack("<d", value)


def field_bytes(number: int, value: bytes) -> bytes:
    return varint((number << 3) | 2) + varint(len(value)) + value


def ltpc(ltp: float, ltt: int) -> bytes:
    return field_double(1, ltp) + field_varint(2, ltt)


def feed_entry(instrument_key: str, ltp: float, ltt: int) -> bytes:
    feed = field_bytes(1, ltpc(ltp, ltt))
    entry = field_bytes(1, instrument_key.encode("utf-8")) + field_bytes(2, feed)
    return field_bytes(2, entry)


class LiveWebsocketScannerTests(unittest.TestCase):
    def test_subscription_payload_is_binary_json(self):
        payload = build_subscription_payload(["NSE_EQ|INE002A01018"], mode="ltpc", guid="test-guid")

        decoded = json.loads(payload.decode("utf-8"))

        self.assertIsInstance(payload, bytes)
        self.assertEqual(decoded["guid"], "test-guid")
        self.assertEqual(decoded["method"], "sub")
        self.assertEqual(decoded["data"]["mode"], "ltpc")
        self.assertEqual(decoded["data"]["instrumentKeys"], ["NSE_EQ|INE002A01018"])

    def test_decodes_v3_feed_response_ltpc_quotes(self):
        body = field_varint(1, 1) + feed_entry("NSE_EQ|INE002A01018", 123.45, 1760000000000) + field_varint(3, 1760000001000)

        quotes = decode_feed_response_quotes(body)

        self.assertEqual(len(quotes), 1)
        self.assertEqual(quotes[0].instrument_key, "NSE_EQ|INE002A01018")
        self.assertAlmostEqual(quotes[0].ltp, 123.45)

    def test_live_quote_updates_current_day_candle(self):
        states = {}

        update_live_state(states, LiveQuote("NSE_EQ|A", 100.0, dt.datetime(2026, 7, 2, 4, 0, tzinfo=dt.timezone.utc)))
        update_live_state(states, LiveQuote("NSE_EQ|A", 105.0, dt.datetime(2026, 7, 2, 4, 1, tzinfo=dt.timezone.utc)))
        update_live_state(states, LiveQuote("NSE_EQ|A", 99.0, dt.datetime(2026, 7, 2, 4, 2, tzinfo=dt.timezone.utc)))

        candle = states["NSE_EQ|A"].to_candle()

        self.assertEqual(candle.open, 100.0)
        self.assertEqual(candle.high, 105.0)
        self.assertEqual(candle.low, 99.0)
        self.assertEqual(candle.close, 99.0)
        self.assertEqual(candle.timestamp, "2026-07-02T00:00:00+05:30")

    def test_merge_live_candle_replaces_same_day_candle(self):
        historical = [
            Candle("2026-07-01T00:00:00+05:30", 90, 95, 88, 94, 1000),
            Candle("2026-07-02T00:00:00+05:30", 94, 96, 93, 95, 1000),
        ]
        states = {}
        update_live_state(states, LiveQuote("NSE_EQ|A", 101.0, dt.datetime(2026, 7, 2, 4, 0, tzinfo=dt.timezone.utc)))

        merged = merge_live_candle(historical, states["NSE_EQ|A"])

        self.assertEqual(len(merged), 2)
        self.assertEqual(merged[-1].close, 101.0)

    def test_scan_with_live_quotes_writes_report(self):
        with tempfile.TemporaryDirectory() as directory:
            db_path = Path(directory) / "candles.sqlite3"
            output = Path(directory) / "live.csv"
            with contextlib.closing(sqlite3.connect(db_path)) as connection:
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
                closes = [10, 11, 9, 12, 8, 13, 7, 14, 15] + [16] * 40
                for index, close in enumerate(closes):
                    connection.execute(
                        "INSERT INTO candles(run_id,instrument_key,interval,candle_at,open,high,low,close,volume) VALUES(?,?,?,?,?,?,?,?,?)",
                        ("test", "NSE_EQ|A", "days:1", f"2026-01-{(index % 28) + 1:02d}T00:00:00+05:30", close, close, close, close, 1),
                    )
                connection.commit()

            ranked = scan_with_live_quotes(
                sqlite_path=db_path,
                output_csv=output,
                labels_csv=None,
                chart_dir=None,
                instruments=[Instrument("A", "NSE_EQ|A")],
                quotes=[],
                interval="days:1",
                min_latest_close=0,
                top_n=5,
                max_charts=0,
            )
            with output.open("r", encoding="utf-8") as file:
                rows = list(csv.DictReader(file))

        self.assertEqual(len(rows), len(ranked))
        self.assertIn("instrument_key", rows[0] if rows else {"instrument_key": ""})

    def test_validate_args_rejects_invalid_values(self):
        parser = build_parser()
        cases = [
            ["--instrument-limit", "0"],
            ["--duration-seconds", "0"],
            ["--top-n", "0"],
            ["--max-charts", "-1"],
            ["--min-latest-close", "-1"],
        ]
        for case in cases:
            with self.subTest(case=case):
                args = parser.parse_args(case)
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    validate_args(parser, args)


if __name__ == "__main__":
    unittest.main()
