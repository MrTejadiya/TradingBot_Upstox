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
