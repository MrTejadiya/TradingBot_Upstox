# Historical Download Upstox JSON Source Plan

## Requirement

Use the Upstox complete instrument JSON URL as a direct source for historical
candle bulk downloads:

```text
https://assets.upstox.com/market-quote/instruments/exchange/complete.json.gz
```

## Observed Source Shape

The URL currently returns gzip-compressed JSON. A probe on 2026-07-02 parsed
124,416 total records and 2,399 `NSE_EQ`/`BSE_EQ` records with
`instrument_type=EQ`.

The final count can change daily because this is an Upstox beginning-of-day
instrument universe file.

## Scope

Extend `scripts/download_historical_candles.py` so it can load instruments
from either:

- the existing manual instrument CSV, or
- an Upstox JSON file/URL, including `.gz` content

The JSON source must:

- import only `segment` values `NSE_EQ` and `BSE_EQ`
- import only `instrument_type=EQ`
- require non-empty `instrument_key`
- use `trading_symbol`, then `short_name`, then `name`, then key as label
- preserve NSE-over-BSE duplicate preference
- keep the existing read-only historical candle endpoint behavior
- keep CSV support unchanged

## Rate And Runtime Impact

Downloading historical candles for roughly 2,399 instruments means roughly one
historical candle request per instrument per interval/date range. With the
default `--throttle-seconds 0.12`, this is about 8.3 requests/second before API
latency, under Upstox standard API limits documented for non-order endpoints.

The downloader should support `--limit` so operators can trial a small subset
before running the full universe.

## Implementation Impact

1. Add JSON source loading helpers in the downloader.
   - Read local JSON or URL.
   - Decompress gzip when needed.
   - Parse and filter Upstox instruments.
   - Reuse existing duplicate preference helper.

2. CLI changes.
   - Keep `--instruments-csv`.
   - Add `--upstox-instruments-json`.
   - Add `--upstox-instruments-url`.
   - Require exactly one source.

3. Tests.
   - Plain JSON source.
   - Gzip JSON source.
   - Equity filtering.
   - NSE-over-BSE duplicate preference.
   - Fake fetch bulk run using JSON source.
   - Argument validation for exactly one source.

4. Docs.
   - README command using the provided Upstox URL.
   - Note expected instrument count can change daily.
   - Recommend `--limit` trial before full download.

## Jira Breakdown

- `RK9311-193`: parent task.
- `RK9311-194`: planning.
- `RK9311-195`: implementation.
- `RK9311-196`: tests.
- `RK9311-197`: docs.

## Safety Notes

This work downloads historical market data only. It must not call order
placement, modification, cancellation, GTT, or portfolio mutation endpoints.
