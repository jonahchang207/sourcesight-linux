# 🎯 SourceSight

External ESP overlay for Counter-Strike 2, built for **Omarchy Linux** (Hyprland) with a modern, clean codebase. Read-only overlay — no injection, no memory writes.

> **Omarchy Linux port:** Linux platform support is under active development on the `omarchy-port` branch. The platform, renderer, and build layers are present; Linux-specific CS2 signatures still require validation against the current native game build before gameplay use.

## 🖥️ Omarchy Linux

SourceSight targets Omarchy's Hyprland session through GLFW's X11 backend (XWayland), OpenGL 3, and Linux `process_vm_readv`. It does not inject a library or write into the game process.

```bash
git clone --recursive -b omarchy-port https://github.com/jonahchang207/sourcesight-linux.git
cd sourcesight-linux
chmod +x scripts/install-omarchy.sh
./scripts/install-omarchy.sh
```

Use CS2's fullscreen-windowed mode. Press `Insert` or right `Shift` to toggle the menu and `End` to save and exit. If Linux denies memory reads, do not run the overlay as root; launch it as the same user/session as Steam and inspect your distribution's `kernel.yama.ptrace_scope` policy.

## 🎬 Showcase

[![SourceSight](.github/showcase.png)](https://youtu.be/3WHHLUyHyzA)

## ✨ Features

- External, read-only ESP: box, skeleton, head tracker, health, armor, name, money, weapon, ammo, ping, and team/enemy flags
- Bomb ESP, radar, spectator list, and velocity graph
- Automatic offset scanning to stay compatible through game updates
- Click-based UI (Dear ImGui) with streamproof and watermark options
- Builds on Windows (Visual Studio) and Linux (CMake)

## ⚠️ Important

> This project is provided *'as is'* for learning purposes with no warranties or responsibility from the developer. Use it at your own risk; you are the only one accountable for your actions.

- **Detection Status:** This project is intended solely for single-player use. That said, no ban reports have been raised for other modes.
- **Anti-Virus Alerts:** This software may resemble malware in behavior because it accesses other processes' memory, so it is commonly flagged by anti-virus programs. Read the source code and build it yourself — all binaries are compiled via the [GitHub workflows](.github/workflows/) from this repository's source.

## 🛠️ Developer Instructions

### Linux (Omarchy)

```bash
git clone --recursive -b omarchy-port https://github.com/jonahchang207/sourcesight-linux
cd sourcesight-linux
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Windows

1. Clone the repository with submodules: `git clone --recursive -b omarchy-port https://github.com/jonahchang207/sourcesight-linux`
   - If you cloned before submodules were added, run `git submodule update --init --recursive`
2. Build using **Visual Studio 2022** (or later): Build **`x64 - Release`**
3. Locate your binary in `<arch>/<configuration>`, e.g. `x64/Release`.

## 🔖 License & Copyright

© 2026 **Jonah Chang**. All rights reserved. See the [LICENSE](LICENSE) file for details.
