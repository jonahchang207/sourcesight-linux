# Verification and release procedure

Releases are maintainer-invoked. Changing a workflow does not publish a release.
Use CMakeLists.txt as the source of the application version. Update CHANGELOG.md
when changing that version; distinguish unreleased source work from old downloads.

## Build and verify

Configure a Release build and compile it, then run the documented regression
suites. Headless OpenGL tests use EGL; CI selects Mesa software rendering via
`LIBGL_ALWAYS_SOFTWARE=1`. Inspect the offline preview at both small and large
window sizes. Live CS2 behavior must be verified separately before claiming it.

Run `node scripts/package-release.mjs` from a clean, reviewed source tree after
tests pass. It produces a versioned directory under `build/release/`, containing
an archive and SHA256SUMS. Existing output directories are never overwritten.
The archive contains BUILD.json with source revision, dirty state, version and
binary SHA-256. This records identity, not proof that the binary was built from
the stated tree; the controlled CI build provides that provenance.

For local testing only, `--allow-dirty` creates an explicitly labeled development
artifact. Do not upload it as a stable release. Profiles, logs, credentials and
game-derived map files are excluded. Checksum verification is not a digital
signature and cannot authenticate a compromised download source.

## Install and rollback

1. Verify the archive with `sha256sum -c SHA256SUMS` beside the downloaded files.
2. Extract each version into its own directory; do not overwrite a working copy.
3. Close the app and back up your `configs/` directory before trying a new version.
4. Copy profiles into the new version's working directory only when wanted.
5. To roll back, close the new app and launch the prior extracted binary from its
   own directory, using the pre-upgrade profile backup. Do not assume an older
   binary understands a newer configuration schema.

The binary is dynamically linked. Check compatibility on the target distribution;
an Arch build is not a universal Linux binary. No automatic replacement, restart,
profile deletion, or remote upload is performed by the packaging script.
