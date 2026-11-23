#!/usr/bin/env python3
"""
BYO-ROM Asset Extractor & Compiler for poke-survivors vita.

Requires your own legally dumped Pokemon Emerald (USA/Europe, BPEE) GBA ROM.
Extracts all world/map/3D building/tree assets from the ROM, prunes unused
stage binaries, generates fallback LiveArea PNGs if missing, and compiles
the Windows PC (.exe) and/or PlayStation Vita (.vpk) builds.

Usage:
    python compile.py --rom "path/to/emerald.gba"
    python compile.py --rom "emerald.gba" --target vita
    python compile.py --rom "emerald.gba" --target pc
    python compile.py --rom "emerald.gba" --target extract-only
"""
import argparse
import glob
import os
import shutil
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.join(ROOT, "tools")


def find_local_rom():
    preferred = os.path.join(ROOT, "emerald.gba")
    if os.path.isfile(preferred):
        return preferred
    candidates = []
    for pat in (os.path.join(ROOT, "*.gba"), os.path.join(ROOT, "rom", "*.gba")):
        candidates.extend(glob.glob(pat))
    return candidates[0] if candidates else None


def validate_emerald_rom(rom_path):
    if not os.path.isfile(rom_path):
        raise SystemExit(f"[ERROR] ROM file not found: {rom_path}")
    size = os.path.getsize(rom_path)
    if size < 16 * 1024 * 1024:
        raise SystemExit(f"[ERROR] File is too small to be a Pokemon Emerald GBA ROM ({size} bytes).")
    with open(rom_path, "rb") as f:
        f.seek(0xA0)
        title = f.read(12).decode("ascii", errors="replace").strip("\x00 ")
        code = f.read(4).decode("ascii", errors="replace")
    if code != "BPEE":
        raise SystemExit(
            f"[ERROR] Unsupported GBA ROM header: title='{title}', code='{code}'.\n"
            "        Expected Pokemon Emerald (USA/Europe) with game code 'BPEE'."
        )
    print(f"[OK] Verified ROM: {os.path.basename(rom_path)} (title='{title}', code='{code}', {size // 1048576} MB)")


def prune_unused_world_files():
    wdir = os.path.join(ROOT, "assets", "world")
    if not os.path.isdir(wdir):
        return
    dead_files = [
        "bld_0.bin", "bld_1.bin", "bld_0.png", "bld_1.png",
        "tre_0.bin", "tre_1.bin",
        "wld_0.bin", "wld_1.bin",
        "rel_0.bin", "rel_1.bin",
        "stage_preview_3.png", "stage_preview_4.png",
    ]
    removed = 0
    for name in dead_files:
        p = os.path.join(wdir, name)
        if os.path.isfile(p):
            os.remove(p)
            removed += 1
    for sub in ("medal", "cave", "lab"):
        sp = os.path.join(wdir, sub)
        if os.path.isdir(sp):
            shutil.rmtree(sp)
            removed += 1
    if removed:
        print(f"[OK] Pruned {removed} unused stage files/folders from assets/world/ to keep data slim.")


def ensure_livearea_placeholders():
    """Create minimal 8-bit indexed PNG placeholders if user hasn't provided custom LiveArea art."""
    targets = [
        (os.path.join(ROOT, "sce_sys", "icon0.png"), (128, 128), (24, 96, 168)),
        (os.path.join(ROOT, "sce_sys", "pic0.png"), (960, 544), (16, 28, 48)),
        (os.path.join(ROOT, "sce_sys", "livearea", "contents", "bg.png"), (840, 500), (20, 42, 74)),
        (os.path.join(ROOT, "sce_sys", "livearea", "contents", "startup.png"), (280, 158), (34, 156, 98)),
    ]
    missing = [t for t in targets if not os.path.isfile(t[0])]
    if not missing:
        return
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("[WARN] Pillow not installed; cannot auto-generate placeholder sce_sys PNGs.")
        return
    for path, (w, h), rgb in missing:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        im = Image.new("RGB", (w, h), rgb)
        dr = ImageDraw.Draw(im)
        dr.rectangle([4, 4, w - 5, h - 5], outline=(245, 210, 80), width=3)
        im = im.convert("P", palette=Image.ADAPTIVE, colors=16)
        im.save(path)
        print(f"[INFO] Created placeholder LiveArea image: {os.path.relpath(path, ROOT)}")


def ensure_pokedex_descriptions_header():
    hdr = os.path.join(ROOT, "src", "pokedex_descriptions.h")
    if os.path.isfile(hdr):
        return
    with open(hdr, "w", encoding="utf-8") as f:
        f.write(
            "#ifndef POKEDEX_DESCRIPTIONS_H\n"
            "#define POKEDEX_DESCRIPTIONS_H\n"
            "static inline const char *get_pokedex_description(int dex) {\n"
            "    (void)dex;\n"
            "    return \"A wild Pokemon encountered on your survival journey.\";\n"
            "}\n"
            "#endif\n"
        )
    print("[INFO] Generated clean src/pokedex_descriptions.h header.")


def check_optional_assets():
    mon_dir = os.path.join(ROOT, "assets", "mon")
    mon_cnt = len([f for f in os.listdir(mon_dir) if f.endswith(".png")]) if os.path.isdir(mon_dir) else 0
    aud_dir = os.path.join(ROOT, "assets", "audio")
    aud_cnt = len([f for f in os.listdir(aud_dir) if f.endswith(".wav")]) if os.path.isdir(aud_dir) else 0
    if mon_cnt == 0:
        print("[WARN] assets/mon/ has no Pokemon sprites yet.")
        print("       Download sprites from https://github.com/PMDCollab/SpriteCollab/tree/master/sprite into assets/mon/.")
    else:
        print(f"[OK] Found {mon_cnt} sprite sheets in assets/mon/.")
    if aud_cnt == 0:
        print("[WARN] assets/audio/ has no .wav audio tracks yet (game will run silently until added).")
    else:
        print(f"[OK] Found {aud_cnt} audio files in assets/audio/.")


def has_vitasdk():
    sdk = os.environ.get("VITASDK", r"C:\vitasdk")
    return os.path.isfile(os.path.join(sdk, "bin", "arm-vita-eabi-gcc.exe")) or shutil.which("arm-vita-eabi-gcc") is not None


def has_msvc():
    return os.path.isfile(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat") or shutil.which("cl") is not None


def run_cmd(desc, cmd):
    print(f"\n==> {desc}")
    res = subprocess.run(cmd, cwd=ROOT)
    if res.returncode != 0:
        raise SystemExit(f"[FAILED] {desc} failed with exit code {res.returncode}")


def main():
    ap = argparse.ArgumentParser(description="BYO-ROM Asset Extractor & Compiler for poke-survivors vita")
    ap.add_argument("--rom", help="Path to your legally dumped Pokemon Emerald (USA/Europe) .gba ROM")
    ap.add_argument("--target", choices=["all", "vita", "pc", "extract-only"], default="all",
                    help="Build target: all (default), vita, pc, or extract-only")
    ap.add_argument("--skip-extract", action="store_true",
                    help="Skip ROM extraction (only if assets/world/ and src/maps_data.c are already extracted)")
    ap.add_argument("--keep-all-world", action="store_true",
                    help="Do not prune unused world stage binaries after extraction")
    args = ap.parse_args()

    print("============================================================")
    print("  poke-survivors vita - BYO-ROM Asset Extractor & Compiler  ")
    print("============================================================")

    if not args.skip_extract:
        rom_path = args.rom or find_local_rom()
        if not rom_path:
            print("\nNo Pokemon Emerald (.gba) ROM found in this folder.")
            rom_path = input("Drag & drop your Pokemon Emerald (USA/Europe) .gba ROM here and press Enter: ").strip().strip('"')
        rom_path = os.path.abspath(rom_path)
        validate_emerald_rom(rom_path)
        run_cmd(
            "Extracting world maps, tilesets, 3D buildings, and trees from your ROM",
            [sys.executable, os.path.join(TOOLS_DIR, "extract_assets.py"), "--rom", rom_path],
        )
        if not args.keep_all_world:
            prune_unused_world_files()
    else:
        maps_c = os.path.join(ROOT, "src", "maps_data.c")
        if not os.path.isfile(maps_c):
            raise SystemExit("[ERROR] --skip-extract was set, but src/maps_data.c does not exist. Provide --rom first.")
        print("[INFO] Skipping ROM extraction (--skip-extract).")

    ensure_pokedex_descriptions_header()
    check_optional_assets()

    if args.target == "extract-only":
        print("\n[DONE] Asset extraction complete.")
        return

    want_pc = args.target in ("all", "pc")
    want_vita = args.target in ("all", "vita")

    built_any = False
    if want_pc:
        if has_msvc():
            run_cmd("Compiling Windows PC executable (build_pc.bat)", ["cmd", "/c", "build_pc.bat"])
            built_any = True
        elif args.target == "pc":
            raise SystemExit("[ERROR] Visual Studio 2022 MSVC (cl.exe) not found.")
        else:
            print("[SKIP] Visual Studio 2022 MSVC not found - skipping Windows PC build.")

    if want_vita:
        if has_vitasdk():
            ensure_livearea_placeholders()
            run_cmd("Compiling PlayStation Vita VPK (build_vita_win.bat)", ["cmd", "/c", "build_vita_win.bat"])
            built_any = True
        elif args.target == "vita":
            raise SystemExit("[ERROR] VitaSDK (arm-vita-eabi-gcc) not found. Install VitaSDK to C:\\vitasdk or set VITASDK.")
        else:
            print("[SKIP] VitaSDK not found - skipping PS Vita build.")

    if not built_any:
        raise SystemExit("[ERROR] No supported compiler toolchain (MSVC or VitaSDK) was found.")

    print("\n============================================================")
    print("  [ALL DONE] Build outputs:")
    if os.path.isfile(os.path.join(ROOT, "poke-survivors vita.exe")):
        print("    PC   : poke-survivors vita.exe")
    if os.path.isfile(os.path.join(ROOT, "dist", "poke-survivors vita.vpk")):
        print("    Vita : dist\\poke-survivors vita.vpk")
        print("    Data : dist\\data\\poke-survivors vita\\  -> copy to ux0:data/poke-survivors vita/")
    print("============================================================")


if __name__ == "__main__":
    main()
