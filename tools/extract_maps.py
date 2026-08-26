#!/usr/bin/env python3
"""
extract_maps.py — Extract CS2 map collision geometry for SourceSight vis check

Reads CS2 .vpk map files and extracts collision mesh triangles into .tri files
that the MapRaytrace module uses for line-of-sight checking.

Requirements:
    pip install valve-resource-format   (or build from source)

Usage:
    python3 extract_maps.py                          # extract all competitive maps
    python3 extract_maps.py de_dust2 de_mirage       # extract specific maps
    python3 extract_maps.py --cs2-path /path/to/csgo # specify CS2 install

Output:
    maps/ directory with .tri files (e.g. maps/de_dust2.tri)
"""

import sys
import os
import struct
import glob
import subprocess
from pathlib import Path

# ─── Configuration ───────────────────────────────────────────────────

COMPETITIVE_MAPS = [
    "de_dust2", "de_mirage", "de_inferno", "de_overpass",
    "de_nuke", "de_ancient", "de_anubis", "de_vertigo", "cs_office",
]

# Possible CS2 install locations on Linux
CS2_SEARCH_PATHS = [
    os.path.expanduser("~/.local/share/Steam/steamapps/common/Counter-Strike Global Offensive/game/csgo"),
    os.path.expanduser("~/.steam/steam/steamapps/common/Counter-Strike Global Offensive/game/csgo"),
    "/usr/share/Steam/steamapps/common/Counter-Strike Global Offensive/game/csgo",
    "/opt/steam/steamapps/common/Counter-Strike Global Offensive/game/csgo",
]

SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_DIR = SCRIPT_DIR.parent
OUTPUT_DIR = PROJECT_DIR / "maps"


# ─── VPK reader (simplified) ─────────────────────────────────────────

class VPKReader:
    """Minimal VPK reader to extract files from CS2 .vpk archives."""

    def __init__(self, vpk_path):
        self.path = vpk_path
        self.f = open(vpk_path, "rb")
        header = self.f.read(4)
        if header != b"VPK\x20":
            raise ValueError(f"Not a VPK file: {vpk_path}")
        self.version = struct.unpack("<I", self.f.read(4))[0]
        self.tree_size = struct.unpack("<I", self.f.read(4))[0]
        # Skip remaining header fields
        if self.version == 2:
            self.f.read(8)  # filedata_section_size, archive_md5_section_size
        self.tree_start = self.f.tell()
        self._read_tree()

    def _read_tree(self):
        """Read the directory tree (simplified: only reads ext + path + filename entries)."""
        self.files = {}
        self.f.seek(self.tree_start)

        while True:
            ext = self._read_null_terminated()
            if not ext:
                break
            while True:
                path = self._read_null_terminated()
                if not path:
                    break
                while True:
                    filename = self._read_null_terminated()
                    if not filename:
                        break
                    full_path = f"{path}/{filename}.{ext}" if path else f"{filename}.{ext}"
                    # Read file entry
                    crc = struct.unpack("<I", self.f.read(4))[0]
                    small_data_size = struct.unpack("<H", self.f.read(2))[0]
                    archive_index = struct.unpack("<H", self.f.read(2))[0]
                    offset = struct.unpack("<I", self.f.read(4))[0]
                    size = struct.unpack("<I", self.f.read(4))[0]
                    terminator = struct.unpack("<H", self.f.read(2))[0]

                    self.files[full_path] = {
                        "archive": archive_index,
                        "offset": offset,
                        "size": size,
                        "small_data_size": small_data_size,
                    }

                    if small_data_size > 0:
                        self.f.read(small_data_size)

    def _read_null_terminated(self):
        """Read a null-terminated string."""
        result = b""
        while True:
            byte = self.f.read(1)
            if not byte or byte == b"\x00":
                return result.decode("utf-8", errors="replace")
            result += byte

    def read_file(self, entry_name):
        """Read a file from the VPK. Returns bytes or None."""
        if entry_name not in self.files:
            return None

        entry = self.files[entry_name]
        if entry["archive"] == 0x7FFF:
            # File is in the directory VPK itself
            self.f.seek(self.tree_start + entry["offset"])
            return self.f.read(entry["size"])
        else:
            # File is in a separate archive VPK
            archive_path = str(self.path).replace(".vpk", f"_{entry['archive']:03d}.vpk")
            if not os.path.exists(archive_path):
                return None
            with open(archive_path, "rb") as af:
                af.seek(entry["offset"])
                return af.read(entry["size"])

    def close(self):
        self.f.close()


# ─── VPhys parser (simplified: extracts collision mesh triangles) ─────

def parse_vphys_text(data):
    """
    Parse a decompressed .vphys text file and extract collision mesh triangles.
    The .vphys format is KV3 (KeyValues3) with embedded binary data.
    This parser extracts the triangle vertex data from the mesh sections.
    """
    triangles = []
    text = data.decode("utf-8", errors="replace")

    # Find all "m_vVertices" arrays — these contain the triangle vertex positions
    # Format: "m_vVertices" [ array of { x, y, z } ... ]
    import re

    # Look for vertex data patterns in the KV3 text
    # The vertices are typically in a binary blob after "m_inpPrevInputState"
    # or in text arrays like: { "x" = "123.45" "y" = "678.90" "z" = "11.22" }

    # Pattern 1: Text-based vertex arrays
    vertex_pattern = re.compile(
        r'"m_vVertices"\s*\[(.*?)\]',
        re.DOTALL
    )

    for match in vertex_pattern.finditer(text):
        block = match.group(1)
        # Extract x, y, z triplets
        xyz_pattern = re.compile(
            r'"x"\s*=\s*"([^"]+)"\s+"y"\s*=\s*"([^"]+)"\s+"z"\s*=\s*"([^"]+)"'
        )
        verts = []
        for vm in xyz_pattern.finditer(block):
            try:
                x, y, z = float(vm.group(1)), float(vm.group(2)), float(vm.group(3))
                verts.append((x, y, z))
            except ValueError:
                continue
        # Every 3 vertices form a triangle
        for i in range(0, len(verts) - 2, 3):
            triangles.append(verts[i:i+3])

    # Pattern 2: Binary vertex data (packed floats)
    # Look for "m_inpPrevInputState" or similar binary blobs
    binary_pattern = re.compile(
        r'"(?:m_inpPrevInputState|m_meshes)"\s*=\s*\[\s*binary\s*\|\s*(\d+)\s*\|(.*?)\]',
        re.DOTALL
    )

    for match in binary_pattern.finditer(text):
        blob_size = int(match.group(1))
        blob_text = match.group(2).strip()
        # Binary data is hex-encoded in the KV3 text
        try:
            blob_bytes = bytes.fromhex(blob_text.replace("\n", "").replace(" ", ""))
            # Each vertex is 3 floats (12 bytes)
            num_verts = len(blob_bytes) // 12
            verts = []
            for i in range(num_verts):
                x, y, z = struct.unpack_from("<fff", blob_bytes, i * 12)
                verts.append((x, y, z))
            for i in range(0, len(verts) - 2, 3):
                triangles.append(verts[i:i+3])
        except Exception:
            continue

    return triangles


def try_source2viewer_extract(vpk_path, map_name, temp_dir):
    """
    Try to use source2viewer CLI to extract and decompress the .vphys_c file.
    Returns the decompressed .vphys data as bytes, or None.
    """
    # Try multiple source2viewer binary names
    for binary in ["source2viewer", "Source2Viewer", "source2viewer-cli"]:
        if not subprocess.run(["which", binary], capture_output=True).returncode == 0:
            continue

        try:
            # Extract world_physics.vphys_c
            result = subprocess.run(
                [binary, "extract", str(vpk_path),
                 f"maps/{map_name}/world_physics.vphys_c",
                 "-o", str(temp_dir)],
                capture_output=True, timeout=30
            )

            # Find the extracted file
            for f in glob.glob(str(temp_dir / f"*{map_name}*")):
                if f.endswith(".vphys_c") or f.endswith(".vphys"):
                    # Try to decompress
                    decompressed = str(temp_dir / f"{map_name}_decompressed.vphys")
                    subprocess.run(
                        [binary, "decompress", f, "-o", decompressed],
                        capture_output=True, timeout=30
                    )
                    if os.path.exists(decompressed):
                        with open(decompressed, "rb") as fh:
                            return fh.read()
                    else:
                        with open(f, "rb") as fh:
                            return fh.read()
        except Exception:
            continue

    return None


def extract_map_triangles(vpk_path, map_name, temp_dir):
    """
    Extract collision mesh triangles from a CS2 map .vpk file.
    Returns a list of (p1, p2, p3) tuples where each p is (x, y, z).
    """
    try:
        vpk = VPKReader(vpk_path)
    except Exception as e:
        print(f"  [-] Failed to open VPK: {e}")
        return []

    # Try to read world_physics.vphys_c (compressed)
    phys_data = None
    for path in [
        f"maps/{map_name}/world_physics.vphys_c",
        f"{map_name}/world_physics.vphys_c",
        "world_physics.vphys_c",
    ]:
        phys_data = vpk.read_file(path)
        if phys_data:
            break

    vpk.close()

    if not phys_data:
        print(f"  [-] world_physics.vphys_c not found in VPK")
        return []

    # Try source2viewer for decompression
    decompressed = try_source2viewer_extract(vpk_path, map_name, temp_dir)

    if decompressed:
        triangles = parse_vphys_text(decompressed)
        if triangles:
            return triangles

    # Fallback: try parsing the raw data (may be already decompressed)
    try:
        triangles = parse_vphys_text(phys_data)
        if triangles:
            return triangles
    except Exception:
        pass

    print(f"  [-] Could not parse collision data (try installing source2viewer)")
    return []


def write_tri_file(triangles, output_path):
    """Write triangles to a .tri file (binary: 9 floats per triangle)."""
    with open(output_path, "wb") as f:
        for p1, p2, p3 in triangles:
            f.write(struct.pack("<9f",
                p1[0], p1[1], p1[2],
                p2[0], p2[1], p2[2],
                p3[0], p3[1], p3[2],
            ))


def find_cs2_path():
    """Auto-detect CS2 installation path."""
    for path in CS2_SEARCH_PATHS:
        if os.path.isdir(os.path.join(path, "maps")):
            return path
    return None


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Extract CS2 map collision geometry for SourceSight vis check")
    parser.add_argument("maps", nargs="*", default=COMPETITIVE_MAPS,
                       help="Maps to extract (default: all competitive maps)")
    parser.add_argument("--cs2-path", help="Path to CS2 csgo/ directory")
    parser.add_argument("--output", default=str(OUTPUT_DIR),
                       help="Output directory for .tri files")
    args = parser.parse_args()

    cs2_path = args.cs2_path or find_cs2_path()
    if not cs2_path:
        print("[-] Could not find CS2 installation. Use --cs2-path to specify.")
        sys.exit(1)

    maps_dir = os.path.join(cs2_path, "maps")
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"[*] CS2 path: {cs2_path}")
    print(f"[*] Output: {output_dir}")
    print(f"[*] Maps to extract: {len(args.maps)}")
    print()

    import tempfile
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        success = 0

        for map_name in args.maps:
            out_file = output_dir / f"{map_name}.tri"
            if out_file.exists():
                size = out_file.stat().st_size
                print(f"[=] {map_name}: already exists ({size // 1024}KB), skipping")
                success += 1
                continue

            vpk_path = os.path.join(maps_dir, f"{map_name}.vpk")
            if not os.path.exists(vpk_path):
                print(f"[!] {map_name}: VPK not found at {vpk_path}")
                continue

            print(f"[>] {map_name}: extracting...")
            triangles = extract_map_triangles(vpk_path, map_name, temp_path)

            if triangles:
                write_tri_file(triangles, out_file)
                size = out_file.stat().st_size
                print(f"[+] {map_name}: OK ({len(triangles)} triangles, {size // 1024}KB)")
                success += 1
            else:
                print(f"[!] {map_name}: extraction failed")

    print()
    print(f"[*] Done: {success}/{len(args.maps)} maps extracted")
    print(f"[*] .tri files are in: {output_dir}")
    if success > 0:
        print(f"[*] Restart SourceSight to load the new maps.")


if __name__ == "__main__":
    main()
