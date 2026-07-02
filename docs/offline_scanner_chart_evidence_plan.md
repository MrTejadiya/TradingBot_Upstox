# Offline Scanner Chart Evidence Plan

## Jira

- `RK9311-210`: parent task, generate PNG evidence for offline ranked candidates.
- `RK9311-211`: planning and impact analysis.
- `RK9311-212`: implementation.
- `RK9311-213`: tests.
- `RK9311-214`: documentation.
- `RK9311-215`: review, verification, commit, and push.

## Requirement

Generate reviewable PNG evidence for top offline scanner candidates so the
ranked CSV can be inspected visually before any trading decision.

## Impact

- Extend `scripts/offline_historical_scanner_report.py`.
- Add optional `--chart-dir` output.
- Add `--max-charts` to limit generated PNG files when `--top-n` is large.
- Reuse the existing PNG chart renderer from
  `scripts/live_rsi_divergence_scan.py`.
- Add `chart_path` to the CSV so each ranked row links to its evidence image.
- Keep the workflow local-only and read-only.

## Verification

- Unit tests for chart generation.
- Unit tests for CSV chart path output.
- Real local run against `data/historical_candles.sqlite3`.
- Full `scripts/run_tests.ps1` verification.

## Safety

Charts are generated from local historical candles only. The workflow does not
load tokens, call Upstox, place orders, cancel orders, or create execution
instructions.
