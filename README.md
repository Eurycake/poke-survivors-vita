# poke-survivors-vita

A Vampire Survivors style fangame for the PS Vita written in C (`vitaGL` + `VitaSDK`). Also builds on Windows for quick testing.

Everything written for this project is open-source (MIT) and free to use — just include a reference/credit if you reuse parts of it.

No ROMs are included here. You need to supply your own Pokemon Emerald (USA/Europe) `.gba` ROM to extract the maps and tilesets.

## PS Vita Setup & Required Plugins

Before running the game on your PS Vita, make sure you have:

1. **`libshacccg.suprx` (Required)** — needed by `vitaGL` for shader compilation.
   - **Easiest way:** Install and open [VitaDB Downloader](https://github.com/Rinnegatamante/VitaDB-Downloader/releases) on your Vita — it will automatically set up `libshacccg.suprx` for you on first launch.
   - **Alternative:** Download and run [ShaRKBR33D](https://github.com/Rinnegatamante/ShaRKBR33D/releases) once on your Vita, which extracts `libshacccg.suprx` to `ur0:data/external/libshacccg.suprx`.
2. **Overclock Plugin (Optional, Recommended)** — the game already requests 444 MHz CPU / 222 MHz GPU on boot, and supports 500 MHz if you have [PSVshell](https://github.com/Electry/PSVshell/releases) (or PSVshellPlus) installed.
   - To install PSVshell: copy `PSVshell.skprx` to `ur0:tai/` and add `ur0:tai/PSVshell.skprx` under `*KERNEL` in `ur0:tai/config.txt`, then reboot (or install it via AutoPlugin II).

## Building for PS Vita

You'll need on your PC:
- Python 3 (added to PATH)
- [VitaSDK](https://vitasdk.org/) installed at `C:\vitasdk` with `vitaGL` (`vdpm install vitaGL`)

Steps:
1. Drop your Pokemon Emerald (USA/Europe) ROM into the root folder (e.g. `emerald.gba`, any `.gba` filename works).
2. Double-click `BUILD.bat`.
3. When it finishes, install `dist\poke-survivors vita.vpk` with VitaShell and copy `dist\data\poke-survivors vita\` to `ux0:data/poke-survivors vita/`.

## Building for Windows (optional)

If you have Visual Studio 2022 (`cl.exe`) installed:
```cmd
python compile.py --target pc
```
This outputs `poke-survivors vita.exe`.

## Credits & References
- Pokemon sprites: [PMDCollab/SpriteCollab](https://github.com/PMDCollab/SpriteCollab) (CC BY-NC)
- 3D Hoenn buildings, trees and relief: adapted from [HctrRB/pokeemerald-vita-3ds](https://github.com/HctrRB/pokeemerald-vita-3ds) (CC BY-NC-SA 4.0)
- Emerald BGM: Pokemon Emerald OST (Game Freak / Nintendo / Creatures Inc.), reference: [pret/pokeemerald](https://github.com/pret/pokeemerald)
- UI, VFX, world props, SFX and LiveArea art: custom made for this project, free to reuse with credit
- Libraries & fonts: VitaSDK, vitaGL, stb, m6x11plus and Chakra Petch (SIL OFL)

## Contributors
- **Eurycake**
- **Antigravity (Gemini)**
- **Claude**

Thanks for checking it out!
