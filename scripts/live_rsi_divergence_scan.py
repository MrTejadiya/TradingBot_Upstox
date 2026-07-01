#!/usr/bin/env python3
"""Read-only Upstox RSI divergence scan for NSE/BSE equity instruments."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_BASE_URL = "https://api.upstox.com"
DEFAULT_TOKEN_FILE = "upstox_token.txt"


@dataclass(frozen=True)
class Candle:
    timestamp: str
    open: float
    high: float
    low: float
    close: float
    volume: int


@dataclass(frozen=True)
class Instrument:
    label: str
    key: str


@dataclass(frozen=True)
class DivergenceResult:
    bullish: bool
    bearish: bool
    latest_close: float
    latest_rsi: float | None
    candle_count: int
    low_pivots: int
    high_pivots: int


def read_access_token(path: Path, env_name: str = "UPSTOX_ACCESS_TOKEN") -> str:
    token = os.environ.get(env_name, "").strip()
    if token:
        return token
    text = path.read_text(encoding="utf-8").strip()
    if not text:
        raise ValueError(f"{path} is empty")

    env_match = re.search(r'SetEnvironmentVariable\(\s*["\']UPSTOX_ACCESS_TOKEN["\']\s*,\s*["\']([^"\']+)["\']', text)
    if env_match:
        return env_match.group(1).strip()
    if "=" in text and "\n" not in text:
        return text.split("=", 1)[1].strip().strip("\"'")
    return text.splitlines()[0].strip().strip("\"'")


def parse_instrument(value: str) -> Instrument:
    if "=" in value:
        label, key = value.split("=", 1)
        return Instrument(label=label.strip(), key=key.strip())
    return Instrument(label=value.strip(), key=value.strip())


def historical_candle_path(instrument_key: str, unit: str, interval: int, from_date: str, to_date: str) -> str:
    encoded = urllib.parse.quote(instrument_key, safe="")
    return f"/v3/historical-candle/{encoded}/{unit}/{interval}/{to_date}/{from_date}"


def parse_candles(body: str) -> list[Candle]:
    payload = json.loads(body)
    if payload.get("status") != "success":
        raise ValueError("historical candle response status is not success")
    rows = payload.get("data", {}).get("candles", [])
    candles: list[Candle] = []
    for row in rows:
        if not isinstance(row, list) or len(row) < 6:
            continue
        candles.append(
            Candle(
                timestamp=str(row[0]),
                open=float(row[1]),
                high=float(row[2]),
                low=float(row[3]),
                close=float(row[4]),
                volume=int(float(row[5])),
            )
        )
    if not candles:
        raise ValueError("historical candle response does not contain parseable OHLCV candles")
    return list(reversed(candles)) if candles[0].timestamp > candles[-1].timestamp else candles


def fetch_candles(base_url: str, token: str, instrument_key: str, unit: str, interval: int, from_date: str, to_date: str) -> list[Candle]:
    url = base_url.rstrip("/") + historical_candle_path(instrument_key, unit, interval, from_date, to_date)
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/json",
            "Authorization": "Bearer " + token,
            "Connection": "close",
            "User-Agent": "TradingBot-Upstox-ReadOnly-RSI-Scan/0.1",
        },
        method="GET",
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return parse_candles(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"HTTP {exc.code}: {detail[:300]}") from exc


def rsi_series(values: list[float], period: int = 14) -> list[float | None]:
    output: list[float | None] = [None] * len(values)
    if period <= 0 or len(values) <= period:
        return output

    average_gain = 0.0
    average_loss = 0.0
    for index in range(1, period + 1):
        change = values[index] - values[index - 1]
        if change >= 0:
            average_gain += change
        else:
            average_loss += -change
    average_gain /= period
    average_loss /= period

    def value() -> float:
        if average_loss == 0:
            return 100.0
        if average_gain == 0:
            return 0.0
        relative_strength = average_gain / average_loss
        return 100.0 - (100.0 / (1.0 + relative_strength))

    output[period] = value()
    for index in range(period + 1, len(values)):
        change = values[index] - values[index - 1]
        gain = max(change, 0.0)
        loss = max(-change, 0.0)
        average_gain = ((average_gain * (period - 1)) + gain) / period
        average_loss = ((average_loss * (period - 1)) + loss) / period
        output[index] = value()
    return output


def pivot_lows(values: list[float], wing_size: int = 1) -> list[int]:
    if wing_size <= 0 or len(values) < (wing_size * 2) + 1:
        return []
    result: list[int] = []
    for index in range(wing_size, len(values) - wing_size):
        if all(values[index] < values[index - offset] and values[index] < values[index + offset] for offset in range(1, wing_size + 1)):
            result.append(index)
    return result


def pivot_highs(values: list[float], wing_size: int = 1) -> list[int]:
    if wing_size <= 0 or len(values) < (wing_size * 2) + 1:
        return []
    result: list[int] = []
    for index in range(wing_size, len(values) - wing_size):
        if all(values[index] > values[index - offset] and values[index] > values[index + offset] for offset in range(1, wing_size + 1)):
            result.append(index)
    return result


def has_bullish_divergence(prices: list[float], oscillator: list[float | None], wing_size: int = 1) -> tuple[bool, int]:
    lows = [index for index in pivot_lows(prices, wing_size) if oscillator[index] is not None]
    if len(lows) < 2:
        return False, len(lows)
    previous, current = lows[-2], lows[-1]
    return prices[current] < prices[previous] and oscillator[current] > oscillator[previous], len(lows)  # type: ignore[operator]


def has_bearish_divergence(prices: list[float], oscillator: list[float | None], wing_size: int = 1) -> tuple[bool, int]:
    highs = [index for index in pivot_highs(prices, wing_size) if oscillator[index] is not None]
    if len(highs) < 2:
        return False, len(highs)
    previous, current = highs[-2], highs[-1]
    return prices[current] > prices[previous] and oscillator[current] < oscillator[previous], len(highs)  # type: ignore[operator]


def analyze_candles(candles: list[Candle], period: int = 14, wing_size: int = 1) -> DivergenceResult:
    closes = [candle.close for candle in candles]
    rsi = rsi_series(closes, period)
    bullish, low_count = has_bullish_divergence(closes, rsi, wing_size)
    bearish, high_count = has_bearish_divergence(closes, rsi, wing_size)
    latest_rsi = next((value for value in reversed(rsi) if value is not None), None)
    return DivergenceResult(
        bullish=bullish,
        bearish=bearish,
        latest_close=closes[-1],
        latest_rsi=latest_rsi,
        candle_count=len(candles),
        low_pivots=low_count,
        high_pivots=high_count,
    )


def default_dates(days: int) -> tuple[str, str]:
    today = dt.date.today()
    return (today - dt.timedelta(days=days)).isoformat(), today.isoformat()


def print_result(label: str, key: str, result: DivergenceResult) -> None:
    latest_rsi = "n/a" if result.latest_rsi is None else f"{result.latest_rsi:.2f}"
    print(
        f"{label} | {key} | candles={result.candle_count} | close={result.latest_close:.2f} | "
        f"rsi={latest_rsi} | bullish_divergence={str(result.bullish).lower()} | "
        f"bearish_divergence={str(result.bearish).lower()} | low_pivots={result.low_pivots} | "
        f"high_pivots={result.high_pivots}"
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Read-only live RSI divergence scan using Upstox historical candles.")
    parser.add_argument("--token-file", default=DEFAULT_TOKEN_FILE)
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument("--instrument", action="append", default=[], help="LABEL=INSTRUMENT_KEY; may be repeated")
    parser.add_argument("--unit", default="days")
    parser.add_argument("--interval", type=int, default=1)
    parser.add_argument("--from-date")
    parser.add_argument("--to-date")
    parser.add_argument("--lookback-days", type=int, default=220)
    parser.add_argument("--rsi-period", type=int, default=14)
    parser.add_argument("--wing-size", type=int, default=1)
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(list(argv) if argv is not None else None)
    from_date, to_date = (args.from_date, args.to_date) if args.from_date and args.to_date else default_dates(args.lookback_days)
    instruments = [parse_instrument(value) for value in args.instrument] or [
        Instrument("RELIANCE_NSE", "NSE_EQ|INE002A01018"),
        Instrument("RELIANCE_BSE", "BSE_EQ|INE002A01018"),
    ]
    token = read_access_token(Path(args.token_file))

    print("Read-only scan: historical candle endpoints only; no order endpoints are called.")
    print(f"Range: {from_date} to {to_date}; unit={args.unit}; interval={args.interval}; rsi_period={args.rsi_period}")
    exit_code = 0
    for instrument in instruments:
        try:
            candles = fetch_candles(args.base_url, token, instrument.key, args.unit, args.interval, from_date, to_date)
            print_result(instrument.label, instrument.key, analyze_candles(candles, args.rsi_period, args.wing_size))
        except Exception as exc:
            exit_code = 1
            print(f"{instrument.label} | {instrument.key} | error={exc}", file=sys.stderr)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
