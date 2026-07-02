# Continuous Scanner Fresh Signal Gating Plan

## Jira

- `RK9311-230`: parent bug, fix continuous scanner stale historical opportunities.
- `RK9311-231`: planning and acceptance criteria.
- `RK9311-232`: fresh live signal gating implementation.
- `RK9311-233`: dashboard/API freshness metadata.
- `RK9311-234`: tests.
- `RK9311-235`: verification, docs, commit, and Jira evidence.

## Problem

The continuous websocket dashboard appended the current live candle before
ranking, but the scanner result only exposed whether a strategy was present, not
which candle produced that signal. A divergence detected on older historical
candles could therefore remain ranked after live prices arrived.

## Implementation

- Record the triggering candle index and timestamp for supported scanner
  strategies.
- Add `latest_signal_age_candles` and `latest_signal_timestamp` to CSV, JSON,
  and dashboard output.
- Require a live quote for the instrument before it can be ranked by the
  continuous dashboard.
- Filter ranked live opportunities by `--max-signal-age-candles`, defaulting to
  `1`.

## Freshness Semantics

- MACD bullish cross triggers on the latest candle, so its live age is `0`.
- RSI pivot divergence needs a right-side candle to confirm the pivot, so the
  default allows age `1`: yesterday's pivot can be confirmed by today's live
  candle.
- Anything older than the configured age is treated as stale and is excluded
  from the dashboard opportunity list.
