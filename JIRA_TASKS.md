# Jira Backlog: C++ Upstox Delivery Trading Bot

Source: `upstox_cpp_trading_bot_srs.pdf`  
Prepared from SRS version 1.0 dated 30 June 2026.

## Suggested Jira Setup

- Project type: Software project, Scrum or Kanban
- Issue types used: Epic, Story, Task
- Suggested components: `app`, `config`, `csv`, `market-data`, `indicators`, `strategy`, `exit-engine`, `risk`, `orders`, `concurrency`, `persistence`, `logging`, `security`, `testing`, `docs`
- Suggested labels: `upstox-bot`, `cpp`, `delivery-trading`, `dry-run-first`

## Epic 1: Project Foundation And CLI

### Story: Scaffold C++ project with CMake

Description: Create the initial C++20 or C++23 application structure with CMake and the proposed source layout.

Acceptance criteria:
- Repository builds on Windows as the first target environment.
- Source folders exist for `app`, `core`, `infra`, `indicators`, `strategies`, `risk`, `order`, `concurrency`, and `tests`.
- CLI entrypoint supports at least `validate`, `dry-run`, `show-orders`, and future-compatible `live` modes.
- Build instructions are documented.

Priority: High  
Components: `app`, `docs`  
Estimate: 5

### Story: Define core domain models

Description: Implement shared domain objects for instruments, market snapshots, strategy signals, decisions, risk events, orders, holdings, portfolio state, and bot runs.

Acceptance criteria:
- Models include `instrument_key` as the canonical identifier.
- Strategy signals include action, confidence, suggested quantity, prices, stop loss, reason, strategy name, and timestamp.
- Order requests include product, validity, order type, tag, source strategy, and run identifier.
- Models are covered by basic serialization or formatting tests where applicable.

Priority: High  
Components: `core`  
Estimate: 5

### Story: Implement operating mode validation

Description: Enforce mode-specific behavior for `validate`, `dry-run`, `paper`, `live`, and `show-orders`.

Acceptance criteria:
- `dry-run` is the default mode.
- `live` requires both `mode=live` and `live_trading_enabled=true`.
- `validate` performs configuration and CSV validation without market calls.
- Invalid mode combinations fail closed with clear errors.

Priority: Highest  
Components: `app`, `security`  
Estimate: 3

## Epic 2: Configuration, CSV, Logging, And Security

### Story: Choose and implement configuration format

Description: Resolve the SRS open decision between YAML and JSON, then implement config loading and validation.

Acceptance criteria:
- Required sections are validated: `app`, `upstox`, `input`, `market_data`, `strategies`, `exit_rules`, `risk`, `rate_limits`, `storage`, and `logging`.
- Required keys include mode, live trading flag, access token source, instruments CSV path, candle intervals, signal modes, risk limits, rate limits, SQLite path, and log directory.
- Config validation reports all actionable missing or malformed fields.
- Config files do not require plaintext API secrets.

Priority: Highest  
Components: `config`, `security`  
Estimate: 8

### Story: Implement secure credential loading

Description: Load Upstox access tokens from environment variables, secure prompt, or approved secret storage.

Acceptance criteria:
- Access token value is never written to logs or SQLite.
- Missing credentials fail closed for modes that require API access.
- `access_token_env` is supported by configuration.
- Unit tests verify redaction of token-like values.

Priority: Highest  
Components: `security`, `config`  
Estimate: 5

### Story: Implement CSV instrument loader

Description: Read the target company watchlist from CSV and produce validated instrument definitions.

Acceptance criteria:
- Mandatory `instrument_key` column is required.
- Minimum columns are supported: `instrument_key`, `symbol`, `enabled`, `quantity`, `max_position_qty`, `target_profit_pct`.
- Recommended optional columns are parsed when present.
- Symbol names are treated as display metadata only.

Priority: Highest  
Components: `csv`  
Estimate: 5

### Story: Implement CSV validation rules

Description: Validate CSV rows according to SRS rules before bot startup.

Acceptance criteria:
- Duplicate instrument keys are rejected.
- `enabled` parses as true or false.
- Quantity and max position are positive integers.
- Percentage and manual price fields are valid decimal values when present.
- Unknown strategy profiles are rejected.
- Invalid rows fail startup unless `skip_invalid_rows` is explicitly enabled.

Priority: Highest  
Components: `csv`, `testing`  
Estimate: 5

### Story: Implement structured logging with rotation and redaction

Description: Add rotating logs with configurable levels and secret redaction.

Acceptance criteria:
- Startup configuration summary is logged without secrets.
- CSV validation summary is logged.
- Strategy signals, aggregation results, risk decisions, and order decisions are logged.
- Access tokens, API secrets, and authorization headers are redacted.
- Daily rotation and configurable log levels are supported.

Priority: High  
Components: `logging`, `security`  
Estimate: 5

## Epic 3: Market Data And Upstox API Client

### Story: Implement Upstox REST API client shell

Description: Build an authenticated Upstox API client for REST calls with request metadata, retries, and redacted logging.

Acceptance criteria:
- Authentication headers are applied from secure credential source.
- API request metadata records response codes, retry count, and latency without secrets.
- Transient HTTP failures use exponential backoff with jitter.
- Non-retryable validation, authentication, and rejected-order errors are not retried blindly.

Priority: High  
Components: `market-data`, `orders`, `security`  
Estimate: 8

### Story: Implement market quote LTP retrieval

Description: Fetch latest traded price using Upstox V3 market quote APIs or streaming-compatible abstraction.

Acceptance criteria:
- LTP is retrieved by `instrument_key`.
- Missing, stale, or malformed quote data is rejected.
- Quote snapshots used in decisions are persisted or queued for persistence.
- Mock API tests cover quote parsing.

Priority: High  
Components: `market-data`, `testing`  
Estimate: 5

### Story: Implement OHLCV candle retrieval and caching

Description: Fetch historical OHLCV candle data for configurable intervals and cache results in SQLite.

Acceptance criteria:
- Intervals such as 1 day, 1 hour, and 15 minutes are supported by configuration.
- Candle data is validated before use by indicators.
- Redundant API calls are reduced by cache lookup.
- Candle snapshots used in strategy decisions are persisted.

Priority: High  
Components: `market-data`, `persistence`  
Estimate: 8

### Story: Add WebSocket market data feed support

Description: Prefer WebSocket feeds for live monitoring to reduce REST polling.

Acceptance criteria:
- Market data thread receives streaming quote updates.
- Feed disconnects are logged and retried according to error policy.
- Strategy workers consume immutable market data snapshots.
- REST fallback behavior is configurable.

Priority: Medium  
Components: `market-data`, `concurrency`  
Estimate: 8

### Story: Implement portfolio sync service

Description: Refresh funds, holdings, positions, and order state through standard APIs.

Acceptance criteria:
- Funds are available for buy risk checks.
- Holdings are available for sell risk checks.
- Portfolio state is protected by mutex or read-write locks.
- Non-critical sync failures do not stop exit monitoring when possible.

Priority: High  
Components: `market-data`, `risk`, `concurrency`  
Estimate: 8

## Epic 4: Indicators, Strategies, And Signal Aggregation

### Story: Implement indicator library baseline

Description: Add reusable calculations for RSI, moving averages, pivots, support/resistance helpers, and volume statistics.

Acceptance criteria:
- RSI calculation is unit tested.
- Moving average calculation is unit tested.
- Indicator functions reject insufficient or malformed candle data.
- Indicator APIs are independent of Upstox client code.

Priority: High  
Components: `indicators`, `testing`  
Estimate: 8

### Story: Implement shared strategy interface

Description: Define modular strategy classes that emit signals without placing orders directly.

Acceptance criteria:
- Strategies implement a common interface.
- Strategy output conforms to the required signal fields.
- Strategies cannot call order APIs.
- Strategy modules can be enabled or disabled by profile.

Priority: Highest  
Components: `strategy`, `core`  
Estimate: 5

### Story: Implement initial buy strategies

Description: Implement version 1 buy strategies chosen from SRS options.

Acceptance criteria:
- RSI oversold strategy is implemented.
- Moving average crossover strategy is implemented.
- Manual buy price trigger from CSV is implemented.
- Each strategy has unit tests for positive, negative, and malformed-data cases.

Priority: High  
Components: `strategy`, `indicators`, `testing`  
Estimate: 8

### Story: Implement advanced buy strategy placeholders

Description: Create extension points or backlog stubs for RSI bullish divergence, trendline support bounce, resistance breakout, and volume breakout confirmation.

Acceptance criteria:
- Interfaces support future advanced strategies without order-flow changes.
- Configuration can reject unavailable strategies with a clear message.
- Jira tasks are linked for Phase 7 implementation.

Priority: Medium  
Components: `strategy`  
Estimate: 3

### Story: Implement sell strategies and exit rules

Description: Support fixed target, manual target, stop loss, trailing stop, and moving average breakdown.

Acceptance criteria:
- Default fixed profit target is 10 percent unless overridden.
- Manual target price from CSV or config is supported.
- Stop loss and trailing stop settings are evaluated.
- Maximum holding period rule is supported or explicitly flagged as unavailable.
- Unit tests cover each exit condition.

Priority: Highest  
Components: `exit-engine`, `strategy`, `testing`  
Estimate: 8

### Story: Implement signal aggregation modes

Description: Aggregate multiple strategy signals using `any`, `all`, and `weighted_score` modes.

Acceptance criteria:
- Buy and sell signal modes are configurable.
- Weighted score compares confidence against configurable thresholds.
- `first_exit_wins` is supported for urgent sells.
- Aggregation decisions are logged and persisted.

Priority: High  
Components: `strategy`, `exit-engine`, `testing`  
Estimate: 5

## Epic 5: Exit Engine, Risk Management, And Safety Gates

### Story: Implement exit engine with priority rules

Description: Monitor holdings and generate exit decisions using the SRS exit priority order.

Acceptance criteria:
- Exit priority is emergency risk exit, stop loss, manual target, fixed target, strategy target, strategy sell signal, trailing stop, maximum holding duration.
- Average buy price, current LTP, manual targets, strategy targets, stop loss, and trailing stop are evaluated.
- A 10 percent default profit target creates a high-priority sell unless overridden by a higher-priority target.
- Exit decisions include reason and source data.

Priority: Highest  
Components: `exit-engine`, `risk`  
Estimate: 8

### Story: Implement risk manager rules

Description: Validate every buy and sell decision before an order is queued.

Acceptance criteria:
- Trading mode, enabled stock, active strategy profile, market session, and order window are validated.
- Quantity, max position, daily order count, max order value, and max daily traded value are validated.
- Funds are checked before buys.
- Holdings are checked before sells.
- Duplicate open orders for the same instrument and side are blocked unless explicitly allowed.
- Every rejection records a reason code.

Priority: Highest  
Components: `risk`, `testing`  
Estimate: 13

### Story: Implement kill switch

Description: Add an immediate live-order block controlled by atomic state.

Acceptance criteria:
- Kill switch blocks new live orders immediately.
- Dry-run decisions can continue to be logged when safe.
- Kill switch state is thread-safe.
- Activation and deactivation are logged.

Priority: Highest  
Components: `risk`, `concurrency`, `security`  
Estimate: 5

### Story: Implement market session and order window checks

Description: Prevent order placement outside configured market sessions and allowed order windows.

Acceptance criteria:
- Configurable market session window is supported.
- Orders outside the window are rejected with a reason code.
- Tests cover boundary times.
- Live mode fails closed when session rules cannot be evaluated.

Priority: High  
Components: `risk`, `config`, `testing`  
Estimate: 5

### Task: Resolve live trading regulatory prerequisites

Description: Document operational checks for static IP, optional approved algo name header, OAuth, 2FA, broker restrictions, exchange rules, and SEBI constraints.

Acceptance criteria:
- Live mode checklist is documented.
- Static IP requirement is represented in config validation or preflight checks.
- Optional `X-Algo-Name` header is only sent when configured.
- The bot never attempts to bypass broker or regulatory controls.

Priority: Highest  
Components: `security`, `docs`  
Estimate: 3

## Epic 6: Order Management, Concurrency, And Rate Limits

### Story: Implement order request priority queue

Description: Process order requests by priority and timestamp.

Acceptance criteria:
- Priority order is emergency sell or stop loss, target profit sell, manual target sell, strategy sell, buy order, non-urgent modify or cancel.
- Simultaneous sells process stop-loss and emergency exits first.
- Profit exits are ordered by urgency and age.
- Unit tests cover queue ordering.

Priority: Highest  
Components: `orders`, `concurrency`, `testing`  
Estimate: 5

### Story: Implement rate-limited API executors

Description: Route all Upstox API calls through centralized token-bucket or leaky-bucket limiters.

Acceptance criteria:
- Separate rate limiters exist for order APIs and standard APIs.
- Internal configured limits are lower than broker-published limits.
- Sell orders reserve order API capacity so buys cannot starve urgent exits.
- Rate limiter behavior is unit tested.

Priority: Highest  
Components: `concurrency`, `orders`, `market-data`, `testing`  
Estimate: 8

### Story: Implement dry-run order dispatcher

Description: Build the single controlled component allowed to place, modify, or cancel orders, initially in dry-run mode.

Acceptance criteria:
- Strategies and workers cannot bypass the dispatcher.
- Dry-run mode creates simulated order decisions without broker placement.
- Dispatcher records every request and simulated result.
- Sell priority is enforced in dry-run tests.

Priority: Highest  
Components: `orders`, `concurrency`, `persistence`  
Estimate: 8

### Story: Implement live order placement behind safety gates

Description: Integrate Upstox delivery order placement only after mode, risk, compliance, and kill-switch checks.

Acceptance criteria:
- Product is `D`.
- Validity is `DAY`.
- Order type defaults to `LIMIT`.
- Market orders are disabled by default and require explicit high-risk configuration if allowed.
- Live orders include required metadata and source strategy.
- Mock API tests verify order payloads.

Priority: High  
Components: `orders`, `security`, `testing`  
Estimate: 13

### Story: Implement order tracking and terminal-state monitoring

Description: Track order IDs, statuses, rejection reasons, fills, and raw redacted API metadata until terminal state or timeout.

Acceptance criteria:
- Upstox order IDs are stored when available.
- Order status updates are polled or subscribed to.
- Rejection reason and fill details are recorded.
- Timeout behavior is configurable and logged.

Priority: High  
Components: `orders`, `persistence`  
Estimate: 8

### Story: Implement thread-safe worker groups

Description: Add worker groups for market data, strategies, exit monitoring, order dispatch, portfolio sync, and persistence.

Acceptance criteria:
- Thread-safe queues exist for signals, risk events, order requests, and persistence events.
- Immutable snapshots are passed into strategies.
- Portfolio state uses appropriate locks.
- Shutdown and kill switch use atomic flags.
- Network calls do not hold long-running shared-state locks.

Priority: High  
Components: `concurrency`  
Estimate: 13

## Epic 7: Persistence, Audit, Testing, And Release Readiness

### Story: Implement SQLite schema and migrations

Description: Create local persistence for bot runs, instruments, candles, quotes, strategy signals, decisions, risk events, orders, and API events.

Acceptance criteria:
- SQLite database path is configurable.
- Tables match SRS audit requirements.
- Sensitive tokens are not stored in plaintext.
- Schema creation is idempotent.

Priority: Highest  
Components: `persistence`, `security`  
Estimate: 8

### Story: Implement persistence worker

Description: Persist logs, decisions, quotes, candle data, orders, and API events without blocking trading logic.

Acceptance criteria:
- Persistence worker consumes queued events.
- Trading logic is not blocked by normal database writes.
- Fatal error context is persisted before shutdown when possible.
- Failed persistence writes are logged with safe context.

Priority: High  
Components: `persistence`, `concurrency`  
Estimate: 8

### Story: Implement show-orders command

Description: Display stored orders and decisions from SQLite.

Acceptance criteria:
- `show-orders` reads from configured SQLite path.
- Output includes run ID, instrument key, side, quantity, price, status, reason, and timestamp.
- Missing database or empty result is handled clearly.
- Secrets and authorization metadata are never displayed.

Priority: Medium  
Components: `app`, `persistence`  
Estimate: 3

### Story: Build dry-run integration test

Description: Test a complete dry-run trading loop with config, CSV, market data stubs, strategy signals, risk validation, priority queue, and persistence.

Acceptance criteria:
- Test validates at least one buy decision and one sell decision.
- No live API order is placed.
- Strategy signals, decisions, risk events, and simulated orders are persisted.
- Test can run deterministically in CI.

Priority: Highest  
Components: `testing`  
Estimate: 8

### Story: Complete unit test suite required by SRS

Description: Add unit tests for CSV validation, indicators, strategies, aggregation, risk, rate limiter, and priority queue behavior.

Acceptance criteria:
- CSV parsing and validation tests are present.
- RSI, moving averages, pivots, divergence, and trendline tests are present or tracked with explicit Phase 7 tasks.
- Each implemented strategy has tests.
- Risk rules have tests.
- Rate limiter and priority queue tests are present.

Priority: Highest  
Components: `testing`  
Estimate: 13

### Task: Prepare manual live-trading acceptance checklist

Description: Define the manual acceptance test required before any live trading.

Acceptance criteria:
- Checklist covers dry-run validation, broker compliance, static IP, credentials, risk limits, kill switch, log redaction, and order payload review.
- Checklist requires explicit user approval before live trading.
- Checklist references the SRS safety constraints.

Priority: Highest  
Components: `docs`, `security`  
Estimate: 3

### Task: Resolve SRS open decisions

Description: Decide and document version 1 behavior for remaining open questions.

Acceptance criteria:
- Configuration format is selected.
- First version strategy set is selected.
- Continuous market-hours operation versus scheduled scans is selected.
- Manual target override behavior is selected.
- Market order policy is selected.

Priority: Highest  
Components: `docs`, `config`, `strategy`, `orders`  
Estimate: 3

## Suggested Phase Mapping

- Phase 1: Epic 1 and Epic 2
- Phase 2: Indicator baseline, strategy interface, and initial buy strategies
- Phase 3: Signal aggregation, sell strategies, exit engine, and risk manager
- Phase 4: Worker groups, rate limiters, priority queue, and dry-run dispatcher
- Phase 5: Upstox market data integration and SQLite audit storage
- Phase 6: Live order placement and order tracking behind safety gates
- Phase 7: Advanced RSI divergence, trendline detection, backtesting, and performance tuning
