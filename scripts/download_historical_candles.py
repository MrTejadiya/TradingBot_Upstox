#!/usr/bin/env python3
"""Bulk download Upstox historical candles for instruments in a CSV file."""

from __future__ import annotations

import argparse
import contextlib
import csv
import datetime as dt
import sqlite3
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from scripts.live_rsi_divergence_scan import (
    DEFAULT_BASE_URL,
    DEFAULT_TOKEN_FILE,
    Candle,
    Instrument,
    fetch_candles,
    prefer_nse_duplicate_listings,
    read_access_token,
)


DEFAULT_RUN_ID = "historical-download"


@dataclass(frozen=True)
class DownloadResult:
    label: str
    instrument_key: str
    ok: bool
    candle_count: int = 0
    error: str = ""


FetchCandles = Callable[[str, str, str, str, int, str, str], list[Candle]]


def parse_bool(value: str) -> bool:
    return value.strip().lower() in {"1", "true", "yes", "y"}


def load_instruments_csv(path: Path, enabled_only: bool = True) -> list[Instrument]:
    with path.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        if reader.fieldnames is None:
            raise ValueError("instrument CSV is empty")
        headers = {name.strip().lower() for name in reader.fieldnames}
        for required in ("instrument_key", "symbol"):
            if required not in headers:
                raise ValueError(f"instrument CSV must contain {required} column")

        instruments: list[Instrument] = []
        for index, row in enumerate(reader, start=2):
            normalized = {str(key).strip().lower(): (value or "").strip() for key, value in row.items()}
            key = normalized.get("instrument_key", "")
            symbol = normalized.get("symbol", "") or key
            if not key:
                raise ValueError(f"row {index}: instrument_key is mandatory")
            if enabled_only and "enabled" in normalized and not parse_bool(normalized.get("enabled", "")):
                continue
            instruments.append(Instrument(label=symbol, key=key))
    return prefer_nse_duplicate_listings(instruments)


def apply_schema(connection: sqlite3.Connection) -> None:
    connection.executescript(
        """
        CREATE TABLE IF NOT EXISTS candles (
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
        CREATE UNIQUE INDEX IF NOT EXISTS idx_candles_instrument_interval_time
            ON candles(instrument_key, interval, candle_at);
        """
    )


def interval_name(unit: str, interval: int) -> str:
    return f"{unit}:{interval}"


def upsert_candles(
    connection: sqlite3.Connection,
    run_id: str,
    instrument_key: str,
    unit: str,
    interval: int,
    candles: list[Candle],
) -> None:
    rows = [
        (
            run_id,
            instrument_key,
            interval_name(unit, interval),
            candle.timestamp,
            candle.open,
            candle.high,
            candle.low,
            candle.close,
            candle.volume,
        )
        for candle in candles
    ]
    connection.executemany(
        """
        INSERT OR REPLACE INTO candles(
            run_id, instrument_key, interval, candle_at, open, high, low, close, volume
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        rows,
    )
    connection.commit()


def write_summary(path: Path, results: list[DownloadResult]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["label", "instrument_key", "status", "candle_count", "error"])
        for result in results:
            writer.writerow(
                [
                    result.label,
                    result.instrument_key,
                    "ok" if result.ok else "error",
                    result.candle_count,
                    result.error,
                ]
            )


def download_all(
    instruments: list[Instrument],
    database_path: Path,
    base_url: str,
    token: str,
    unit: str,
    interval: int,
    from_date: str,
    to_date: str,
    run_id: str = DEFAULT_RUN_ID,
    throttle_seconds: float = 0.0,
    limit: int = 0,
    fetch: FetchCandles = fetch_candles,
) -> list[DownloadResult]:
    selected = instruments[:limit] if limit > 0 else instruments
    database_path.parent.mkdir(parents=True, exist_ok=True)
    results: list[DownloadResult] = []
    with contextlib.closing(sqlite3.connect(database_path)) as connection:
        apply_schema(connection)
        for index, instrument in enumerate(selected):
            try:
                candles = fetch(base_url, token, instrument.key, unit, interval, from_date, to_date)
                upsert_candles(connection, run_id, instrument.key, unit, interval, candles)
                results.append(
                    DownloadResult(
                        label=instrument.label,
                        instrument_key=instrument.key,
                        ok=True,
                        candle_count=len(candles),
                    )
                )
            except Exception as exc:
                results.append(
                    DownloadResult(label=instrument.label, instrument_key=instrument.key, ok=False, error=str(exc))
                )
            if throttle_seconds > 0 and index + 1 < len(selected):
                time.sleep(throttle_seconds)
    return results


def default_dates(days: int) -> tuple[str, str]:
    today = dt.date.today()
    return (today - dt.timedelta(days=days)).isoformat(), today.isoformat()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Download historical candles for all instruments in a CSV file.")
    parser.add_argument("--instruments-csv", required=True)
    parser.add_argument("--sqlite-path", default="data/historical_candles.sqlite3")
    parser.add_argument("--summary-csv", default="reports/historical-candle-download-summary.csv")
    parser.add_argument("--token-file", default=DEFAULT_TOKEN_FILE)
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument("--unit", default="days")
    parser.add_argument("--interval", type=int, default=1)
    parser.add_argument("--from-date")
    parser.add_argument("--to-date")
    parser.add_argument("--lookback-days", type=int, default=365)
    parser.add_argument("--run-id", default=DEFAULT_RUN_ID)
    parser.add_argument("--throttle-seconds", type=float, default=0.12)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--include-disabled", action="store_true")
    return parser


def validate_args(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    if args.interval <= 0:
        parser.error("--interval must be a positive integer")
    if args.lookback_days <= 0:
        parser.error("--lookback-days must be a positive integer")
    if args.throttle_seconds < 0:
        parser.error("--throttle-seconds must be non-negative")
    if args.limit < 0:
        parser.error("--limit must be non-negative")
    if bool(args.from_date) != bool(args.to_date):
        parser.error("--from-date and --to-date must be provided together")
    if args.from_date and args.to_date:
        try:
            from_date = dt.date.fromisoformat(args.from_date)
            to_date = dt.date.fromisoformat(args.to_date)
        except ValueError:
            parser.error("--from-date and --to-date must use YYYY-MM-DD format")
        if from_date > to_date:
            parser.error("--from-date must be on or before --to-date")


def main(argv: Iterable[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(list(argv) if argv is not None else None)
    validate_args(parser, args)
    from_date, to_date = (args.from_date, args.to_date) if args.from_date and args.to_date else default_dates(args.lookback_days)
    instruments = load_instruments_csv(Path(args.instruments_csv), enabled_only=not args.include_disabled)
    token = read_access_token(Path(args.token_file))

    print("Read-only download: historical candle endpoints only; no order endpoints are called.")
    print(f"instruments={len(instruments)} range={from_date}..{to_date} unit={args.unit} interval={args.interval}")
    results = download_all(
        instruments=instruments,
        database_path=Path(args.sqlite_path),
        base_url=args.base_url,
        token=token,
        unit=args.unit,
        interval=args.interval,
        from_date=from_date,
        to_date=to_date,
        run_id=args.run_id,
        throttle_seconds=args.throttle_seconds,
        limit=args.limit,
    )
    write_summary(Path(args.summary_csv), results)
    ok_count = sum(1 for result in results if result.ok)
    error_count = len(results) - ok_count
    print(f"completed ok={ok_count} errors={error_count} sqlite={args.sqlite_path} summary={args.summary_csv}")
    for result in results:
        status = "ok" if result.ok else f"error={result.error}"
        print(f"{result.label} | {result.instrument_key} | candles={result.candle_count} | {status}")
    return 0 if error_count == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
