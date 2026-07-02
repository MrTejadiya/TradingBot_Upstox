# Provisional Live RSI Divergence Plan

## Jira

- `RK9311-242`: parent story, add provisional live RSI divergence opportunities.
- `RK9311-243`: planning and impact analysis.
- `RK9311-244`: provisional RSI divergence detector.
- `RK9311-245`: live dashboard integration.
- `RK9311-246`: tests.
- `RK9311-247`: verification, live restart, commit, and Jira evidence.

## Requirement

Capture RSI divergence opportunities as early as possible while the market is
open. Confirmed daily RSI divergence often appears late because a pivot low
needs a right-side candle. The live dashboard should therefore show a separate
provisional signal that uses today's dynamic candle as an unconfirmed pivot
candidate.

## Signal Semantics

`rsi_bullish_divergence_provisional` is true when:

- today's latest dynamic candle is lower than its left-side candle or candles,
  using the configured `wing_size`;
- today's close is lower than the last confirmed RSI pivot-low close;
- today's RSI is higher than the RSI at that confirmed pivot low.

The signal is intentionally provisional:

- it can appear intraday;
- it can disappear if today's price or RSI changes;
- it is ranked separately from confirmed `rsi_bullish_divergence`;
- it receives a lower default score than confirmed RSI divergence.

## Scoring

- Confirmed RSI bullish divergence base score: `0.80`.
- Provisional RSI bullish divergence base score: `0.65`.
- MACD bullish cross base score: `0.70`.

The live dashboard still applies per-strategy freshness before ranking.
