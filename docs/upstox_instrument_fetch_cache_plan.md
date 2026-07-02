# Upstox Instrument Fetch Cache Plan

## Requirement

Add the deferred fetch/cache layer for the Upstox complete instrument JSON file
so operators can use a configured URL source instead of manually downloading the
file before every run.

## Source And Safety Decision

The existing local JSON file import remains the default `upstox_json` workflow.
URL fetching is optional and must write to a local cache before parsing. Startup
parses the cached/local JSON through the same importer already covered by
`RK9311-174` through `RK9311-176`.

This work must not add any order placement authority. It only changes how the
instrument universe JSON text is obtained before the existing parser and SQLite
instrument import run.

## Scope

Version 1 will support:

- configured Upstox JSON URL
- configured local cache path
- dependency-injected HTTP transport for testable downloads
- force-refresh option to download every startup when enabled
- fallback to the existing cache when download fails and fallback is enabled
- clear startup failure when neither download nor cache is available
- same NSE/BSE preference, duplicate rejection, and equity-only filtering as the
  current local JSON importer

Out of scope:

- automatic discovery of the latest URL from the Upstox documentation page
- cron/scheduled pre-market refresh
- checksum/signature validation
- broker order placement

## Implementation Impact

1. Add a fetch/cache component in `infra`.
   - Input: URL, cache path, HTTP transport, refresh/fallback policy.
   - Output: JSON text plus metadata describing whether content came from
     download or cache.
   - Atomic-ish cache write by writing a temporary file then replacing the
     cache file.
   - Tests for download success, cache fallback, failed missing cache, and
     no-refresh cache read.

2. Extend configuration.
   - Add optional `input.upstox_instruments_url`.
   - Add optional `input.upstox_instruments_cache`.
   - Add `input.refresh_upstox_instruments` defaulting to `false`.
   - Add `input.allow_stale_upstox_instruments_cache` defaulting to `true`.
   - Preserve current local `input.upstox_instruments_json` behavior.

3. Wire app startup.
   - If `instrument_source=upstox_json` and a URL is configured, fetch/cache
     before parsing.
   - If no URL is configured, continue reading `upstox_instruments_json`.
   - Resolve URL cache/local paths relative to the config file when relative.
   - Do not persist instruments or create a bot run when source acquisition or
     parsing fails.

4. Docs.
   - README examples for local-file and URL/cache modes.
   - Live checklist item confirming file date/cache source was reviewed.
   - SRS decision update noting URL/cache remains a data-source concern only.

## Jira Breakdown

- `RK9311-178`: parent task, Upstox instrument JSON fetch/cache support.
- `RK9311-179`: planning and impact analysis.
- `RK9311-180`: fetch/cache component.
- `RK9311-181`: config fields and validation.
- `RK9311-182`: app startup wiring.
- `RK9311-183`: documentation and checklist update.

## Verification

Each implementation subtask will have focused tests plus the unified local test
runner before commit:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run_tests.ps1
```
