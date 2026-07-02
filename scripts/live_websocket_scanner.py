#!/usr/bin/env python3
"""Read-only Upstox Market Data Feed V3 websocket scanner."""

from __future__ import annotations

import argparse
import contextlib
import csv
import datetime as dt
import json
import sqlite3
import sys
import time
import uuid
import urllib.request
import urllib.error
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from scripts.live_rsi_divergence_scan import Candle, Instrument, read_access_token
from scripts.offline_historical_scanner_report import (
    DEFAULT_LABELS_CSV,
    DEFAULT_SQLITE_PATH,
    OfflineScannerResult,
    load_label_map,
    rank_results,
    scan_candles,
    write_charts,
    write_report,
)


DEFAULT_BASE_URL = "https://api.upstox.com"
DEFAULT_OUTPUT_CSV = "reports/live-websocket-scanner-ranking.csv"
DEFAULT_CHART_DIR = "reports/live-websocket-scanner-charts"
DEFAULT_INTERVAL = "days:1"
DEFAULT_MIN_LATEST_CLOSE = 20.0
DEFAULT_WEIGHTS = {
    "rsi_bullish_divergence": 1.20,
    "macd_bullish_cross": 1.00,
}


@dataclass(frozen=True)
class LiveQuote:
    instrument_key: str
    ltp: float
    timestamp: dt.datetime


@dataclass
class LiveCandleState:
    timestamp: str
    open: float
    high: float
    low: float
    close: float
    volume: int = 0

    def update(self, price: float) -> None:
        if self.open <= 0:
            self.open = price
            self.high = price
            self.low = price
        self.high = max(self.high, price)
        self.low = min(self.low, price)
        self.close = price

    def to_candle(self) -> Candle:
        return Candle(self.timestamp, self.open, self.high, self.low, self.close, self.volume)


def parse_varint(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    shift = 0
    while offset < len(data):
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7F) << shift
        if byte < 0x80:
            return value, offset
        shift += 7
        if shift > 70:
            break
    raise ValueError("invalid protobuf varint")


def parse_protobuf_fields(data: bytes) -> list[tuple[int, int, bytes | int | float]]:
    import struct

    fields: list[tuple[int, int, bytes | int | float]] = []
    offset = 0
    while offset < len(data):
        key, offset = parse_varint(data, offset)
        field_number = key >> 3
        wire_type = key & 0x07
        if wire_type == 0:
            value, offset = parse_varint(data, offset)
            fields.append((field_number, wire_type, value))
        elif wire_type == 1:
            if offset + 8 > len(data):
                raise ValueError("truncated protobuf fixed64")
            fields.append((field_number, wire_type, struct.unpack("<d", data[offset : offset + 8])[0]))
            offset += 8
        elif wire_type == 2:
            length, offset = parse_varint(data, offset)
            end = offset + length
            if end > len(data):
                raise ValueError("truncated protobuf bytes")
            fields.append((field_number, wire_type, data[offset:end]))
            offset = end
        elif wire_type == 5:
            if offset + 4 > len(data):
                raise ValueError("truncated protobuf fixed32")
            fields.append((field_number, wire_type, data[offset : offset + 4]))
            offset += 4
        else:
            raise ValueError(f"unsupported protobuf wire type {wire_type}")
    return fields


def decode_ltpc(message: bytes) -> tuple[float | None, int | None]:
    ltp: float | None = None
    ltt: int | None = None
    for field_number, _, value in parse_protobuf_fields(message):
        if field_number == 1 and isinstance(value, float):
            ltp = value
        elif field_number == 2 and isinstance(value, int):
            ltt = value
    return ltp, ltt


def decode_feed_ltpc(feed: bytes) -> tuple[float | None, int | None]:
    for field_number, _, value in parse_protobuf_fields(feed):
        if not isinstance(value, bytes):
            continue
        if field_number == 1:
            return decode_ltpc(value)
        if field_number == 2:
            for full_field, _, full_value in parse_protobuf_fields(value):
                if not isinstance(full_value, bytes):
                    continue
                if full_field in {1, 2}:
                    for nested_field, _, nested_value in parse_protobuf_fields(full_value):
                        if nested_field == 1 and isinstance(nested_value, bytes):
                            return decode_ltpc(nested_value)
        if field_number == 3:
            for nested_field, _, nested_value in parse_protobuf_fields(value):
                if nested_field == 1 and isinstance(nested_value, bytes):
                    return decode_ltpc(nested_value)
    return None, None


def decode_feed_response_quotes(data: bytes) -> list[LiveQuote]:
    quotes: list[LiveQuote] = []
    current_ts: int | None = None
    feed_entries: list[bytes] = []
    for field_number, _, value in parse_protobuf_fields(data):
        if field_number == 2 and isinstance(value, bytes):
            feed_entries.append(value)
        elif field_number == 3 and isinstance(value, int):
            current_ts = value

    for entry in feed_entries:
        instrument_key = ""
        feed: bytes | None = None
        for field_number, _, value in parse_protobuf_fields(entry):
            if field_number == 1 and isinstance(value, bytes):
                instrument_key = value.decode("utf-8", errors="replace")
            elif field_number == 2 and isinstance(value, bytes):
                feed = value
        if not instrument_key or feed is None:
            continue
        ltp, ltt = decode_feed_ltpc(feed)
        if ltp is None or ltp <= 0:
            continue
        timestamp_ms = ltt or current_ts or int(time.time() * 1000)
        quotes.append(
            LiveQuote(
                instrument_key=instrument_key,
                ltp=ltp,
                timestamp=dt.datetime.fromtimestamp(timestamp_ms / 1000, tz=dt.timezone.utc),
            )
        )
    return quotes


def build_subscription_payload(instrument_keys: list[str], mode: str = "ltpc", guid: str | None = None) -> bytes:
    payload = {
        "guid": guid or str(uuid.uuid4()),
        "method": "sub",
        "data": {
            "mode": mode,
            "instrumentKeys": instrument_keys,
        },
    }
    return json.dumps(payload, separators=(",", ":")).encode("utf-8")


def authorize_market_data_feed(base_url: str, token: str) -> str:
    request = urllib.request.Request(
        base_url.rstrip("/") + "/v3/feed/market-data-feed/authorize",
        headers={
            "Accept": "application/json",
            "Authorization": "Bearer " + token,
            "User-Agent": "TradingBot-Upstox-Live-Websocket-Scanner/0.1",
        },
        method="GET",
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"Upstox websocket authorize failed: HTTP {exc.code}: {detail[:300]}") from exc
    uri = str(payload.get("data", {}).get("authorized_redirect_uri") or "").strip()
    if payload.get("status") != "success" or not uri.startswith("wss://"):
        raise RuntimeError("Upstox authorize response did not contain a websocket URI")
    return uri


def load_instrument_keys_from_ranking(path: Path, limit: int) -> list[Instrument]:
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        instruments = [
            Instrument(label=(row.get("symbol") or row.get("label") or row.get("instrument_key") or "").strip(),
                       key=(row.get("instrument_key") or "").strip())
            for row in reader
            if (row.get("instrument_key") or "").strip()
        ]
    return instruments[:limit] if limit > 0 else instruments


def load_historical_candles(connection: sqlite3.Connection, instrument_key: str, interval: str) -> list[Candle]:
    rows = connection.execute(
        """
        SELECT candle_at, open, high, low, close, volume
        FROM candles
        WHERE instrument_key = ? AND interval = ?
        ORDER BY candle_at ASC
        """,
        (instrument_key, interval),
    ).fetchall()
    return [
        Candle(str(row[0]), float(row[1]), float(row[2]), float(row[3]), float(row[4]), int(row[5]))
        for row in rows
    ]


def merge_live_candle(historical: list[Candle], live: LiveCandleState | None) -> list[Candle]:
    if live is None:
        return historical
    if historical and historical[-1].timestamp[:10] == live.timestamp[:10]:
        return [*historical[:-1], live.to_candle()]
    return [*historical, live.to_candle()]


def update_live_state(states: dict[str, LiveCandleState], quote: LiveQuote) -> None:
    trading_day = quote.timestamp.astimezone(dt.timezone(dt.timedelta(hours=5, minutes=30))).date().isoformat()
    timestamp = trading_day + "T00:00:00+05:30"
    state = states.get(quote.instrument_key)
    if state is None or state.timestamp != timestamp:
        state = LiveCandleState(timestamp=timestamp, open=quote.ltp, high=quote.ltp, low=quote.ltp, close=quote.ltp)
        states[quote.instrument_key] = state
    else:
        state.update(quote.ltp)


def run_websocket_capture(uri: str, instrument_keys: list[str], mode: str, duration_seconds: float) -> list[LiveQuote]:
    try:
        import websocket
    except ImportError as exc:
        raise RuntimeError("missing dependency: install websocket-client to run the live websocket scanner") from exc

    quotes: list[LiveQuote] = []
    deadline = time.monotonic() + duration_seconds
    ws = websocket.create_connection(uri, timeout=10)
    try:
        ws.send_binary(build_subscription_payload(instrument_keys, mode=mode))
        while time.monotonic() < deadline:
            ws.settimeout(max(0.5, min(5.0, deadline - time.monotonic())))
            try:
                message = ws.recv()
            except Exception:
                if time.monotonic() >= deadline:
                    break
                continue
            if isinstance(message, str):
                continue
            quotes.extend(decode_feed_response_quotes(bytes(message)))
    finally:
        ws.close()
    return quotes


def scan_with_live_quotes(
    sqlite_path: Path,
    output_csv: Path,
    labels_csv: Path | None,
    chart_dir: Path | None,
    instruments: list[Instrument],
    quotes: list[LiveQuote],
    interval: str,
    min_latest_close: float,
    top_n: int,
    max_charts: int,
) -> list[OfflineScannerResult]:
    label_map = load_label_map(labels_csv)
    for instrument in instruments:
        label_map.setdefault(instrument.key, instrument.label)

    live_states: dict[str, LiveCandleState] = {}
    for quote in quotes:
        update_live_state(live_states, quote)

    candle_cache: dict[str, list[Candle]] = {}
    results: list[OfflineScannerResult] = []
    with contextlib.closing(sqlite3.connect(sqlite_path)) as connection:
        for instrument in instruments:
            historical = load_historical_candles(connection, instrument.key, interval)
            candles = merge_live_candle(historical, live_states.get(instrument.key))
            candle_cache[instrument.key] = candles
            results.append(
                scan_candles(
                    instrument_key=instrument.key,
                    candles=candles,
                    label_map=label_map,
                    weights=DEFAULT_WEIGHTS,
                    rsi_period=14,
                    wing_size=1,
                    macd_fast_period=12,
                    macd_slow_period=26,
                    macd_signal_period=9,
                    min_candles=40,
                )
            )
    ranked = rank_results(results, minimum_score=0.01, top_n=top_n, include_all=False, min_latest_close=min_latest_close)
    write_charts(ranked, candle_cache, chart_dir, rsi_period=14, wing_size=1, max_charts=max_charts)
    write_report(output_csv, ranked)
    return ranked


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run read-only Upstox V3 websocket scanner.")
    parser.add_argument("--sqlite-path", default=DEFAULT_SQLITE_PATH)
    parser.add_argument("--ranking-csv", default="reports/offline-historical-scanner-ranking.csv")
    parser.add_argument("--labels-csv", default=DEFAULT_LABELS_CSV)
    parser.add_argument("--output-csv", default=DEFAULT_OUTPUT_CSV)
    parser.add_argument("--chart-dir", default=DEFAULT_CHART_DIR)
    parser.add_argument("--token-file", default="upstox_token.txt")
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument("--interval", default=DEFAULT_INTERVAL)
    parser.add_argument("--instrument-limit", type=int, default=50)
    parser.add_argument("--duration-seconds", type=float, default=60.0)
    parser.add_argument("--mode", default="ltpc", choices=["ltpc", "full"])
    parser.add_argument("--top-n", type=int, default=25)
    parser.add_argument("--max-charts", type=int, default=10)
    parser.add_argument("--min-latest-close", type=float, default=DEFAULT_MIN_LATEST_CLOSE)
    parser.add_argument("--dry-run", action="store_true", help="Build subscription plan but do not connect")
    return parser


def validate_args(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    if args.instrument_limit <= 0:
        parser.error("--instrument-limit must be positive")
    if args.duration_seconds <= 0:
        parser.error("--duration-seconds must be positive")
    if args.top_n <= 0:
        parser.error("--top-n must be positive")
    if args.max_charts < 0:
        parser.error("--max-charts must be non-negative")
    if args.min_latest_close < 0:
        parser.error("--min-latest-close must be non-negative")


def main(argv: Iterable[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(list(argv) if argv is not None else None)
    validate_args(parser, args)
    instruments = load_instrument_keys_from_ranking(Path(args.ranking_csv), args.instrument_limit)
    if not instruments:
        raise RuntimeError("ranking CSV did not contain any instrument_key rows")

    print("Read-only online scanner: Upstox Market Data Feed V3 only; no order endpoints are called.")
    print(f"instruments={len(instruments)} duration_seconds={args.duration_seconds} mode={args.mode}")
    if args.dry_run:
        print(build_subscription_payload([instrument.key for instrument in instruments], mode=args.mode).decode("utf-8"))
        return 0

    try:
        token = read_access_token(Path(args.token_file))
        uri = authorize_market_data_feed(args.base_url, token)
        quotes = run_websocket_capture(uri, [instrument.key for instrument in instruments], args.mode, args.duration_seconds)
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        return 1
    print(f"quotes={len(quotes)} unique_instruments={len({quote.instrument_key for quote in quotes})}")
    ranked = scan_with_live_quotes(
        sqlite_path=Path(args.sqlite_path),
        output_csv=Path(args.output_csv),
        labels_csv=Path(args.labels_csv) if args.labels_csv else None,
        chart_dir=Path(args.chart_dir) if args.chart_dir else None,
        instruments=instruments,
        quotes=quotes,
        interval=args.interval,
        min_latest_close=args.min_latest_close,
        top_n=args.top_n,
        max_charts=args.max_charts,
    )
    print(f"ranked={len(ranked)} output={args.output_csv}")
    for result in ranked[:10]:
        print(
            f"{result.symbol or result.instrument_key} | {result.instrument_key} | score={result.score:.4f} | "
            f"signals={result.signal_count} | close={result.latest_close:.2f}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
