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
.\build\tradingbot_upstox.exe --mode show-orders
```

`dry-run` is the default mode. `live` requires explicit live-trading gates.

## Configuration

Version 1 uses JSON configuration. The loader validates these required sections:
`app`, `upstox`, `input`, `market_data`, `strategies`, `exit_rules`, `risk`,
`rate_limits`, `storage`, and `logging`.

Start from `config.example.json` and provide access tokens through the configured
environment variable, not in the config file.

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
stored through a cache interface. The current implementation includes an
in-memory cache for tests; the SQLite-backed cache belongs with the persistence
storage work.

The market feed layer currently provides a websocket-ready abstraction for LTP
subscription payloads, quote message parsing, and disconnect notifications.

Portfolio sync reads available equity funds and long-term holdings from Upstox
into the shared `PortfolioState` model for downstream risk and order decisions.

The strategy module includes baseline technical indicators over candle close
prices: SMA, EMA, RSI, and MACD.

Strategies share a common evaluation interface with instrument, candles, quote,
and portfolio context, returning validated strategy signals plus diagnostics.

Initial buy strategies include manual buy-price triggers and RSI oversold
signals with configured quantity, target, and stop-loss mapping.

Advanced buy strategy placeholders expose named EMA crossover, breakout, and
volume surge extension points with diagnostics but no live trade signals yet.

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

Live order placement targets Upstox V3 `/v3/order/place` only after explicit
live-trading, risk, market-session, and rate-limit safety gates pass.

Order monitoring maps Upstox order-book statuses into terminal/non-terminal
`OrderRecord` state for tracking and follow-up workflows.

Runtime worker groups provide thread-safe task submission, drain/stop behavior,
and exception capture for background processing.

SQLite persistence starts with ordered, idempotent migrations for bot runs,
orders, risk events, strategy signals, audit events, and lookup indexes.

The persistence worker applies pending migrations and asynchronously writes
orders, risk events, and audit events through a sink abstraction.
