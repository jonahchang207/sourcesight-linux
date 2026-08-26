#!/bin/bash
# extract_maps.sh — Extract CS2 map collision geometry for SourceSight vis check
#
# Prerequisites:
#   1. source2viewer CLI (https://github.com/ValveResourceFormat/ValveResourceFormat)
#      Build from source or download release, place in PATH or set S2V_PATH
#   2. cs2-map-parser (https://github.com/AtomicBool/cs2-map-parser)
#      Build from source, place vphys_parser in PATH or set PARSER_PATH
#
# Usage:
#   ./extract_maps.sh                    # extracts all known competitive maps
#   ./extract_maps.sh de_dust2 de_mirage # extract specific maps only
#
# Output: maps/ directory next to the executable with .tri files

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${PROJECT_DIR}/maps"

# Auto-detect CS2 install path
CS2_PATHS=(
    "$HOME/.local/share/Steam/steamapps/common/Counter-Strike Global Offensive/game/csgo"
    "$HOME/.steam/steam/steamapps/common/Counter-Strike Global Offensive/game/csgo"
    "/usr/share/Steam/steamapps/common/Counter-Strike Global Offensive/game/csgo"
    "/opt/steam/steamapps/common/Counter-Strike Global Offensive/game/csgo"
)

CS2_MAPS=""
for p in "${CS2_PATHS[@]}"; do
    if [[ -d "$p/maps" ]]; then
        CS2_MAPS="$p/maps"
        echo "[+] Found CS2 maps at: $CS2_MAPS"
        break
    fi
done

if [[ -z "$CS2_MAPS" ]]; then
    echo "[-] Could not find CS2 maps directory. Please set CS2_MAPS env var."
    exit 1
fi

# Tool paths
S2V="${S2V_PATH:-source2viewer}"
PARSER="${PARSER_PATH:-vphys_parser}"

# Check tools
for tool in "$S2V" "$PARSER"; do
    if ! command -v "$tool" &>/dev/null; then
        echo "[-] Required tool not found: $tool"
        echo "    Install source2viewer: https://github.com/ValveResourceFormat/ValveResourceFormat"
        echo "    Install cs2-map-parser: https://github.com/AtomicBool/cs2-map-parser"
        exit 1
    fi
done

mkdir -p "$OUTPUT_DIR"
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT

# Maps to extract (competitive pool + user's favorites)
MAPS=(
    "de_dust2"
    "de_mirage"
    "de_inferno"
    "de_overpass"
    "de_nuke"
    "de_ancient"
    "de_anubis"
    "de_vertigo"
    "cs_office"
)

# If specific maps were passed as arguments, use those instead
if [[ $# -gt 0 ]]; then
    MAPS=("$@")
fi

echo "[*] Extracting collision geometry for ${#MAPS[@]} maps..."
echo ""

for MAP in "${MAPS[@]}"; do
    VPK="${CS2_MAPS}/${MAP}.vpk"
    OUT_TRI="${OUTPUT_DIR}/${MAP}.tri"

    if [[ -f "$OUT_TRI" ]]; then
        echo "[=] ${MAP}: already exists ($(du -h "$OUT_TRI" | cut -f1)), skipping"
        continue
    fi

    if [[ ! -f "$VPK" ]]; then
        echo "[!] ${MAP}: VPK not found at ${VPK}, skipping"
        continue
    fi

    echo "[>] ${MAP}: extracting..."

    # Step 1: Extract world_physics.vphys_c from the VPK
    PHYS_FILE="${TEMP_DIR}/${MAP}_world_physics.vphys_c"
    "$S2V" extract "$VPK" "maps/${MAP}/world_physics.vphys_c" -o "$TEMP_DIR" 2>/dev/null || {
        # Try alternate path
        "$S2V" extract "$VPK" "${MAP}/world_physics.vphys_c" -o "$TEMP_DIR" 2>/dev/null || {
            echo "[!] ${MAP}: failed to extract world_physics.vphys_c"
            continue
        }
    }

    # Find the extracted file (source2viewer may put it in a subdirectory)
    PHYS_FILE=$(find "$TEMP_DIR" -name "world_physics*" -type f | head -1)
    if [[ -z "$PHYS_FILE" ]]; then
        echo "[!] ${MAP}: world_physics file not found after extraction"
        continue
    fi

    # Step 2: Decompress the .vphys_c (source2viewer may already do this)
    DECOMPRESSED="${TEMP_DIR}/${MAP}_world_physics.vphys"
    if [[ "$PHYS_FILE" == *".vphys_c" ]]; then
        "$S2V" decompress "$PHYS_FILE" -o "$DECOMPRESSED" 2>/dev/null || cp "$PHYS_FILE" "$DECOMPRESSED"
    else
        cp "$PHYS_FILE" "$DECOMPRESSED"
    fi

    # Step 3: Convert to .tri using the map parser
    "$PARSER" "$DECOMPRESSED" "$OUT_TRI" 2>/dev/null || {
        echo "[!] ${MAP}: conversion to .tri failed"
        continue
    }

    if [[ -f "$OUT_TRI" ]]; then
        SIZE=$(du -h "$OUT_TRI" | cut -f1)
        echo "[+] ${MAP}: OK (${SIZE})"
    else
        echo "[!] ${MAP}: .tri file not created"
    fi

    # Clean up extracted files for this map
    rm -f "$TEMP_DIR"/${MAP}_*
done

echo ""
echo "[*] Done. .tri files are in: ${OUTPUT_DIR}"
echo "[*] Restart SourceSight to load the new maps."
