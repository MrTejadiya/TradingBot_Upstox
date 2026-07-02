# Offline Scanner Report Enrichment Plan

## Jira

- `RK9311-204`: parent task, enrich offline scanner report with symbols and filters.
- `RK9311-205`: planning and impact analysis.
- `RK9311-206`: implementation.
- `RK9311-207`: tests.
- `RK9311-208`: documentation.
- `RK9311-209`: review, verification, commit, and push.

## Requirement

Make the offline scanner ranking report easier to review before trading by
including human-readable symbols and allowing configurable latest-close filters.

## Impact

- Extend `scripts/offline_historical_scanner_report.py`.
- Add optional label metadata loading from a local CSV. The default source is
  the historical download summary CSV because it already contains `label` and
  `instrument_key`.
- Add `symbol` to report output next to `instrument_key`.
- Add `--min-latest-close` and `--max-latest-close` filters so operator reviews
  can exclude very low-priced or out-of-scope instruments without changing
  scanner logic.
- Keep behavior read-only and local-only. No Upstox token, network, or order
  endpoint is used.

## Verification

- Unit tests for label CSV loading.
- Unit tests for symbol output.
- Unit tests for min/max latest-close filtering.
- Full `scripts/run_tests.ps1` verification.

## Safety

Filtering affects report output only. It does not place, cancel, or prepare
orders. The generated CSV remains evidence for human review, not an execution
instruction.
