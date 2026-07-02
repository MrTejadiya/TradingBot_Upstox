# Live Scanner Per-Strategy Freshness Plan

## Jira

- `RK9311-236`: parent bug, fix per-strategy freshness for live scanner ranking.
- `RK9311-237`: planning and acceptance criteria.
- `RK9311-238`: per-strategy stale signal pruning.
- `RK9311-239`: per-strategy freshness metadata.
- `RK9311-240`: mixed fresh/stale strategy tests.
- `RK9311-241`: verification, live restart, commit, and evidence.

## Root Cause

The continuous dashboard filtered ranked rows by the newest signal on the row.
For EXCELSOFT, MACD crossed on today's dynamic candle while the RSI bullish
divergence pivot pair was older. The row was fresh overall, but the stale RSI
strategy still contributed to score and strategy labels.

## Fix

- Store signal index and timestamp per strategy.
- Before ranking live dashboard rows, remove any individual strategy whose
  signal age exceeds `--max-signal-age-candles`.
- Recompute score, signal count, latest signal age, and strategy labels from
  the remaining fresh strategies.
- Expose `strategy_signal_ages` and `strategy_signal_timestamps` in JSON for
  review.

## EXCELSOFT Finding

For the observed EXCELSOFT row:

- RSI bullish divergence pair was `2026-06-25` close `74.87`, RSI `33.896`
  versus `2026-06-30` close `74.67`, RSI `34.1973`.
- Price made a lower low and RSI made a higher low, so RSI divergence was true.
- That RSI signal was older than the live freshness window.
- MACD crossed on the `2026-07-02` dynamic candle, so MACD remained fresh.
