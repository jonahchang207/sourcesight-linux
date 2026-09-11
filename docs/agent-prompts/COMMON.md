# Common worker instructions

You are implementing product-quality improvements in the existing Linux C++20,
Dear ImGui/OpenGL SourceSight application at /home/corund/sourcesight-linux.
Work directly in your assigned files. Read applicable AGENTS.md first if present.
Inspect existing behavior before changing it. Use apply_patch for edits.

Preserve all existing dirty/untracked work, especially map/player wireframes,
bullet trails, screen capture, current graphite menu and configuration options.
Do not change gameplay capabilities or add evasion, process writes, remote
telemetry, automatic uploads or new online services. Do not claim that an external
overlay can selectively remove game buildings while preserving native gun/HUD.
Keep 10% graphite map panel fill independent of line color; old opaque-fill flag
must remain ignored. Do not modify external submodules, licenses or branding ownership.

Keep work bounded and incremental. Reuse existing components/dependencies.
Avoid speculative rewrites and cosmetic churn outside your task. Own your tests.
No nested agents. No commits, push, package publication or dependency upgrades.
Do not run builds concurrently against the shared build directory: tell the
coordinator when ready. You may perform read-only checks and isolated test builds.

Return: implemented behavior; exact changed paths; tests actually run and results;
unverified limitations; required integration calls/API details. Never report a
test as passing if it only compiled, or claim live-game validation from fixtures.
If you cannot complete a requirement, report the specific remaining work.
