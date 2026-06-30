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

`dry-run` is the default mode. `live` is recognized but blocked until explicit live-trading gates are implemented.

