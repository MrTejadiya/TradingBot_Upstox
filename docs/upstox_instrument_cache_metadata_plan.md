# Upstox Instrument Cache Metadata Plan

## Requirement

Write reviewable metadata whenever the Upstox instrument JSON URL/cache flow
successfully refreshes the local cache. Operators should be able to confirm the
source URL, refresh time, payload size, and HTTP status before live trading.

## Scope

Version 1 will add a sidecar metadata file next to the configured cache path.
For a cache path such as:

```text
data/upstox_complete.cache.json
```

the metadata path will be:

```text
data/upstox_complete.cache.json.metadata.json
```

The metadata file is written only after a successful download and successful
cache write. Reading an existing cache without refresh does not rewrite
metadata.

## Metadata Fields

- `source_url`: configured Upstox instrument JSON URL.
- `cache_path`: local cache path that was written.
- `refreshed_at_utc`: UTC timestamp when the cache was refreshed.
- `status_code`: HTTP status from the successful download.
- `bytes`: downloaded body size in bytes.

## Safety Behavior

- Metadata write failure should fail the refresh so operators do not get a cache
  file without review metadata.
- Existing local-file mode remains unchanged.
- Existing stale-cache fallback remains unchanged; fallback to an older cache is
  allowed only when configured.
- No broker order placement behavior changes.

## Implementation Impact

1. Extend `UpstoxInstrumentJsonCache`.
   - Add default metadata sidecar path derivation.
   - Write metadata after successful cache write.
   - Surface metadata path in the result.
   - Add tests for metadata fields, no metadata rewrite on cache hit, and
     refresh failure when metadata cannot be written.

2. Docs.
   - README: explain metadata sidecar path and fields.
   - Live checklist: require metadata review for URL/cache mode.
   - SRS decision: include metadata as part of the URL/cache audit trail.

## Jira Breakdown

- `RK9311-184`: parent task, cache refresh metadata.
- `RK9311-185`: planning and impact analysis.
- `RK9311-186`: metadata write implementation and tests.
- `RK9311-187`: documentation and checklist updates.

## Verification

Each code subtask will run focused tests plus:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run_tests.ps1
```
