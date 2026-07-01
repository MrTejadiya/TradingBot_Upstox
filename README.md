# TradingBot Upstox

C++ Upstox delivery trading bot scaffold based on the SRS in `upstox_cpp_trading_bot_srs.pdf`.

## Build

```powershell
cmake -S . -B build -G "Ninja"
cmake --build build
ctest --test-dir build --output-on-failure
```

If Ninja is not available, omit `-G "Ninja"` and let CMake choose a local generator.

## CLI

```powershell
.\build\tradingbot_upstox.exe --mode validate
.\build\tradingbot_upstox.exe --mode dry-run
.\build\tradingbot_upstox.exe --mode paper
.\build\tradingbot_upstox.exe --mode show-orders
.\build\tradingbot_upstox.exe --config config.example.json --mode show-orders
```

`dry-run` is the default mode. `paper` uses local simulator/backtest components
without broker order placement. `live` requires explicit live-trading gates.
Pass `--config <path>` to load JSON configuration; explicit CLI flags such as
`--mode` override config defaults.

## Configuration

Version 1 uses JSON configuration. The loader validates these required sections:
`app`, `upstox`, `input`, `market_data`, `strategies`, `exit_rules`, `risk`,
`rate_limits`, `storage`, and `logging`.

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

`input.instruments_csv` points to the configured instrument universe. When the
path is relative, configured app runs resolve it relative to the config file
location. For SQLite-backed non-reporting modes, the app loads this CSV before
the mode runs and upserts the instrument rows into SQLite.

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
