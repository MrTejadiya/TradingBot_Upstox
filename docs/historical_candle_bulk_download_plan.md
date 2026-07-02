# Historical Candle Bulk Download Plan

## Requirement

Download historical candle data for all companies/instruments present in the
instrument CSV file and store it locally for scanner/backtest use. This workflow
must remain read-only against Upstox order APIs.

## Current Constraint

No `instruments.csv` file is currently present in the workspace. The
implementation will add a reusable downloader and tests now. The actual
download run requires the operator to provide the CSV path or place the file in
the configured location.

## Scope

Version 1 will add a Python command-line downloader that:

- reads the existing instrument CSV format
- applies the same NSE-over-BSE duplicate preference
- fetches Upstox historical candles for each instrument
- stores candles into the existing SQLite `candles` table
- creates the required SQLite schema if the database is new
- resumes safely with `INSERT OR REPLACE`
- writes a CSV summary report per instrument
- supports date range, unit, interval, base URL, token file, throttle delay, and
  limit options
- calls only historical candle endpoints; no order endpoints

## Implementation Impact

1. Add `scripts/download_historical_candles.py`.
   - Reuse token loading, candle path, candle parsing, fetch, and duplicate
     preference helpers from `scripts/live_rsi_divergence_scan.py`.
   - Parse the C++ instrument CSV headers.
   - Persist to SQLite compatible with the app migrations.
   - Emit summary rows: label, key, status, candle count, cache/database path,
     and error.

2. Add tests.
   - CSV parsing and invalid/missing header behavior.
   - NSE-over-BSE preference.
   - SQLite schema creation and candle upsert.
   - Bulk workflow with fake fetch function.
   - Failure summary for one instrument while continuing with others.

3. Docs.
   - README usage example.
   - Explicit read-only/no-order note.
   - Note that the actual run requires an instrument CSV path.

## Jira Breakdown

- `RK9311-188`: parent task, historical candle bulk download.
- `RK9311-189`: planning and impact analysis.
- `RK9311-190`: downloader implementation.
- `RK9311-191`: downloader tests.
- `RK9311-192`: documentation and operator guidance.

## Verification

Each implementation subtask will run focused tests plus:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run_tests.ps1
```
