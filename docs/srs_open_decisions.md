# SRS Open Decisions

This record resolves section 25 of `upstox_cpp_trading_bot_srs.pdf` for version
1. Changes to these decisions should be made through a new Jira task, reviewed
against broker/regulatory requirements, and covered by tests where behavior
changes.

## Decision 1: Configuration Format

Decision: use JSON for version 1.

Rationale: JSON is already implemented and tested in the configuration loader,
keeps the first release dependency-light, and is supported by
`config.example.json`. YAML can be added later if there is a clear operator need.

Implementation references:

- `config.example.json`
- `include/tradingbot/infra/config.hpp`
- `src/infra/config.cpp`
- `tests/infra/config_tests.cpp`

## Decision 2: First Strategy Set

Decision: version 1 enables manual buy-price triggers, RSI oversold buys, EMA
bullish crossover buys, resistance breakout buys, volume-surge confirmation
buys, target-profit/manual-target sells, stop-loss sells, signal aggregation,
and exit priority handling.

Rationale: this gives the bot an auditable delivery-trading strategy surface
while keeping each signal rule deterministic and unit tested before it can feed
aggregation, risk validation, and order dispatch.

Implementation references:

- `src/strategy/buy_strategies.cpp`
- `src/strategy/sell_strategies.cpp`
- `src/strategy/advanced_buy_strategies.cpp`
- `src/strategy/signal_aggregation.cpp`
- `src/strategy/exit_engine.cpp`

## Decision 3: Runtime Shape

Decision: version 1 is a supervised command-line/service run during configured
market hours. Dry-run remains the default. Fully unattended continuous trading
and scheduled scan orchestration remain deferred until live acceptance evidence
exists.

Rationale: the current architecture has worker groups, market-session gates,
rate-limited execution, and persistence primitives, but the SRS safety posture
requires dry-run evidence and manual acceptance before unattended live trading.

Implementation references:

- `include/tradingbot/app/operating_mode.hpp`
- `src/app/operating_mode.cpp`
- `include/tradingbot/runtime/worker_group.hpp`
- `src/strategy/market_session.cpp`
- `docs/live_trading_acceptance_checklist.md`

## Decision 4: Manual Target Priority

Decision: a configured manual target overrides the fixed profit target, but it
does not override higher-priority safety exits. Emergency risk and stop loss
remain higher priority than manual target.

Rationale: manual target is an intentional per-instrument operator setting, but
capital-protection exits must still win.

Implementation references:

- `src/strategy/exit_engine.cpp`
- `src/strategy/sell_strategies.cpp`
- `tests/strategy/exit_engine_tests.cpp`
- `tests/strategy/sell_strategies_tests.cpp`

## Decision 5: Market Orders

Decision: market orders are forbidden for version 1 live trading. Delivery
orders must use `product=D`, `validity=DAY`, and default to `LIMIT`.

Rationale: the SRS prioritizes safety, Upstox documentation currently includes
market-order restriction errors for API orders, and limit orders provide a more
auditable first live-trading surface. Any future market-order support requires a
new approval task, explicit high-risk configuration, and tests.

Implementation references:

- `include/tradingbot/core/domain.hpp`
- `src/order/live_order_dispatcher.cpp`
- `tests/order/live_order_dispatcher_tests.cpp`
- `docs/live_trading_acceptance_checklist.md`

## Decision 6: NSE/BSE Duplicate Listings

Decision: the instrument universe must not process both NSE and BSE listings for
the same equity identity. Exact duplicate `instrument_key` rows remain invalid.
When `NSE_EQ|...` and `BSE_EQ|...` share the same identity after the pipe
separator, the NSE row is retained and the BSE row is dropped. A BSE row is
retained only when no matching NSE row is present.

Rationale: scanning or trading both exchange listings for the same equity can
double count a candidate and create duplicate decision pressure. Preferring NSE
keeps the universe deterministic while preserving BSE-only coverage.

Implementation references:

- `src/infra/instrument_csv.cpp`
- `tests/infra/instrument_csv_tests.cpp`
- `tests/app/app_runner_tests.cpp`
- `scripts/live_rsi_divergence_scan.py`
- `tests/scripts/live_rsi_divergence_scan_tests.py`

## Decision 7: Complete Upstox Instrument Universe

Decision: support Upstox complete instrument JSON as an alternate instrument
source through `input.instrument_source=upstox_json`, while keeping manual CSV
as the default. The JSON source can be a local file or a configured URL with a
local cache. Version 1 imports only `NSE_EQ` and `BSE_EQ` records with
`instrument_type=EQ`; unsupported records are skipped.

Rationale: Upstox publishes beginning-of-day instrument files and recommends
JSON instrument data for robustness and future scalability. A local file keeps
startup deterministic; URL/cache mode reduces manual refresh work while still
parsing from a local cache and failing before bot-run creation when acquisition
or parsing fails. Successful URL refreshes write a sidecar metadata file so the
operator can review the source URL, refresh time, HTTP status, and payload size
before live trading.

Implementation references:

- `include/tradingbot/infra/upstox_instrument_json.hpp`
- `src/infra/upstox_instrument_json.cpp`
- `src/app/app_runner.cpp`
- `src/infra/upstox_instrument_json_cache.cpp`
- `src/infra/win_http_transport.cpp`
- `config.example.json`
- `tests/infra/upstox_instrument_json_tests.cpp`
- `tests/infra/upstox_instrument_json_cache_tests.cpp`
- `tests/app/app_runner_tests.cpp`
