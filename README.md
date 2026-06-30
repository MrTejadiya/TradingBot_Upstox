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
