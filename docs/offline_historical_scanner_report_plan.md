# Offline Historical Scanner Report Plan

## Jira

- `RK9311-198`: parent task, offline scanner ranking from SQLite historical candles.
- `RK9311-199`: planning and impact analysis.
- `RK9311-200`: implementation.
- `RK9311-201`: tests.
- `RK9311-202`: documentation.
- `RK9311-203`: review, verification, and commit.

## Requirement

Use the downloaded historical candle cache to run scanners offline, rank the
highest scoring buy candidates, and write a reviewable report without touching
any Upstox order endpoint.

## Impact

- Add a script under `scripts/` that reads the existing `candles` SQLite table.
- Reuse the existing RSI divergence implementation from
  `scripts/live_rsi_divergence_scan.py`.
- Add a local MACD crossover check so offline ranking can combine multiple
  scanner signals.
- Keep the workflow read-only: no token loading, no network calls, and no order
  endpoint usage.
- Add tests with a temporary SQLite database so the report behavior is
  deterministic and safe.
- Document the operator command in `README.md`.

## Output

The report writes CSV rows with:

- rank and instrument key
- combined score
- contributing scanner names
- latest close and RSI values
- candle count and diagnostics
- RSI divergence and MACD details for review

## Verification

- Python unit tests for SQLite loading, RSI/MACD scoring, ranking, CSV output,
  argument validation, and no-signal behavior.
- Full project verification through `scripts/run_tests.ps1`.

## Safety

This workflow reads only local SQLite data and writes a local CSV report. It
does not read broker credentials, does not call Upstox APIs, and cannot place or
cancel orders.
