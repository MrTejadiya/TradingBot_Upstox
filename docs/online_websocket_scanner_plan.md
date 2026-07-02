# Online Websocket Scanner Plan

## Jira

- `RK9311-218`: parent task, run online Upstox websocket scanner.
- `RK9311-219`: planning and impact analysis.
- `RK9311-220`: implementation.
- `RK9311-221`: tests.
- `RK9311-222`: documentation.
- `RK9311-223`: bounded live run, evidence, commit, and push.

## Requirement

Run the scanner online with Upstox Market Data Feed V3 websocket ticks, not the
offline SQLite-only report.

## Current State

The C++ code has a websocket-ready `MarketFeed` abstraction and live scanner
engine, but it does not yet open an Upstox websocket connection from the CLI.
The existing offline report ranks candidates from local SQLite only.

## Upstox V3 Feed Notes

- Authorize with `GET /v3/feed/market-data-feed/authorize`.
- Use the returned one-time `authorized_redirect_uri` websocket URL.
- Subscribe with a binary request payload containing `guid`, `method`, `mode`,
  and `instrumentKeys`.
- Decode incoming binary messages using the official Market Data V3 protobuf.
- LTPC mode supports up to 5,000 keys for a single category according to the
  public documentation, but this runner will default to a smaller bounded list
  for safer testing.

## Implementation Scope

- Add `scripts/live_websocket_scanner.py`.
- Use local historical candles from `data/historical_candles.sqlite3` as the
  closed-candle base.
- Stream websocket LTPC ticks into a provisional current-day candle.
- Reuse the existing Python RSI divergence, MACD, ranking, label, CSV, and PNG
  report helpers where practical.
- Write ranked evidence to CSV and optional PNG charts.
- Keep the runner read-only: no order endpoints, no order payloads, no broker
  order mutations.

## Verification

- Unit tests for subscription payload, protobuf decoding, candle aggregation,
  ranking output, and argument validation.
- Bounded live run with a small instrument list and short duration.

## Safety

The runner uses only the market data websocket and the market data authorize
REST endpoint. It must not call Upstox order placement, order cancellation, or
portfolio mutation endpoints.
