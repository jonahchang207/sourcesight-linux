#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_dir}/build"
config_dir="${XDG_CONFIG_HOME:-${HOME}/.config}/hypr"
rules_file="${config_dir}/sourcesight.conf"

sudo pacman -S --needed --noconfirm base-devel cmake curl glfw-x11 libx11 mesa
cmake -S "${repo_dir}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${build_dir}" --parallel

mkdir -p "${config_dir}"
cat > "${rules_file}" <<'RULES'
# SourceSight Linux overlay rules (Hyprland 0.50+ syntax)
windowrule = float on, match:title ^(SourceSight Linux)$
windowrule = fullscreen on, match:title ^(SourceSight Linux)$
windowrule = pin on, match:title ^(SourceSight Linux)$
windowrule = no_blur on, match:title ^(SourceSight Linux)$
windowrule = no_shadow on, match:title ^(SourceSight Linux)$
windowrule = no_anim on, match:title ^(SourceSight Linux)$
RULES

main_conf="${config_dir}/hyprland.conf"
include_line="source = ${rules_file}"
if [[ -f "${main_conf}" ]] && ! grep -Fqx "${include_line}" "${main_conf}"; then
    printf '\n%s\n' "${include_line}" >> "${main_conf}"
fi
hyprctl reload >/dev/null 2>&1 || true
printf 'Built: %s/sourcesight\n' "${build_dir}"
printf 'Run CS2 in fullscreen-windowed mode, then run that executable.\n'
