#!/usr/bin/env python3
"""
Unified Asset Extractor for the GitHub-Clean Workspace.

Extracts all Pokemon Emerald (USA/Europe, BPEE) map, tileset, and stage data
directly from a user-supplied legally dumped GBA ROM into assets/world/ and
src/maps_data.c.

Usage:
    python tools/extract_assets.py --rom "emerald.gba"
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EMERALD_DIR = os.path.join(ROOT, "tools", "emerald_maps")


def run_step(desc, cmd, env=None):
    print(f"\n==> {desc}")
    res = subprocess.run(cmd, cwd=ROOT, env=env)
    if res.returncode != 0:
        raise SystemExit(f"[FAILED] {desc} exited with code {res.returncode}")


def main():
    ap = argparse.ArgumentParser(description="Extract Emerald ROM maps & tilesets into assets/world/ and src/maps_data.c")
    ap.add_argument("--rom", required=True, help="Path to your legally dumped Pokemon Emerald (USA/Europe) .gba ROM")
    ap.add_argument("--voxel-dir", help="Optional path to external 3D voxel pack containing buildings.bin and relief.bin")
    args = ap.parse_args()

    rom_path = os.path.abspath(args.rom)
    if not os.path.isfile(rom_path):
        raise SystemExit(f"ROM not found: {rom_path}")

    env = os.environ.copy()
    env["EMERALD_ROM"] = rom_path

    # 1. Extract Emerald tilesets & baked stages (src/maps_data.c) directly from the GBA ROM
    run_step(
        "Extracting Emerald tilesets & stage maps (build_maps.py)",
        [sys.executable, os.path.join(EMERALD_DIR, "build_maps.py"), rom_path, ROOT],
        env=env,
    )

    # 2. Extract Emerald BGM & SFX only if assets/audio/ is missing .wav files
    audio_dir = os.path.join(ROOT, "assets", "audio")
    existing_wavs = [f for f in os.listdir(audio_dir) if f.endswith(".wav")] if os.path.isdir(audio_dir) else []
    audio_script = os.path.join(ROOT, "tools", "extract_audio.py")
    if len(existing_wavs) < 40 and os.path.isfile(audio_script):
        run_step(
            "Extracting Emerald BGM & SFX from ROM (extract_audio.py)",
            [sys.executable, audio_script, rom_path, ROOT],
            env=env,
        )

    # 2. Optional: Export 3D extruded buildings & voxel trees if an external voxel pack directory was provided
    if args.voxel_dir:
        vdir = os.path.abspath(args.voxel_dir)
        bld_bin = os.path.join(vdir, "assets", "voxel", "buildings.bin")
        if not os.path.isfile(bld_bin):
            bld_bin = os.path.join(vdir, "buildings.bin")
        if os.path.isfile(bld_bin) and os.path.isfile(os.path.join(EMERALD_DIR, "export_buildings.py")):
            run_step(
                "Exporting 3D Hoenn buildings (export_buildings.py)",
                [sys.executable, os.path.join(EMERALD_DIR, "export_buildings.py"), bld_bin, ROOT],
                env=env,
            )
        if os.path.isdir(vdir) and os.path.isfile(os.path.join(EMERALD_DIR, "export_world.py")):
            run_step(
                "Exporting 3D relief and trees (export_world.py)",
                [sys.executable, os.path.join(EMERALD_DIR, "export_world.py"), vdir, ROOT],
                env=env,
            )

    print("\n[DONE] Emerald ROM world/map assets extracted into assets/world/ and src/maps_data.c.")


if __name__ == "__main__":
    main()
