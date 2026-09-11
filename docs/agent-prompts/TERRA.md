# Terra: reliability, recoverable profiles and diagnostics

Read COMMON.md. Edit only src/config/**, src/core/engine/cache/**,
new src/core/diagnostics/**, and new tools/test-config* or
tools/test-runtime* files. Other engine/frontend/main/window files are read-only;
send the coordinator a precise integration request if changes there are needed.

Implement these bounded improvements:

1. Inspect cache refresh failure paths. Prevent stale players/local data being
   presented as valid after disconnect, invalid local player, map transition or
   failed reads. Publish coherent snapshots with a timestamp/generation and
   explicit readiness/failure status; avoid races and unbounded busy loops.
   Preserve legitimate spectator behavior. Add deterministic tests or a small
   injectable seam for lifecycle transitions without the game/driver.
2. Make config/profile writes recoverable: explicit schema version, deterministic
   migration of older profiles, atomic same-directory replacement, previous-good
   backup and helpful error state. A malformed/truncated profile must not leave
   partially applied globals or overwrite the last good profile. Protect profile
   names/path boundaries; preserve unknown fields when reasonable. Reject future
   schema versions safely. Legacy wireframe_occlude_game stays ignored.
   Avoid a giant config rewrite; cover existing flat keys and backwards compatibility.
3. Expose lightweight diagnostics for cache freshness/state and configuration
   failures through a small thread-safe API. Add a user-invoked sanitized JSON
   report builder/export API with explicit destination and error reporting. Include
   version/build identity if available, status and aggregate timing only. Exclude
   usernames, paths, addresses, tokens, player names/IDs, memory dumps and screenshots.
   No automatic exports/network calls. Do not fabricate GPU timings: allow the
   coordinator to supply measured CPU/GPU fields or mark unavailable.
4. Test corrupt profile recovery, missing keys, future versions, numeric boundaries,
   successful round trips, write failure preserving old file, and diagnostic
   redaction/freshness transitions. Reuse the existing build-link test pattern if
   needed; do not mutate real user profiles. Export integration API details early
   so the coordinator can wire menu status without guessing.

Send precise follow-up requests for any integration outside your ownership.
Report incomplete behaviors honestly; do not claim reconnect is fully handled
if only a status flag was added.
