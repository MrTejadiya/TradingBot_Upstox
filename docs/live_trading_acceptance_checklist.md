# Manual Live-Trading Acceptance Checklist

This checklist must be completed before enabling live order placement. It is an
operator sign-off document, not financial advice, and it does not replace broker,
exchange, SEBI, tax, or legal guidance.

Last reviewed against public Upstox documentation on 2026-07-01.

## Decision

- [ ] Live trading is approved for this bot run.
- [ ] Live trading is rejected; keep `mode=dry-run` or `mode=validate`.

Operator:

Date and time:

Git commit reviewed:

Configuration hash or file version:

Dry-run evidence location:

## Broker And Regulatory Prerequisites

- [ ] Upstox API app is active and belongs to the intended trading account.
- [ ] OAuth token generation and refresh process is documented for the operator.
- [ ] Primary static IP is registered with Upstox before placing API orders.
- [ ] Secondary static IP is registered, or the lack of a backup IP is accepted.
- [ ] If an exchange-approved algo name is configured, `X-Algo-Name` exactly matches the Upstox app setting.
- [ ] If no exchange-approved algo name is configured, the bot is configured not to send `X-Algo-Name`.
- [ ] Expected order throughput is under the regular-algo order limit, or formal SEBI/exchange algo registration evidence is attached.
- [ ] Operator confirms no attempt will be made to bypass OAuth, 2FA, static IP, exchange rules, broker restrictions, or SEBI requirements.

Evidence:

## Scope Confirmation

- [ ] Bot is limited to Indian equity delivery orders for version 1.
- [ ] Instrument CSV contains only intended equity delivery instruments.
- [ ] No intraday, margin, futures and options, commodity, mutual fund, high-frequency, or multi-broker workflow is enabled.
- [ ] Order payloads use `product=D` and `validity=DAY`.
- [ ] Default order type remains `LIMIT`.
- [ ] Market orders are disabled unless a future explicit high-risk approval process is added.

Evidence:

## Configuration Gates

- [ ] `mode=live` is intentionally configured only for the acceptance run.
- [ ] `live_trading_enabled=true` is intentionally configured only after this checklist is complete.
- [ ] Access token is supplied through the configured environment variable or approved secret store.
- [ ] No access token, API secret, authorization header, or plaintext credential is present in config files, logs, commits, or Jira comments.
- [ ] `max_orders_per_day`, `max_daily_traded_value`, `max_order_value`, and per-instrument quantity caps match the operator's risk limit.
- [ ] Internal order API rate limits are below the current broker-published limits with reserve capacity for exits.
- [ ] Kill switch source is configured and tested.

Evidence:

## Data And Strategy Readiness

- [ ] CSV validation passes with duplicate instruments rejected.
- [ ] All enabled instruments have intended quantity, max position, target, and stop-loss values.
- [ ] Market data freshness checks reject stale, missing, or malformed quotes/candles.
- [ ] Strategy profile selection is reviewed per instrument.
- [ ] Buy strategies emit expected signals in dry-run evidence.
- [ ] Sell and exit rules emit expected target, stop-loss, and priority exits in dry-run evidence.
- [ ] Signal aggregation mode and threshold are reviewed.

Evidence:

## Dry-Run Acceptance

- [ ] At least one full market-session dry run completed without live broker order calls.
- [ ] Dry-run recorded strategy signals, decisions, risk events, order requests, and simulated orders.
- [ ] Dry-run sell priority behavior was observed or tested.
- [ ] No duplicate open order condition was allowed through risk checks.
- [ ] No buy order was approved without sufficient configured funds.
- [ ] No sell order was approved without sufficient holdings.
- [ ] Logs contain no access tokens, API secrets, or authorization headers.
- [ ] SQLite audit records can be queried with `show-orders`.

Evidence:

## Test And Review Evidence

- [ ] `cmake --build build` passes on the release machine.
- [ ] `ctest --test-dir build --output-on-failure --timeout 30` passes.
- [ ] Manual review confirms the live dispatcher still requires live mode, live-trading confirmation, market-session approval, risk approval, and rate-limit capacity.
- [ ] Manual review confirms dry-run remains the default operating mode.
- [ ] Manual review confirms order dispatcher gives sell exits priority over buys.
- [ ] Manual review confirms order and API metadata are redacted before logging or persistence.

Evidence:

## First Live Run Limits

- [ ] First live run uses the smallest acceptable order quantity.
- [ ] First live run uses a short, supervised window during normal market hours.
- [ ] Operator is present with Upstox web or mobile access open for independent order monitoring.
- [ ] Operator has a manual cancellation plan ready before the run starts.
- [ ] Kill switch action is rehearsed immediately before the run.
- [ ] No unattended live trading is allowed until the first live run is reviewed.

Evidence:

## Post-Run Review

- [ ] Upstox order book matches local order records.
- [ ] Fills, rejections, and terminal statuses are reconciled.
- [ ] Holdings and funds are reconciled with Upstox.
- [ ] Logs and SQLite audit trail are archived.
- [ ] Any mismatch, rejection, stale-data event, or operator concern blocks further live trading until resolved.

Evidence:

## Reference Links

- Upstox static IP and algo-name announcement: https://upstox.com/developer/api-documentation/announcements/algo-trading-circular/
- Upstox API rate limits: https://upstox.com/developer/api-documentation/rate-limiting/
- Upstox Place Order V3: https://upstox.com/developer/api-documentation/v3/place-order/
- Upstox GTT order restrictions and related static-IP/algo-name errors: https://upstox.com/developer/api-documentation/place-gtt-order/
