# Changelog

## Unreleased

- Full-map panels use a separate translucent graphite fill; the legacy forced
  opaque fill is ignored. Depth testing suppresses rear surfaces by default.
- Real OpenGL framebuffer regression tests cover panel opacity, X-ray edges,
  camera movement, profile migration and UI compositing.
- Release packaging derives its version from CMake, records binary identity,
  rejects unlabeled dirty builds and refuses to overwrite existing artifacts.
- Shared graphite styling, option-alias section navigation, confirmed visual
  presets and an isolated menu preview with headless screenshots.
- Schema-validated profiles, atomic persistence, previous-good backups and
  sanitized user-invoked diagnostics with render-thread CPU measurements.
- Freshness-aware cache snapshots and joined engine shutdown; local input is
  skipped when no usable local player is present.
- Product-quality regression suites and reusable Luna/Terra work prompts.
  Live-game verification and automatic process reattachment remain outstanding.

## 0.6.0

See [the original release notes](RELEASE_NOTES_v0.6.0.md). Subsequent source-tree
changes are not included in the already published v0.6.0 binary.
