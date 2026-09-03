#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_dir}/build"
config_dir="${XDG_CONFIG_HOME:-${HOME}/.config}/hypr"
rules_file="${config_dir}/sourcesight.conf"

sudo pacman -S --needed --noconfirm base-devel cmake curl glfw-x11 libx11 libxrandr mesa ttf-dejavu linux-headers
cmake -S "${repo_dir}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${build_dir}" --parallel

# Optional: kernel-level virtual mouse used by the aim feature.
# Builds and loads /dev/person-mouse, then requires a re-login.
if [[ -x "${repo_dir}/drivers/install.sh" ]] && [[ -c /dev/person-mouse ]]; then
    echo "person-mouse driver already loaded (/dev/person-mouse present)"
elif [[ -x "${repo_dir}/drivers/install.sh" ]]; then
    echo ""
    echo "Kernel mouse driver not installed yet. To enable the aim feature, run:"
    echo "  sudo ${repo_dir}/drivers/install.sh"
    echo "then log out and back in."
    echo ""
fi

mkdir -p "${config_dir}"

# Omarchy 0.55+ uses a Lua config (hyprland.lua); older setups use hyprlang
# (hyprland.conf). Write the overlay rules in the matching format and source
# them. Note: fullscreen is intentionally omitted - float + pin keeps the
# surface above CS2 without fullscreen-specific input routing.
lua_conf="${config_dir}/hyprland.lua"
lua_rules="${config_dir}/sourcesight.lua"
hyprlang_rules="${config_dir}/sourcesight.conf"

if [[ -f "${lua_conf}" ]]; then
    cat > "${lua_rules}" <<'LUA'
-- SourceSight Linux ESP overlay: fullscreen transparent click-through surface.
-- Kept above CS2, never takes focus, no decorations/blur/anim.
o.window({ class = "^SourceSight Linux$" }, {
  float = true,
  fullscreen = true,
  pin = true,
  no_focus = true,
  no_blur = true,
  no_shadow = true,
  no_anim = true,
})
LUA
    if ! grep -Fq 'require("hypr.sourcesight")' "${lua_conf}"; then
        printf '\n-- SourceSight overlay window rules (click-through ESP surface).\nrequire("hypr.sourcesight")\n' >> "${lua_conf}"
    fi
else
    cat > "${hyprlang_rules}" <<'RULES'
# SourceSight Linux overlay rules (Hyprland 0.50+ syntax)
windowrule = float on, match:title ^(SourceSight Linux)$
windowrule = fullscreen on, match:title ^(SourceSight Linux)$
windowrule = pin on, match:title ^(SourceSight Linux)$
windowrule = no_blur on, match:title ^(SourceSight Linux)$
windowrule = no_shadow on, match:title ^(SourceSight Linux)$
windowrule = no_anim on, match:title ^(SourceSight Linux)$
RULES

    main_conf="${config_dir}/hyprland.conf"
    include_line="source = ${hyprlang_rules}"
    if [[ -f "${main_conf}" ]] && ! grep -Fqx "${include_line}" "${main_conf}"; then
        printf '\n%s\n' "${include_line}" >> "${main_conf}"
    fi
fi
hyprctl reload >/dev/null 2>&1 || true

# Second layer of the blur/shadow exclusion: apply the rule immediately at the
# compositor level via `hyprctl eval` (the supported runtime mechanism on
# Hyprland 0.55+, where the legacy `windowrulev2` hyprlang path no longer works).
# This de-frosts the overlay right away, independent of the config rule above.
# Runs on X11/XWayland and native Wayland alike; harmless if `hyprctl` is absent.
if command -v hyprctl >/dev/null 2>&1; then
    hyprctl eval 'o.window({ class = "^SourceSight Linux$" }, { no_blur = true, no_shadow = true, no_focus = true, pin = true, float = true })' >/dev/null 2>&1 \
        && echo "Applied SourceSight blur/shadow exclusion (hyprctl eval)" \
        || echo "Could not apply blur/shadow exclusion via hyprctl eval (ignoring)"
else
    echo "hyprctl not found; blur/shadow exclusion applied via the config rule on next Hyprland reload"
fi

printf '\nBuilt: %s/sourcesight\n' "${build_dir}"
printf 'Run CS2 in fullscreen-windowed mode, then run that executable.\n'
