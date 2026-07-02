#!/usr/bin/env python3
"""Read-only scanner ranking report from local historical candle SQLite data."""

from __future__ import annotations

import argparse
import contextlib
import csv
import sqlite3
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from scripts.live_rsi_divergence_scan import Candle, analyze_candles


DEFAULT_SQLITE_PATH = "data/historical_candles.sqlite3"
DEFAULT_OUTPUT_CSV = "reports/offline-historical-scanner-ranking.csv"
DEFAULT_INTERVAL = "days:1"
DEFAULT_WEIGHTS = {
    "rsi_bullish_divergence": 1.20,
    "macd_bullish_cross": 1.00,
}


@dataclass(frozen=True)
class MacdSnapshot:
    macd: float
    signal: float
    histogram: float


@dataclass
class OfflineScannerResult:
    instrument_key: str
    score: float = 0.0
    signal_count: int = 0
    strategies: list[str] = field(default_factory=list)
    latest_close: float = 0.0
    latest_rsi: float | None = None
    candle_count: int = 0
    bullish_rsi_divergence: bool = False
    bearish_rsi_divergence: bool = False
    macd: MacdSnapshot | None = None
    diagnostic: str = ""


def apply_weight(score: float, strategy: str, weights: dict[str, float]) -> float:
    weight = weights.get(strategy, 1.0)
    return score * (weight if weight > 0 else 1.0)


def ema_series(values: list[float], period: int) -> list[float]:
    if period <= 0 or len(values) < period:
        return []
    seed = sum(values[:period]) / period
    multiplier = 2.0 / (period + 1.0)
    output = [seed]
    ema = seed
    for value in values[period:]:
        ema = ((value - ema) * multiplier) + ema
        output.append(ema)
    return output


def macd_series(values: list[float], fast_period: int = 12, slow_period: int = 26, signal_period: int = 9) -> list[MacdSnapshot]:
    if fast_period <= 0 or slow_period <= 0 or signal_period <= 0 or fast_period >= slow_period:
        return []
    fast = ema_series(values, fast_period)
    slow = ema_series(values, slow_period)
    if not fast or not slow:
        return []
    offset = len(fast) - len(slow)
    macd_values = [fast[index + offset] - slow[index] for index in range(len(slow))]
    signal = ema_series(macd_values, signal_period)
    if not signal:
        return []
    signal_offset = len(macd_values) - len(signal)
    return [
        MacdSnapshot(macd=macd_values[index + signal_offset], signal=signal[index], histogram=macd_values[index + signal_offset] - signal[index])
        for index in range(len(signal))
    ]


def is_bullish_macd_cross(values: list[float], fast_period: int, slow_period: int, signal_period: int) -> tuple[bool, MacdSnapshot | None]:
    snapshots = macd_series(values, fast_period, slow_period, signal_period)
    if len(snapshots) < 2:
        return False, snapshots[-1] if snapshots else None
    previous = snapshots[-2]
    current = snapshots[-1]
    return previous.histogram <= 0.0 and current.histogram > 0.0, current


def load_candles(connection: sqlite3.Connection, instrument_key: str, interval: str) -> list[Candle]:
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
        Candle(
            timestamp=str(row[0]),
            open=float(row[1]),
            high=float(row[2]),
            low=float(row[3]),
            close=float(row[4]),
            volume=int(row[5]),
        )
        for row in rows
    ]


def list_instrument_keys(connection: sqlite3.Connection, interval: str, limit: int = 0) -> list[str]:
    query = "SELECT DISTINCT instrument_key FROM candles WHERE interval = ? ORDER BY instrument_key ASC"
    if limit > 0:
        query += " LIMIT ?"
        rows = connection.execute(query, (interval, limit)).fetchall()
    else:
        rows = connection.execute(query, (interval,)).fetchall()
    return [str(row[0]) for row in rows]


def scan_candles(
    instrument_key: str,
    candles: list[Candle],
    weights: dict[str, float],
    rsi_period: int,
    wing_size: int,
    macd_fast_period: int,
    macd_slow_period: int,
    macd_signal_period: int,
    min_candles: int,
) -> OfflineScannerResult:
    result = OfflineScannerResult(instrument_key=instrument_key, candle_count=len(candles))
    if len(candles) < min_candles:
        result.diagnostic = "insufficient candles"
        return result

    divergence = analyze_candles(candles, period=rsi_period, wing_size=wing_size)
    result.latest_close = divergence.latest_close
    result.latest_rsi = divergence.latest_rsi
    result.bullish_rsi_divergence = divergence.bullish
    result.bearish_rsi_divergence = divergence.bearish

    if divergence.bullish:
        strategy = "rsi_bullish_divergence"
        result.strategies.append(strategy)
        result.score += apply_weight(0.80, strategy, weights)
        result.signal_count += 1

    closes = [candle.close for candle in candles]
    bullish_macd, macd = is_bullish_macd_cross(closes, macd_fast_period, macd_slow_period, macd_signal_period)
    result.macd = macd
    if bullish_macd:
        strategy = "macd_bullish_cross"
        result.strategies.append(strategy)
        result.score += apply_weight(0.70, strategy, weights)
        result.signal_count += 1

    if result.signal_count == 0:
        result.diagnostic = "no bullish scanner signal"
    elif result.bearish_rsi_divergence:
        result.diagnostic = "bullish signal present with bearish RSI divergence warning"
    else:
        result.diagnostic = "ranked"
    return result


def rank_results(results: list[OfflineScannerResult], minimum_score: float, top_n: int, include_all: bool) -> list[OfflineScannerResult]:
    selected = [result for result in results if include_all or result.score >= minimum_score]
    selected.sort(key=lambda item: (-item.score, -item.signal_count, item.instrument_key))
    if top_n > 0:
        selected = selected[:top_n]
    return selected


def write_report(path: Path, results: list[OfflineScannerResult]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(
            [
                "rank",
                "instrument_key",
                "score",
                "signal_count",
                "strategies",
                "latest_close",
                "latest_rsi",
                "candle_count",
                "bullish_rsi_divergence",
                "bearish_rsi_divergence",
                "macd",
                "macd_signal",
                "macd_histogram",
                "diagnostic",
            ]
        )
        for index, result in enumerate(results, start=1):
            macd = result.macd
            writer.writerow(
                [
                    index,
                    result.instrument_key,
                    f"{result.score:.4f}",
                    result.signal_count,
                    ";".join(sorted(result.strategies)),
                    f"{result.latest_close:.4f}" if result.latest_close else "",
                    "" if result.latest_rsi is None else f"{result.latest_rsi:.4f}",
                    result.candle_count,
                    str(result.bullish_rsi_divergence).lower(),
                    str(result.bearish_rsi_divergence).lower(),
                    "" if macd is None else f"{macd.macd:.6f}",
                    "" if macd is None else f"{macd.signal:.6f}",
                    "" if macd is None else f"{macd.histogram:.6f}",
                    result.diagnostic,
                ]
            )


def parse_weights(values: list[str]) -> dict[str, float]:
    weights = dict(DEFAULT_WEIGHTS)
    for value in values:
        if "=" not in value:
            raise ValueError("--strategy-weight must use name=value")
        name, raw_weight = value.split("=", 1)
        name = name.strip()
        if not name:
            raise ValueError("--strategy-weight name must not be empty")
        weight = float(raw_weight)
        if weight <= 0:
            raise ValueError("--strategy-weight value must be positive")
        weights[name] = weight
    return weights


def run_report(
    sqlite_path: Path,
    output_csv: Path,
    interval: str,
    rsi_period: int,
    wing_size: int,
    macd_fast_period: int,
    macd_slow_period: int,
    macd_signal_period: int,
    min_candles: int,
    minimum_score: float,
    top_n: int,
    limit: int,
    weights: dict[str, float],
    include_all: bool = False,
) -> list[OfflineScannerResult]:
    if not sqlite_path.exists():
        raise FileNotFoundError(sqlite_path)
    with contextlib.closing(sqlite3.connect(sqlite_path)) as connection:
        instrument_keys = list_instrument_keys(connection, interval, limit)
        results = [
            scan_candles(
                instrument_key=instrument_key,
                candles=load_candles(connection, instrument_key, interval),
                weights=weights,
                rsi_period=rsi_period,
                wing_size=wing_size,
                macd_fast_period=macd_fast_period,
                macd_slow_period=macd_slow_period,
                macd_signal_period=macd_signal_period,
                min_candles=min_candles,
            )
            for instrument_key in instrument_keys
        ]
    ranked = rank_results(results, minimum_score, top_n, include_all)
    write_report(output_csv, ranked)
    return ranked


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Rank scanner candidates from local historical candle SQLite data.")
    parser.add_argument("--sqlite-path", default=DEFAULT_SQLITE_PATH)
    parser.add_argument("--output-csv", default=DEFAULT_OUTPUT_CSV)
    parser.add_argument("--interval", default=DEFAULT_INTERVAL)
    parser.add_argument("--rsi-period", type=int, default=14)
    parser.add_argument("--wing-size", type=int, default=1)
    parser.add_argument("--macd-fast-period", type=int, default=12)
    parser.add_argument("--macd-slow-period", type=int, default=26)
    parser.add_argument("--macd-signal-period", type=int, default=9)
    parser.add_argument("--min-candles", type=int, default=40)
    parser.add_argument("--minimum-score", type=float, default=0.01)
    parser.add_argument("--top-n", type=int, default=50)
    parser.add_argument("--limit", type=int, default=0, help="Limit instruments read from SQLite for quick checks")
    parser.add_argument("--strategy-weight", action="append", default=[], help="Override scanner weight, for example macd_bullish_cross=1.5")
    parser.add_argument("--include-all", action="store_true", help="Write all scanned instruments, including zero-score rows")
    return parser


def validate_args(parser: argparse.ArgumentParser, args: argparse.Namespace) -> None:
    for name in ("rsi_period", "wing_size", "macd_fast_period", "macd_slow_period", "macd_signal_period", "min_candles"):
        if getattr(args, name) <= 0:
            parser.error(f"--{name.replace('_', '-')} must be a positive integer")
    if args.macd_fast_period >= args.macd_slow_period:
        parser.error("--macd-fast-period must be less than --macd-slow-period")
    if args.minimum_score < 0:
        parser.error("--minimum-score must be non-negative")
    if args.top_n < 0:
        parser.error("--top-n must be non-negative")
    if args.limit < 0:
        parser.error("--limit must be non-negative")
    try:
        parse_weights(args.strategy_weight)
    except ValueError as exc:
        parser.error(str(exc))


def main(argv: Iterable[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(list(argv) if argv is not None else None)
    validate_args(parser, args)
    weights = parse_weights(args.strategy_weight)
    print("Read-only offline scanner: local SQLite input only; no network or order endpoints are called.")
    ranked = run_report(
        sqlite_path=Path(args.sqlite_path),
        output_csv=Path(args.output_csv),
        interval=args.interval,
        rsi_period=args.rsi_period,
        wing_size=args.wing_size,
        macd_fast_period=args.macd_fast_period,
        macd_slow_period=args.macd_slow_period,
        macd_signal_period=args.macd_signal_period,
        min_candles=args.min_candles,
        minimum_score=args.minimum_score,
        top_n=args.top_n,
        limit=args.limit,
        weights=weights,
        include_all=args.include_all,
    )
    print(f"ranked={len(ranked)} output={args.output_csv}")
    for result in ranked[:10]:
        print(
            f"{result.instrument_key} | score={result.score:.4f} | signals={result.signal_count} | "
            f"strategies={','.join(sorted(result.strategies)) or 'none'} | close={result.latest_close:.2f}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
