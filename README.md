# TradingBot Upstox

C++ Upstox delivery trading bot scaffold based on the SRS in `upstox_cpp_trading_bot_srs.pdf`.

## Build

```powershell
cmake -S . -B build -G "Ninja"
cmake --build build
ctest --test-dir build --output-on-failure
```

If Ninja is not available, omit `-G "Ninja"` and let CMake choose a local generator.

To run the full local verification set, including the Python scanner tests:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run_tests.ps1
```

## CLI

```powershell
.\build\tradingbot_upstox.exe --mode validate
.\build\tradingbot_upstox.exe --mode dry-run
.\build\tradingbot_upstox.exe --mode paper
.\build\tradingbot_upstox.exe --mode show-orders
.\build\tradingbot_upstox.exe --config config.example.json --mode show-orders
.\build\tradingbot_upstox.exe --config config.example.json --mode show-orders --limit 10
```

`dry-run` is the default mode. `paper` uses local simulator/backtest components
without broker order placement. `live` requires explicit live-trading gates.
Pass `--config <path>` to load JSON configuration; explicit CLI flags such as
`--mode` override config defaults.
Pass `--limit <count>` with `show-orders` to print only the first N loaded
history rows.

## Configuration

Version 1 uses JSON configuration. The loader validates these required sections:
`app`, `upstox`, `input`, `market_data`, `strategies`, `exit_rules`, `risk`,
`rate_limits`, `storage`, and `logging`.

`live_scanner` is optional and controls the read-only live scanner pipeline.
Set `worker_count` to `0` to use the scanner's CPU-core default. Set
`partition_count` to `0` to use the engine default partitioning. RSI scanning
uses `rsi_period` and `wing_size`; MACD scanning uses `macd_fast_period`,
`macd_slow_period`, and `macd_signal_period`. `minimum_score` is the fail-closed
candidate threshold, `top_n` limits ranked candidates when greater than zero,
and `strategy_weights` ranks scanner signals such as `rsi_divergence` and
`macd_bullish_cross`. The scanner runtime persists reviewable signals and
selected decisions only; it does not place broker orders.

`exit_rules.max_holding_duration_hours` configures the maximum holding-period
exit rule. Set it to `0` only when that exit should be disabled by the caller.

`market_data.max_quote_age_seconds` configures the quote freshness window used
by strategy and exit-engine price checks. Timestamp-less legacy quotes retain
the default behavior.

`risk.max_order_value` caps the value of any individual buy or sell decision
before order dispatch.

`risk.max_orders_per_day` caps the number of order decisions accepted for the
trading day when the caller supplies the current daily order count.

`risk.max_daily_traded_value` caps the projected total traded value for the
trading day when the caller supplies the current daily traded value.

Start from `config.example.json` and provide access tokens through the configured
environment variable, not in the config file.

`input.instrument_source` selects the configured instrument universe source.
Use `csv` for a manually curated CSV file or `upstox_json` for a local copy of
the Upstox complete instrument JSON file. When source paths are relative,
configured app runs resolve them relative to the config file location. For
SQLite-backed non-reporting modes, the app loads the configured source before
the mode runs and upserts normalized instrument rows into SQLite.

Set `upstox.force_ipv4=true` for live/order API traffic when the Upstox static
IP allowlist is configured with IPv4 addresses. The RK9311-29 broker test showed
that IPv6 egress can be rejected even when the configured IPv4 static IP is
valid.

The credential loader fails closed when the configured token environment variable
is missing. Logs and metadata should pass secrets through the redaction helpers
before output.

## Instrument CSV

The CSV loader reads `instrument_key` as the canonical identifier. `symbol` is
display metadata only. Minimum supported columns are `instrument_key`, `symbol`,
`enabled`, `quantity`, `max_position_qty`, and `target_profit_pct`.
Exact duplicate `instrument_key` rows are invalid. When an NSE and BSE listing
share the same equity identity, such as `NSE_EQ|INE...` and `BSE_EQ|INE...`,
the loader keeps the NSE row and drops the BSE duplicate. A BSE row is retained
when no matching NSE row is present.

## Upstox Instrument JSON

Upstox publishes beginning-of-day instrument files, including a complete JSON
file, in the Instruments API documentation:
https://upstox.com/developer/api-documentation/instruments/

To use the full Upstox universe, either download the complete JSON file locally
or configure the app to download it into a local cache before parsing.

Local-file mode:

```json
"input": {
  "instrument_source": "upstox_json",
  "upstox_instruments_json": "data/upstox_complete.json",
  "default_enabled": true,
  "default_quantity": 1,
  "default_max_position_qty": 1,
  "default_target_profit_pct": 10.0,
  "default_strategy_profile": "",
  "default_notes": "imported from Upstox instrument JSON"
}
```

URL/cache mode:

```json
"input": {
  "instrument_source": "upstox_json",
  "upstox_instruments_url": "https://assets.upstox.com/market-quote/instruments/exchange/complete.json.gz",
  "upstox_instruments_cache": "data/upstox_complete.cache.json",
  "refresh_upstox_instruments": true,
  "allow_stale_upstox_instruments_cache": true,
  "default_enabled": true,
  "default_quantity": 1,
  "default_max_position_qty": 1,
  "default_target_profit_pct": 10.0,
  "default_strategy_profile": "",
  "default_notes": "imported from Upstox instrument JSON"
}
```

When `upstox_instruments_url` is set, `upstox_instruments_cache` is required.
If `refresh_upstox_instruments=false`, startup reads an existing cache first and
does not call the network. If `refresh_upstox_instruments=true`, startup tries
to download the URL, writes the cache, then parses that content. When
`allow_stale_upstox_instruments_cache=true`, a failed download can fall back to
an existing cache. If no usable local/cache JSON is available, startup fails
before creating a bot-run row or importing partial instruments.

After a successful URL refresh, the app writes a metadata sidecar beside the
cache file. For `data/upstox_complete.cache.json`, the metadata file is
`data/upstox_complete.cache.json.metadata.json`. It records `source_url`,
`cache_path`, `refreshed_at_utc`, `status_code`, and `bytes`. Review this file
before any live run that depends on URL/cache mode. Cache-hit startup reads do
not rewrite the metadata file.

The JSON importer currently accepts only equity delivery candidates where
`segment` is `NSE_EQ` or `BSE_EQ` and `instrument_type` is `EQ`. Unsupported
records, such as FO, MCX, indexes, mutual funds, or other non-equity records,
are skipped. Exact duplicate `instrument_key` values are rejected. If the same
equity identity is listed on both NSE and BSE, the importer keeps the NSE key
and drops the BSE duplicate; BSE is retained only when no matching NSE listing
is present.

The configured `default_*` fields generate the trading controls for imported
records because the Upstox instrument file does not contain bot-specific
quantity, max-position, target, profile, or notes settings. Review these values
carefully before any live-trading run.

## Logging

Structured log records are emitted as single-line JSON-like entries with secret
redaction enabled by default. Daily file rotation is represented by date-based
log file names such as `tradingbot-20260630.log`.

## Upstox API Client

The initial REST client is transport-agnostic. It applies bearer authentication,
records redacted request metadata, and retries transient HTTP statuses through a
bounded retry policy.

The market quote service fetches LTP data through Upstox API V3 at
`/v3/market-quote/ltp?instrument_key=...` and maps `last_price` into a
`QuoteSnapshot`.

Historical candles are fetched through the V3 historical candle endpoint and
stored through a cache interface. The current implementation includes both an
in-memory cache for tests and a SQLite-backed cache for persisted candle
history.

The market feed layer currently provides a websocket-ready abstraction for LTP
subscription payloads, quote message parsing, and disconnect notifications.

## Read-Only Scanner Evidence

Use `scripts/live_rsi_divergence_scan.py` to fetch historical candles and render
RSI divergence evidence charts without touching any order endpoint:

```powershell
python scripts\live_rsi_divergence_scan.py `
  --token-file upstox_token.txt `
  --instrument RELIANCE_NSE=NSE_EQ|INE002A01018 `
  --from-date 2026-01-01 `
  --to-date 2026-07-01 `
  --output-dir reports\rsi-divergence
```

The script reads `UPSTOX_ACCESS_TOKEN` first and falls back to the configured
token file. If explicit dates are omitted, it uses `--lookback-days` to build a
date range. `--output-dir` writes PNG charts that mark price pivots, RSI pivots,
and detected divergence pairs. The script validates date ranges and scanner
parameters before token loading or network calls, and it only calls Upstox
historical candle APIs. If both NSE and BSE keys are supplied for the same
equity identity, the script scans the NSE key and drops the BSE duplicate; a
BSE key is used only when no matching NSE key is present.

## Historical Candle Bulk Download

Use `scripts/download_historical_candles.py` to download and persist historical
candles for every instrument in an instrument CSV file. This script is
read-only against Upstox: it calls historical candle endpoints only and does not
touch order endpoints.

```powershell
python scripts\download_historical_candles.py `
  --instruments-csv instruments.csv `
  --token-file upstox_token.txt `
  --sqlite-path data\historical_candles.sqlite3 `
  --summary-csv reports\historical-candle-download-summary.csv `
  --from-date 2025-07-01 `
  --to-date 2026-07-02 `
  --unit days `
  --interval 1 `
  --throttle-seconds 0.12
```

The downloader expects the same CSV format as the app instrument import and
uses `instrument_key` plus `symbol`. By default, rows with `enabled=false` are
skipped; pass `--include-disabled` to download them too. NSE/BSE duplicate
equity listings are collapsed to NSE, and BSE is used only when no matching NSE
listing exists.

Downloaded candles are stored in the existing SQLite-compatible `candles` table
with interval names such as `days:1`. Inserts use `INSERT OR REPLACE`, so the
command can be safely rerun for the same instrument/date range. The summary CSV
records per-instrument success, candle count, and any error so failed downloads
can be retried.

The repository does not currently include an `instruments.csv` file. Place the
instrument CSV in the workspace or pass its absolute path before running the
bulk download.

The downloader can also use the Upstox complete instrument JSON directly:

```powershell
python scripts\download_historical_candles.py `
  --upstox-instruments-url https://assets.upstox.com/market-quote/instruments/exchange/complete.json.gz `
  --token-file upstox_token.txt `
  --sqlite-path data\historical_candles.sqlite3 `
  --summary-csv reports\historical-candle-download-summary.csv `
  --from-date 2025-07-01 `
  --to-date 2026-07-02 `
  --unit days `
  --interval 1 `
  --throttle-seconds 0.12
```

For a safe trial run before downloading the full universe, add `--limit 10`.
The Upstox complete file changes over time; at the time this workflow was
added, it contained about 2,399 NSE/BSE equity instruments after filtering to
`segment` `NSE_EQ`/`BSE_EQ` and `instrument_type=EQ`. NSE/BSE duplicate equity
listings are collapsed to NSE before downloading candles.

## Offline Historical Scanner Report

After historical candles are stored locally, use
`scripts/offline_historical_scanner_report.py` to rank scanner candidates from
SQLite without broker credentials, network calls, or order endpoints:

```powershell
python scripts\offline_historical_scanner_report.py `
  --sqlite-path data\historical_candles.sqlite3 `
  --output-csv reports\offline-historical-scanner-ranking.csv `
  --labels-csv reports\historical-candle-download-summary.csv `
  --chart-dir reports\offline-historical-scanner-charts `
  --interval days:1 `
  --min-latest-close 20 `
  --top-n 50
```

The report combines bullish RSI divergence and bullish MACD crossover signals,
applies configurable strategy weights, and writes a CSV with rank, score,
symbol, contributing strategies, latest close, latest RSI, candle count, MACD
values, and diagnostics. By default, `--labels-csv` points at the historical
download summary CSV, which already contains `label` and `instrument_key`
columns. Use `--min-latest-close` and `--max-latest-close` to keep the report
focused on an operator-reviewed price band. Override weights with repeated
`--strategy-weight name=value` arguments, such as
`--strategy-weight rsi_bullish_divergence=1.5`. Add `--include-all` when you
want zero-score rows included for audit review. When `--chart-dir` is supplied,
the report writes PNG evidence charts for ranked candidates and stores the file
path in the `chart_path` CSV column. Use `--max-charts` to limit PNG generation
when the ranked list is large.

Portfolio sync reads available equity funds and long-term holdings from Upstox
into the shared `PortfolioState` model for downstream risk and order decisions.

The strategy module includes baseline technical indicators over candle close
prices: SMA, EMA, RSI, MACD, pivots, divergence checks, and support/resistance
trendlines.

Strategies share a common evaluation interface with instrument, candles, quote,
and portfolio context, returning validated strategy signals plus diagnostics.

Initial buy strategies include manual buy-price triggers and RSI oversold
signals with configured quantity, target, and stop-loss mapping.

Advanced buy strategies cover EMA bullish crossover, resistance breakout, and
volume surge confirmation signals.

Sell strategies cover target-profit/manual-target exits and stop-loss exits,
using current holdings for sell quantity and average buy price thresholds.

Signal aggregation converts validated strategy signals into decisions using
first-actionable, highest-confidence, or majority-vote modes.

The exit engine evaluates sell exits in deterministic priority order:
emergency risk, stop loss, manual target, fixed profit target, then strategy
sell signals.

The risk manager validates decisions against instrument status, quantity caps,
duplicate open orders, buy funds, and sell holdings before orders are created.

The kill switch emits emergency rejected risk events for manual, configuration,
or external stop triggers.

Market session checks use configurable local time windows with delivery-equity
defaults of 09:15-15:30 and a conservative last-order cutoff before close.

Order requests are queued with deterministic priority ordering, FIFO
tie-breaking, peek/pop semantics, and cancellation support.

Rate-limited API executors use a deterministic sliding window limiter with
retry-after feedback and callback execution only when capacity is available.

The dry-run order dispatcher records simulated accepted/rejected order records
with deterministic dry-run IDs and no broker API calls.

The paper portfolio simulator applies local filled order records to cash and
holdings, then reports realized and unrealized P&L from supplied quotes without
broker API calls.

The backtest runner replays candle history through existing strategy interfaces,
aggregates decisions, and applies local paper fills for deterministic performance
checks.

Live order placement targets Upstox V3 `/v3/order/place` only after explicit
live-trading, risk, market-session, and rate-limit safety gates pass.

Order monitoring maps Upstox order-book statuses into terminal/non-terminal
`OrderRecord` state for tracking and follow-up workflows.

Runtime worker groups provide thread-safe task submission, drain/stop behavior,
and exception capture for background processing.

SQLite persistence uses ordered, idempotent migrations for bot runs, orders,
risk events, strategy signals, decisions, quote snapshots, candles, API events,
instruments, audit events, and lookup indexes.

The persistence worker applies pending migrations and asynchronously writes
orders, risk events, audit events, strategy signals, decisions, quote snapshots,
candles, API events, and bot run lifecycle rows through a sink abstraction.

When `storage.sqlite_path` is configured, the app runner opens the SQLite
database, applies pending migrations, records a started bot-run row before
configured non-reporting modes execute, and records a completed row after the
run returns. It also imports configured instruments transactionally before the
run starts; a failed instrument batch rolls back and blocks startup instead of
leaving partial instrument state. The run row stores the mode and a stable hash
of the loaded config file text so persisted records can be traced back to their
runtime settings.

On Windows, the build copies a compatible `sqlite3.dll` beside
`tradingbot_upstox.exe` and the SQLite test binaries. SQLite is loaded at
runtime so the application does not depend on a compiler-specific import
library.

The show-orders command renders order records in a stable table format and
prints an explicit empty state when no orders are available. With
`--config <path>` and `storage.sqlite_path` set, `show-orders` reads historical
orders directly from SQLite without starting a bot run or placing broker orders.

Dry-run integration coverage connects strategy signal generation, aggregation,
risk approval, order queuing, dry-run dispatch, and persistence without broker calls.

## Live Trading Readiness

Live trading must remain disabled until the manual acceptance checklist is
completed: [docs/live_trading_acceptance_checklist.md](docs/live_trading_acceptance_checklist.md).

SRS section 25 decisions are recorded in
[docs/srs_open_decisions.md](docs/srs_open_decisions.md).
