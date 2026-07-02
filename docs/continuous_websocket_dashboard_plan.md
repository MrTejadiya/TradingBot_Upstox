# Continuous Websocket Dashboard Plan

## Jira

- `RK9311-224`: parent task, continuous all-instrument websocket scanner dashboard.
- `RK9311-225`: planning and impact analysis.
- `RK9311-226`: implementation.
- `RK9311-227`: tests.
- `RK9311-228`: documentation.
- `RK9311-229`: run evidence, commit, and push.

## Requirement

Track all instrument keys continuously until market close and show scanner
results through a local server or console.

## Scope

- Add a read-only dashboard runner under `scripts/`.
- Load all tracked instruments from `reports/historical-candle-download-summary.csv`.
- Subscribe to Upstox Market Data Feed V3 in `ltpc` mode.
- Keep a provisional live daily candle per instrument from websocket ticks.
- Periodically scan all instruments against local historical candles plus live
  candles.
- Write CSV/JSON snapshots and expose them through a local HTTP dashboard.
- Print a concise console top-list on each scan cycle.

## Limits

The public Upstox V3 documentation lists an LTPC individual subscription limit
of 5,000 instrument keys. The current equity universe has 2,399 keys, so a
single LTPC subscription is within that limit.

## Safety

The dashboard uses only local SQLite/CSV files and the Upstox Market Data Feed
V3 websocket authorize/feed endpoints. It does not call order placement,
cancellation, or portfolio mutation endpoints.
