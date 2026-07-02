#!/usr/bin/env python3
"""Continuous read-only Upstox websocket scanner dashboard."""

from __future__ import annotations

import argparse
import contextlib
import csv
import datetime as dt
import html
import json
import sqlite3
import sys
import threading
import time
from dataclasses import dataclass, field
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Iterable

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from scripts.live_rsi_divergence_scan import Instrument, read_access_token
from scripts.live_websocket_scanner import (
    DEFAULT_BASE_URL,
    DEFAULT_INTERVAL,
    DEFAULT_MIN_LATEST_CLOSE,
    DEFAULT_WEIGHTS,
    authorize_market_data_feed,
    build_subscription_payload,
    decode_feed_response_quotes,
    load_historical_candles,
    merge_live_candle,
    update_live_state,
)
from scripts.offline_historical_scanner_report import (
    DEFAULT_LABELS_CSV,
    DEFAULT_SQLITE_PATH,
    OfflineScannerResult,
    load_label_map,
    rank_results,
    scan_candles,
    write_report,
)


DEFAULT_OUTPUT_CSV = "reports/continuous-websocket-scanner-ranking.csv"
DEFAULT_OUTPUT_JSON = "reports/continuous-websocket-scanner-results.json"
IST = dt.timezone(dt.timedelta(hours=5, minutes=30))


@dataclass
class DashboardSnapshot:
    status: str = "starting"
    started_at_utc: str = ""
    updated_at_utc: str = ""
    deadline_utc: str = ""
    subscribed_instruments: int = 0
    quote_count: int = 0
    unique_live_instruments: int = 0
    scan_count: int = 0
    last_error: str = ""
    ranked: list[OfflineScannerResult] = field(default_factory=list)


class ContinuousScannerState:
    def __init__(self, snapshot: DashboardSnapshot):
        self._snapshot = snapshot
        self._lock = threading.Lock()

    def update(self, **changes) -> None:
        with self._lock:
            for key, value in changes.items():
                setattr(self._snapshot, key, value)

    def get(self) -> DashboardSnapshot:
        with self._lock:
            return DashboardSnapshot(
                status=self._snapshot.status,
                started_at_utc=self._snapshot.started_at_utc,
                updated_at_utc=self._snapshot.updated_at_utc,
                deadline_utc=self._snapshot.deadline_utc,
                subscribed_instruments=self._snapshot.subscribed_instruments,
                quote_count=self._snapshot.quote_count,
                unique_live_instruments=self._snapshot.unique_live_instruments,
                scan_count=self._snapshot.scan_count,
                last_error=self._snapshot.last_error,
                ranked=list(self._snapshot.ranked),
            )


def utc_now() -> dt.datetime:
    return dt.datetime.now(dt.timezone.utc)


def iso_utc(value: dt.datetime) -> str:
    return value.astimezone(dt.timezone.utc).isoformat(timespec="seconds")


def market_close_deadline(now: dt.datetime, close_time: str) -> dt.datetime:
    hour, minute = (int(part) for part in close_time.split(":", 1))
    local_now = now.astimezone(IST)
    close_local = dt.datetime.combine(local_now.date(), dt.time(hour, minute), tzinfo=IST)
    if local_now >= close_local:
        return now
    return close_local.astimezone(dt.timezone.utc)


def load_instruments_from_summary(path: Path, limit: int = 0) -> list[Instrument]:
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        instruments = [
            Instrument(label=(row.get("label") or row.get("symbol") or row.get("instrument_key") or "").strip(),
                       key=(row.get("instrument_key") or "").strip())
            for row in reader
            if (row.get("instrument_key") or "").strip()
        ]
    return instruments[:limit] if limit > 0 else instruments


def result_to_dict(result: OfflineScannerResult) -> dict:
    return {
        "instrument_key": result.instrument_key,
        "symbol": result.symbol,
        "score": round(result.score, 4),
        "signal_count": result.signal_count,
        "strategies": sorted(result.strategies),
        "latest_close": result.latest_close,
        "latest_rsi": result.latest_rsi,
        "latest_signal_age_candles": result.latest_signal_age_candles,
        "latest_signal_timestamp": result.latest_signal_timestamp,
        "candle_count": result.candle_count,
        "diagnostic": result.diagnostic,
    }


def snapshot_to_dict(snapshot: DashboardSnapshot) -> dict:
    return {
        "status": snapshot.status,
        "started_at_utc": snapshot.started_at_utc,
        "updated_at_utc": snapshot.updated_at_utc,
        "deadline_utc": snapshot.deadline_utc,
        "subscribed_instruments": snapshot.subscribed_instruments,
        "quote_count": snapshot.quote_count,
        "unique_live_instruments": snapshot.unique_live_instruments,
        "scan_count": snapshot.scan_count,
        "last_error": snapshot.last_error,
        "ranked": [result_to_dict(result) for result in snapshot.ranked],
    }


def write_json_snapshot(path: Path, snapshot: DashboardSnapshot) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(snapshot_to_dict(snapshot), indent=2), encoding="utf-8")


def render_html(snapshot: DashboardSnapshot) -> str:
    rows = []
    for index, result in enumerate(snapshot.ranked[:50], start=1):
        rows.append(
            "<tr>"
            f"<td>{index}</td>"
            f"<td>{html.escape(result.symbol or result.instrument_key)}</td>"
            f"<td>{html.escape(result.instrument_key)}</td>"
            f"<td>{result.score:.4f}</td>"
            f"<td>{result.signal_count}</td>"
            f"<td>{html.escape(';'.join(sorted(result.strategies)))}</td>"
            f"<td>{result.latest_close:.2f}</td>"
            f"<td>{'' if result.latest_rsi is None else f'{result.latest_rsi:.2f}'}</td>"
            f"<td>{'' if result.latest_signal_age_candles is None else result.latest_signal_age_candles}</td>"
            f"<td>{html.escape(result.latest_signal_timestamp)}</td>"
            "</tr>"
        )
    return f"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta http-equiv="refresh" content="10">
  <title>TradingBot Live Scanner</title>
  <style>
    body {{ font-family: Segoe UI, Arial, sans-serif; margin: 24px; background: #f8fafc; color: #172033; }}
    h1 {{ margin-bottom: 8px; }}
    .metrics {{ display: grid; grid-template-columns: repeat(4, minmax(160px, 1fr)); gap: 12px; margin: 18px 0; }}
    .metric {{ background: white; border: 1px solid #d7dde8; padding: 12px; border-radius: 6px; }}
    .metric b {{ display: block; font-size: 22px; margin-top: 4px; }}
    table {{ width: 100%; border-collapse: collapse; background: white; border: 1px solid #d7dde8; }}
    th, td {{ padding: 8px 10px; border-bottom: 1px solid #e5e9f0; text-align: left; font-size: 13px; }}
    th {{ background: #eef2f7; }}
    .error {{ color: #a33131; }}
  </style>
</head>
<body>
  <h1>TradingBot Live Scanner</h1>
  <div>Status: <b>{html.escape(snapshot.status)}</b> | Updated: {html.escape(snapshot.updated_at_utc or 'n/a')}</div>
  <div class="metrics">
    <div class="metric">Subscribed<b>{snapshot.subscribed_instruments}</b></div>
    <div class="metric">Quotes<b>{snapshot.quote_count}</b></div>
    <div class="metric">Live Instruments<b>{snapshot.unique_live_instruments}</b></div>
    <div class="metric">Scan Cycles<b>{snapshot.scan_count}</b></div>
  </div>
  <div class="error">{html.escape(snapshot.last_error)}</div>
  <table>
    <thead><tr><th>Rank</th><th>Symbol</th><th>Instrument</th><th>Score</th><th>Signals</th><th>Strategies</th><th>Close</th><th>RSI</th><th>Signal Age</th><th>Signal Time</th></tr></thead>
    <tbody>{''.join(rows)}</tbody>
  </table>
</body>
</html>"""


def has_fresh_live_signal(result: OfflineScannerResult, instrument_key: str, live_states, max_signal_age_candles: int) -> bool:
    if instrument_key not in live_states:
        result.diagnostic = "waiting for live quote"
        return False
    if result.latest_signal_age_candles is None:
        return False
    if result.latest_signal_age_candles > max_signal_age_candles:
        result.diagnostic = f"stale signal age {result.latest_signal_age_candles} candles"
        return False
    return True


def scan_snapshot(
    sqlite_path: Path,
    labels_csv: Path | None,
    instruments: list[Instrument],
    live_states,
    interval: str,
    min_latest_close: float,
    top_n: int,
    max_signal_age_candles: int,
) -> list[OfflineScannerResult]:
    label_map = load_label_map(labels_csv)
    for instrument in instruments:
        label_map.setdefault(instrument.key, instrument.label)
    results: list[OfflineScannerResult] = []
    with contextlib.closing(sqlite3.connect(sqlite_path)) as connection:
        for instrument in instruments:
            historical = load_historical_candles(connection, instrument.key, interval)
            candles = merge_live_candle(historical, live_states.get(instrument.key))
            result = scan_candles(
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
            if has_fresh_live_signal(result, instrument.key, live_states, max_signal_age_candles):
                results.append(result)
    return rank_results(results, minimum_score=0.01, top_n=top_n, include_all=False, min_latest_close=min_latest_close)


def make_handler(state: ContinuousScannerState):
    class DashboardHandler(BaseHTTPRequestHandler):
        def log_message(self, format, *args):  # noqa: A003
            return

        def do_GET(self):  # noqa: N802
            snapshot = state.get()
            if self.path.startswith("/api/results"):
                body = json.dumps(snapshot_to_dict(snapshot), indent=2).encode("utf-8")
                content_type = "application/json"
            else:
                body = render_html(snapshot).encode("utf-8")
                content_type = "text/html; charset=utf-8"
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    return DashboardHandler


def start_server(host: str, port: int, state: ContinuousScannerState) -> ThreadingHTTPServer:
    server = ThreadingHTTPServer((host, port), make_handler(state))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server


def run_continuous(args: argparse.Namespace) -> int:
    try:
        import websocket
    except ImportError:
        print("missing dependency: install websocket-client to run the continuous dashboard", file=sys.stderr)
        return 1

    instruments = load_instruments_from_summary(Path(args.instruments_csv), args.instrument_limit)
    if len(instruments) > args.max_subscription_keys:
        print(
            f"instrument count {len(instruments)} exceeds max subscription keys {args.max_subscription_keys}",
            file=sys.stderr,
        )
        return 2

    started = utc_now()
    deadline = started + dt.timedelta(seconds=args.duration_seconds) if args.duration_seconds > 0 else market_close_deadline(started, args.market_close)
    snapshot = DashboardSnapshot(
        status="starting",
        started_at_utc=iso_utc(started),
        updated_at_utc=iso_utc(started),
        deadline_utc=iso_utc(deadline),
        subscribed_instruments=len(instruments),
    )
    state = ContinuousScannerState(snapshot)
    server = start_server(args.host, args.port, state)
    print(f"dashboard=http://{args.host}:{args.port}/")
    print(f"api=http://{args.host}:{args.port}/api/results")
    print("Read-only continuous scanner: Upstox Market Data Feed V3 only; no order endpoints are called.")

    live_states = {}
    quote_count = 0
    seen = set()
    scan_count = 0
    last_scan = 0.0

    try:
        token = read_access_token(Path(args.token_file))
        uri = authorize_market_data_feed(args.base_url, token)
        ws = websocket.create_connection(uri, timeout=10)
        try:
            ws.send_binary(build_subscription_payload([instrument.key for instrument in instruments], mode=args.mode))
            state.update(status="streaming", updated_at_utc=iso_utc(utc_now()))
            while utc_now() < deadline:
                ws.settimeout(1.0)
                try:
                    message = ws.recv()
                    if not isinstance(message, str):
                        for quote in decode_feed_response_quotes(bytes(message)):
                            quote_count += 1
                            seen.add(quote.instrument_key)
                            update_live_state(live_states, quote)
                except Exception:
                    pass

                now_mono = time.monotonic()
                if now_mono - last_scan >= args.scan_interval_seconds:
                    ranked = scan_snapshot(
                        sqlite_path=Path(args.sqlite_path),
                        labels_csv=Path(args.labels_csv) if args.labels_csv else None,
                        instruments=instruments,
                        live_states=live_states,
                        interval=args.interval,
                        min_latest_close=args.min_latest_close,
                        top_n=args.top_n,
                        max_signal_age_candles=args.max_signal_age_candles,
                    )
                    scan_count += 1
                    updated = iso_utc(utc_now())
                    state.update(
                        status="streaming",
                        updated_at_utc=updated,
                        quote_count=quote_count,
                        unique_live_instruments=len(seen),
                        scan_count=scan_count,
                        ranked=ranked,
                    )
                    current = state.get()
                    write_report(Path(args.output_csv), ranked)
                    write_json_snapshot(Path(args.output_json), current)
                    print(
                        f"{updated} quotes={quote_count} live_keys={len(seen)} scans={scan_count} "
                        f"top={', '.join((item.symbol or item.instrument_key) for item in ranked[:5])}"
                    )
                    last_scan = now_mono
        finally:
            ws.close()
    except Exception as exc:
        state.update(status="error", last_error=str(exc), updated_at_utc=iso_utc(utc_now()))
        write_json_snapshot(Path(args.output_json), state.get())
        print(str(exc), file=sys.stderr)
        return 1
    finally:
        server.shutdown()

    state.update(status="complete", updated_at_utc=iso_utc(utc_now()))
    write_json_snapshot(Path(args.output_json), state.get())
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run continuous all-instrument Upstox websocket scanner dashboard.")
    parser.add_argument("--sqlite-path", default=DEFAULT_SQLITE_PATH)
    parser.add_argument("--instruments-csv", default=DEFAULT_LABELS_CSV)
    parser.add_argument("--labels-csv", default=DEFAULT_LABELS_CSV)
    parser.add_argument("--output-csv", default=DEFAULT_OUTPUT_CSV)
    parser.add_argument("--output-json", default=DEFAULT_OUTPUT_JSON)
    parser.add_argument("--token-file", default="upstox_token.txt")
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument("--interval", default=DEFAULT_INTERVAL)
    parser.add_argument("--mode", default="ltpc", choices=["ltpc", "full"])
    parser.add_argument("--instrument-limit", type=int, default=0, help="0 means all tracked instruments")
    parser.add_argument("--max-subscription-keys", type=int, default=5000)
    parser.add_argument("--scan-interval-seconds", type=float, default=30.0)
    parser.add_argument("--duration-seconds", type=float, default=0.0, help="0 means run until market close")
    parser.add_argument("--market-close", default="15:30", help="HH:MM IST")
    parser.add_argument("--top-n", type=int, default=50)
    parser.add_argument("--min-latest-close", type=float, default=DEFAULT_MIN_LATEST_CLOSE)
    parser.add_argument("--max-signal-age-candles", type=int, default=1, help="Maximum candle age for ranked live signals; 1 allows yesterday's RSI pivot to be confirmed by today's live candle")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    return parser


def validate_args(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    if args.instrument_limit < 0:
        parser.error("--instrument-limit must be non-negative")
    if args.max_subscription_keys <= 0:
        parser.error("--max-subscription-keys must be positive")
    if args.scan_interval_seconds <= 0:
        parser.error("--scan-interval-seconds must be positive")
    if args.duration_seconds < 0:
        parser.error("--duration-seconds must be non-negative")
    if args.top_n <= 0:
        parser.error("--top-n must be positive")
    if args.min_latest_close < 0:
        parser.error("--min-latest-close must be non-negative")
    if args.max_signal_age_candles < 0:
        parser.error("--max-signal-age-candles must be non-negative")
    try:
        market_close_deadline(utc_now(), args.market_close)
    except Exception:
        parser.error("--market-close must use HH:MM format")


def main(argv: Iterable[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(list(argv) if argv is not None else None)
    validate_args(parser, args)
    return run_continuous(args)


if __name__ == "__main__":
    raise SystemExit(main())
