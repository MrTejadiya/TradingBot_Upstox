# Upstox Complete Instrument Universe Plan

## Requirement

Use the complete set of instrument keys available on Upstox instead of relying
only on manually maintained instrument CSV rows.

## Source Decision

Use Upstox Instrument JSON files as the primary full-universe source. Upstox
documents the BOD instrument files as the complete list available at the
beginning of day and recommends JSON instrument data over CSV for robustness and
future scalability.

Initial implementation will support a local JSON file path first. Downloading
from a URL can be added through a small fetch/cache layer after the parser and
normalization are tested.

## Scope

Version 1 will import equity delivery candidates only:

- `segment` is `NSE_EQ` or `BSE_EQ`
- `instrument_type` is `EQ`
- `instrument_key` is non-empty
- `trading_symbol` or `short_name` can populate display symbol metadata
- non-equity, index, FO, MCX, mutual fund, MTF-only, MIS-only, and suspended
  lists remain out of scope for trading-universe import

The full Upstox JSON file can still contain other records; the importer will
skip unsupported records rather than failing the entire load.

## Existing Behavior To Preserve

- Exact duplicate `instrument_key` rows are invalid.
- NSE/BSE duplicate listings for the same equity identity collapse to NSE.
- BSE is retained only when a matching NSE listing is absent.
- Dry-run remains the default operating mode.
- No order placement is added to this work.
- Imported instruments must still pass existing risk defaults before a later
  strategy can produce orderable decisions.

## Implementation Impact

1. Add an Upstox instrument JSON parser in `infra`.
   - Input: JSON array from Upstox BOD complete instruments.
   - Output: `std::vector<core::Instrument>`.
   - Map exchange and key fields into existing `core::Instrument`.
   - Apply safe defaults for generated fields.
   - Reuse the NSE-over-BSE duplicate preference already used by CSV loading.

2. Extend configuration.
   - Add instrument source mode: `csv` or `upstox_json`.
   - Add local JSON path for the Upstox complete file.
   - Add default generated values for imported instruments:
     `enabled`, `quantity`, `max_position_qty`, `target_profit_pct`,
     optional stop-loss/trailing values.
   - Keep current CSV config working.

3. Wire app startup.
   - In configured non-reporting modes, load instruments from the configured
     source.
   - Persist normalized instruments using `SqliteInstrumentStore`.
   - Fail startup clearly if configured source is missing or malformed.

4. Tests.
   - Parser unit tests for sample NSE/BSE Upstox JSON objects.
   - Parser tests for skipping unsupported instruments.
   - NSE/BSE preference tests for JSON imports.
   - Config parser tests for source mode and default values.
   - App runner test proving JSON import persists normalized instruments.
   - Unified runner verification.

5. Docs.
   - README: how to download/use the Upstox complete instrument JSON.
   - README: source mode config examples.
   - Acceptance checklist: full-universe import reviewed before live trading.

## Jira Breakdown

- `RK9311-172`: main task, complete Upstox instrument universe.
- `RK9311-173`: planning and impact analysis.
- `RK9311-174`: Upstox instrument JSON parser.
- `RK9311-175`: configurable instrument source.
- `RK9311-176`: app startup wiring and persistence.
- `RK9311-177`: docs and operator guidance.

## Safety Notes

This work increases the number of instruments available to scan. It must not
increase live order placement authority. Live orders remain gated by operating
mode, live-trading confirmation, market session, risk manager, rate limiter, and
the live trading acceptance checklist.
