#!/usr/bin/env python3
"""Read-only Upstox RSI divergence scan for NSE/BSE equity instruments."""

from __future__ import annotations

import argparse
import binascii
import datetime as dt
import json
import os
import re
import struct
import sys
import urllib.error
import urllib.parse
import urllib.request
import zlib
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
    rsi_values: list[float | None]
    price_low_indexes: list[int]
    price_high_indexes: list[int]
    bullish_pair: tuple[int, int] | None = None
    bearish_pair: tuple[int, int] | None = None


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


def exchange_for_key(key: str) -> str:
    if key.startswith("NSE_EQ|"):
        return "NSE_EQ"
    if key.startswith("BSE_EQ|"):
        return "BSE_EQ"
    return ""


def listing_identity(key: str) -> str:
    return key.split("|", 1)[1] if "|" in key else key


def prefer_nse_duplicate_listings(instruments: list[Instrument]) -> list[Instrument]:
    selected: list[Instrument] = []
    index_by_identity: dict[str, int] = {}
    for instrument in instruments:
        exchange = exchange_for_key(instrument.key)
        if exchange not in {"NSE_EQ", "BSE_EQ"}:
            selected.append(instrument)
            continue

        identity = listing_identity(instrument.key)
        existing_index = index_by_identity.get(identity)
        if existing_index is None:
            index_by_identity[identity] = len(selected)
            selected.append(instrument)
            continue

        existing = selected[existing_index]
        if exchange == "NSE_EQ" and exchange_for_key(existing.key) == "BSE_EQ":
            selected[existing_index] = instrument
    return selected


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


def bullish_divergence_pair(prices: list[float], oscillator: list[float | None], wing_size: int = 1) -> tuple[int, int] | None:
    lows = [index for index in pivot_lows(prices, wing_size) if oscillator[index] is not None]
    if len(lows) < 2:
        return None
    previous, current = lows[-2], lows[-1]
    if prices[current] < prices[previous] and oscillator[current] > oscillator[previous]:  # type: ignore[operator]
        return previous, current
    return None


def bearish_divergence_pair(prices: list[float], oscillator: list[float | None], wing_size: int = 1) -> tuple[int, int] | None:
    highs = [index for index in pivot_highs(prices, wing_size) if oscillator[index] is not None]
    if len(highs) < 2:
        return None
    previous, current = highs[-2], highs[-1]
    if prices[current] > prices[previous] and oscillator[current] < oscillator[previous]:  # type: ignore[operator]
        return previous, current
    return None


def has_bullish_divergence(prices: list[float], oscillator: list[float | None], wing_size: int = 1) -> tuple[bool, int]:
    lows = [index for index in pivot_lows(prices, wing_size) if oscillator[index] is not None]
    return bullish_divergence_pair(prices, oscillator, wing_size) is not None, len(lows)


def has_bearish_divergence(prices: list[float], oscillator: list[float | None], wing_size: int = 1) -> tuple[bool, int]:
    highs = [index for index in pivot_highs(prices, wing_size) if oscillator[index] is not None]
    return bearish_divergence_pair(prices, oscillator, wing_size) is not None, len(highs)


def analyze_candles(candles: list[Candle], period: int = 14, wing_size: int = 1) -> DivergenceResult:
    closes = [candle.close for candle in candles]
    rsi = rsi_series(closes, period)
    lows = [index for index in pivot_lows(closes, wing_size) if rsi[index] is not None]
    highs = [index for index in pivot_highs(closes, wing_size) if rsi[index] is not None]
    bullish_pair = bullish_divergence_pair(closes, rsi, wing_size)
    bearish_pair = bearish_divergence_pair(closes, rsi, wing_size)
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
        rsi_values=rsi,
        price_low_indexes=lows,
        price_high_indexes=highs,
        bullish_pair=bullish_pair,
        bearish_pair=bearish_pair,
    )


class PngCanvas:
    def __init__(self, width: int, height: int, background: tuple[int, int, int] = (255, 255, 255)):
        self.width = width
        self.height = height
        self.pixels = bytearray(background * (width * height))

    def set_pixel(self, x: int, y: int, color: tuple[int, int, int]) -> None:
        if 0 <= x < self.width and 0 <= y < self.height:
            offset = ((y * self.width) + x) * 3
            self.pixels[offset : offset + 3] = bytes(color)

    def line(self, x1: int, y1: int, x2: int, y2: int, color: tuple[int, int, int]) -> None:
        dx = abs(x2 - x1)
        sx = 1 if x1 < x2 else -1
        dy = -abs(y2 - y1)
        sy = 1 if y1 < y2 else -1
        error = dx + dy
        while True:
            self.set_pixel(x1, y1, color)
            if x1 == x2 and y1 == y2:
                break
            doubled = 2 * error
            if doubled >= dy:
                error += dy
                x1 += sx
            if doubled <= dx:
                error += dx
                y1 += sy

    def rect(self, x1: int, y1: int, x2: int, y2: int, color: tuple[int, int, int]) -> None:
        self.line(x1, y1, x2, y1, color)
        self.line(x2, y1, x2, y2, color)
        self.line(x2, y2, x1, y2, color)
        self.line(x1, y2, x1, y1, color)

    def circle(self, cx: int, cy: int, radius: int, color: tuple[int, int, int]) -> None:
        for y in range(cy - radius, cy + radius + 1):
            for x in range(cx - radius, cx + radius + 1):
                if ((x - cx) ** 2) + ((y - cy) ** 2) <= radius * radius:
                    self.set_pixel(x, y, color)

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        raw = bytearray()
        stride = self.width * 3
        for y in range(self.height):
            raw.append(0)
            raw.extend(self.pixels[y * stride : (y + 1) * stride])

        def chunk(name: bytes, data: bytes) -> bytes:
            return struct.pack(">I", len(data)) + name + data + struct.pack(">I", binascii.crc32(name + data) & 0xFFFFFFFF)

        png = b"\x89PNG\r\n\x1a\n"
        png += chunk("IHDR".encode(), struct.pack(">IIBBBBB", self.width, self.height, 8, 2, 0, 0, 0))
        png += chunk("IDAT".encode(), zlib.compress(bytes(raw), 9))
        png += chunk("IEND".encode(), b"")
        path.write_bytes(png)


def scale_points(values: list[float], left: int, top: int, width: int, height: int) -> list[tuple[int, int]]:
    low = min(values)
    high = max(values)
    spread = high - low if high != low else 1.0
    points: list[tuple[int, int]] = []
    for index, value in enumerate(values):
        x = left + round((index / max(len(values) - 1, 1)) * width)
        y = top + height - round(((value - low) / spread) * height)
        points.append((x, y))
    return points


def draw_polyline(canvas: PngCanvas, points: list[tuple[int, int]], color: tuple[int, int, int]) -> None:
    for start, end in zip(points, points[1:]):
        canvas.line(start[0], start[1], end[0], end[1], color)


def draw_thick_line(canvas: PngCanvas, start: tuple[int, int], end: tuple[int, int], color: tuple[int, int, int]) -> None:
    for offset in (-1, 0, 1):
        canvas.line(start[0], start[1] + offset, end[0], end[1] + offset, color)
        canvas.line(start[0] + offset, start[1], end[0] + offset, end[1], color)


def chart_filename(label: str, key: str) -> str:
    safe = re.sub(r"[^A-Za-z0-9_.-]+", "_", f"{label}_{key}")
    return safe.strip("_") + ".png"


def write_chart(path: Path, candles: list[Candle], result: DivergenceResult) -> None:
    closes = [candle.close for candle in candles]
    rsi_values = [value if value is not None else 50.0 for value in result.rsi_values]
    canvas = PngCanvas(1000, 620)
    margin_left = 60
    plot_width = 890
    price_top = 40
    price_height = 300
    rsi_top = 400
    rsi_height = 160
    axis = (80, 80, 80)
    price_color = (30, 94, 160)
    rsi_color = (120, 60, 150)
    low_color = (20, 150, 80)
    high_color = (200, 70, 55)
    divergence_color = (230, 120, 20)

    canvas.rect(margin_left, price_top, margin_left + plot_width, price_top + price_height, axis)
    canvas.rect(margin_left, rsi_top, margin_left + plot_width, rsi_top + rsi_height, axis)
    for level in (30, 70):
        y = rsi_top + rsi_height - round((level / 100.0) * rsi_height)
        canvas.line(margin_left, y, margin_left + plot_width, y, (210, 210, 210))

    price_points = scale_points(closes, margin_left, price_top, plot_width, price_height)
    rsi_points = scale_points(rsi_values, margin_left, rsi_top, plot_width, rsi_height)
    draw_polyline(canvas, price_points, price_color)
    draw_polyline(canvas, rsi_points, rsi_color)

    for index in result.price_low_indexes:
        canvas.circle(*price_points[index], 5, low_color)
        canvas.circle(*rsi_points[index], 4, low_color)
    for index in result.price_high_indexes:
        canvas.circle(*price_points[index], 5, high_color)
        canvas.circle(*rsi_points[index], 4, high_color)

    for pair in (result.bullish_pair, result.bearish_pair):
        if pair:
            a, b = pair
            draw_thick_line(canvas, price_points[a], price_points[b], divergence_color)
            draw_thick_line(canvas, rsi_points[a], rsi_points[b], divergence_color)
            canvas.circle(*price_points[a], 7, divergence_color)
            canvas.circle(*price_points[b], 7, divergence_color)
            canvas.circle(*rsi_points[a], 6, divergence_color)
            canvas.circle(*rsi_points[b], 6, divergence_color)

    canvas.save(path)


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
    parser.add_argument("--output-dir", help="Write PNG evidence charts to this directory")
    return parser


def validate_args(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    for name in ("interval", "lookback_days", "rsi_period", "wing_size"):
        if getattr(args, name) <= 0:
            parser.error(f"--{name.replace('_', '-')} must be a positive integer")
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
    instruments = prefer_nse_duplicate_listings([parse_instrument(value) for value in args.instrument] or [
        Instrument("RELIANCE_NSE", "NSE_EQ|INE002A01018"),
        Instrument("RELIANCE_BSE", "BSE_EQ|INE002A01018"),
    ])
    token = read_access_token(Path(args.token_file))

    print("Read-only scan: historical candle endpoints only; no order endpoints are called.")
    print(f"Range: {from_date} to {to_date}; unit={args.unit}; interval={args.interval}; rsi_period={args.rsi_period}")
    exit_code = 0
    for instrument in instruments:
        try:
            candles = fetch_candles(args.base_url, token, instrument.key, args.unit, args.interval, from_date, to_date)
            result = analyze_candles(candles, args.rsi_period, args.wing_size)
            print_result(instrument.label, instrument.key, result)
            if args.output_dir:
                chart_path = Path(args.output_dir) / chart_filename(instrument.label, instrument.key)
                write_chart(chart_path, candles, result)
                print(f"chart={chart_path}")
        except Exception as exc:
            exit_code = 1
            print(f"{instrument.label} | {instrument.key} | error={exc}", file=sys.stderr)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
