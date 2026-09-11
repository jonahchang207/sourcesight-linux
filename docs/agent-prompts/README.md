# SourceSight product-quality work plan

This is an implementation plan, not a claim that every item is complete.
Use `COMMON.md` plus the named worker prompt. Luna and Terra are explicitly
selected by the maintainer to keep implementation cost down; the coordinating
agent reviews integration and verification. Do not assume relative prices.

## Ownership

- Luna: menu/theme UX, then isolated offline preview tools.
- Terra: cache lifecycle, diagnostic data, configuration persistence and tests.
- Coordinator: build/CI/release scripts, documentation, integration verification.

Luna and Terra may work in parallel, but must not edit each other's files.
They must ask the coordinator for cross-boundary changes. Shared-build writes
are serialized by the coordinator. No agent commits, pushes, publishes a release,
changes licenses, or touches unrelated local work without a new instruction.

## Completion gates

1. Application builds; all pre-existing regression suites still pass.
2. Added behavior has deterministic tests, including error paths.
3. Menu is checked in an offline preview at multiple resolutions.
4. CI runs applicable regression suites before packaging.
5. Release artifacts have version metadata, checksums, and documented rollback.
6. Reports distinguish local tests from live-game verification; deferred work is explicit.

The working tree already contains substantial unreleased changes. Never reset it
or package an old HEAD as if it included these changes.

## Handoff status

Luna and Terra were dispatched with these prompts and both produced implementation
changes. Luna stopped at a usage limit before completing its review; the coordinator
finished integration, fixed initialization and harness-cleanup defects, strengthened
menu tests, and checked rendered previews at 940x600 and 1280x900.

Implemented: shared graphite menu tokens, alias-based section navigation, confirmed
visual presets, empty-state offline menu preview, cache freshness/status, joined
shutdown, schema-validated atomic profiles with backups, sanitized diagnostics,
CI regression gates and safer release packaging. GPU timings, automatic game
reattachment, per-section reset controls and full synthetic match scenes remain
future work. Backup restoration is currently manual, documented in README.md.
No new release was published and no existing work was committed automatically.
