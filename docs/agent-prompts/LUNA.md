# Luna: graphite UX and offline preview

Read COMMON.md. Edit only src/gui/frontend/menu/** and new
tools/preview-menu* or tools/test-menu* files. Do not edit configuration, engine,
renderer/window, CI, root CMake, docs or other agents' tests.

Implement these sequentially:

1. Consolidate the current graphite palette, spacing, radii and typography into
   Theme.hpp (inspect its existing consumers first). Remove conflicting legacy
   glass/sapphire styling from the menu without breaking overlay consumers.
   Keep one deliberate accent, readable disabled text, coherent button states,
   consistent padding and existing category icons. Do not replace ImGui or add
   a web stack. Preserve the existing feature layout unless needed for usability.
2. Add working, clearly scoped settings search/navigation. Prefer a small
   searchable index of section names AND important option aliases; selecting a
   result navigates to the correct tab/section. Do not advertise full-text search
   if it only filters tab names. Empty/no-result/clear states must work.
3. Disable controls inapplicable to the current mode (e.g. CPU edge budget and
   distance in full-map mode). Make advanced controls collapsible where useful.
   Add explicit, confirmed per-section reset or named visual quality presets
   without silently enabling automation. Do not overwrite unrelated settings.
   Use existing public config APIs only; ask coordinator if a new one is needed.
4. Add an isolated, reproducible offline menu preview/test tool using existing
   ImGui/OpenGL/GLFW infrastructure, with no CS2 attachment, updater, network,
   input injection, screen capture or user-profile writes. Use synthetic data or
   empty-state fixtures honestly. Support at least 940x600 and 1280x900 and
   document invocation in your report for coordinator integration.
   Headless smoke tests should check style/ID stack balance and finite geometry
   across every tab, and search/reset behavior as practical. A visible preview
   mode should let the maintainer inspect actual widgets without the game.

Ask before changing ownership boundaries. Report small completed milestones;
do not spend the entire task redesigning one card. Prioritize functional search,
consistent graphite and a reproducible preview over extra animation or new logos.
