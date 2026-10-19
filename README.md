# Stunt Double: Reignition

A PC port of **Stuntman: Ignition** (Xbox 360, 2007) built by static
recompilation with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).
The game's PowerPC executable is translated ahead of time into C++ and
compiled natively: no emulator, no JIT.

> **Status: early development.** Menus, career and races play; expect bugs.

This repository contains **no game files** and no recompiled game code. You
need your own legally dumped copy of the Xbox 360 disc.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 or 2026 with *Desktop development with C++*, the
  Windows SDK, *C++ Clang Compiler for Windows* and *C++ CMake tools*
  (provides Clang, CMake and Ninja)
- Git, Python 3, PowerShell 7
- .NET 10 SDK, for the launcher (optional: without it only the game is built)
- A GPU with Direct3D 12 support
- Your own ISO of Stuntman: Ignition for Xbox 360

## Building

1. Extract your ISO into `game\` so that `game\default.xex` exists, e.g. with
   [extract-xiso](https://github.com/XboxDev/extract-xiso):
   ```powershell
   extract-xiso -d game -x "Stuntman - Ignition.iso"
   ```
   (`game` can also be a directory junction to an existing extraction.)
2. Set up the SDK (clones ReXGlue v0.10.0, applies `patches\sdk`, builds the
   `rexglue` CLI):
   ```powershell
   .\setup.ps1
   ```
3. Configure and build (codegen runs as part of the build):
   ```powershell
   . .\scripts\devenv.ps1
   cmake --preset win-amd64-relwithdebinfo -DREXSDK_DIR=thirdparty/rexglue-sdk
   cmake --build out\build\win-amd64-relwithdebinfo
   ```
4. Start `out\build\win-amd64-relwithdebinfo\StuntmanLauncher.exe`, pick
   your settings and press **Play**. Or run the game directly from the
   repository root:
   ```powershell
   .\out\build\win-amd64-relwithdebinfo\stuntman.exe --log_file=stuntman.log
   ```

## Launcher

`StuntmanLauncher.exe` sits next to `stuntman.exe` and writes `stuntman.toml`,
the settings file the game loads at startup:

- Display mode and resolution, including ultrawide (21:9, 32:9) and 16:10.
  The game is 16:9: on other screens it is shown with black bars or
  stretched. A true ultrawide view needs a game patch (not done yet).
- Supersampling, VSync, texture filtering, CAS / FSR output filter
- Game language (English, French, German, Spanish, Italian)
- Frame rate: 30 (original) or 60 FPS
- Log file
- Achievements: which ones you unlocked and when. The game exports their
  text and icons from its executable at each start, so the list appears
  after the first launch.
- Saves: career and replays at a glance, delete single replays, back up and
  restore (zips in `Documents\stuntman\backups`), start over from scratch
  or reset achievements. A backup is made before anything is deleted.

Its interface is in English or Italian, following Windows. In game, F1 opens
a smaller settings menu over the same file.

## Layout

| Path | Contents |
|---|---|
| `stuntman_manifest.toml` | Codegen manifest: function boundaries, switch tables, mid-asm hooks |
| `src/` | Host code: app setup, game-specific hooks, the F1 settings menu |
| `launcher/` | The launcher (C#, WPF) |
| `generated/` | Codegen output (not committed) |
| `patches/sdk/` | Our patches on top of ReXGlue v0.10.0 (function discovery for MSVC C++ code, FPSCR fix) |
| `scripts/devenv.ps1` | Loads the Visual Studio toolchain into the shell |
| `docs/NOTES.md` | Findings about the game and the state of the port |
| `tools/` | Analysis helpers |

## License

The code in this repository is under the [MIT License](LICENSE), except the
patches in `patches/sdk/`: they modify the ReXGlue SDK and follow its BSD
3-Clause license.

Neither covers Stuntman: Ignition. Its executable and assets belong to their
respective owners and are never included here; you need your own copy.

## Credits

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), built on
  [Xenia](https://github.com/xenia-project/xenia) and
  [XenonRecomp](https://github.com/hedge-dev/XenonRecomp)

Stuntman: Ignition is a trademark of its respective owners. This project is
not affiliated with or endorsed by them, nor by Microsoft or Xbox.
