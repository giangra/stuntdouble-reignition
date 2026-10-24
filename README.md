# Stunt Double: Reignition

A PC port of **Stuntman: Ignition** (Xbox 360, 2007) built by static
recompilation with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).
The game's PowerPC executable is translated ahead of time into C++ and
compiled natively: no emulator, no JIT.

> **Status: early development.** Menus, career and races play; expect bugs.

This repository contains **no game files** and no recompiled game code, and
neither do its releases. You need your own legally dumped copy of the Xbox
360 disc: the game is built on your PC from it.

## Installing

1. Download `StuntmanSetup-<version>.exe` from
   [Releases](https://github.com/giangra/stuntdouble-reignition/releases)
   and run it.
2. Pick your Stuntman: Ignition ISO and an install folder (default
   `C:\Games\StuntmanIgnition`), then press **Install**.
3. When it's done, start the game from the **Stuntman Ignition** shortcut on
   the desktop. It opens on a start screen where you set video, controls and
   language, look at your achievements, and press **Play**.

The installer checks that the ISO is Stuntman: Ignition, extracts it, and
gets what the build needs if it is missing: the free Microsoft Visual Studio
Build Tools (about 6 GB, asks for administrator rights) and a portable Git.
Then it downloads this project's source and the ReXGlue SDK, translates the
game's code to C++ and compiles it. Count about 12 GB of free space and
30–60 minutes the first time. If it stops, run it again: finished steps are
kept. Its log is `install.log` in the install folder.

### Updating

From 0.3.0 on, the launcher checks GitHub for a newer release when it starts
and shows an **Update to x.y.z** button. It downloads the new setup (checking
its SHA-256 and version), which rebuilds the game at the new version from the
game files already extracted in the install folder: no ISO needed, about
10–20 minutes. Saves, achievements and `stuntman.toml` are kept. From 0.2.x
or earlier, download the new setup by hand once and install over the same
folder.

## Requirements (building by hand)

- Windows 10/11 x64
- Visual Studio 2022 or 2026 with *Desktop development with C++*, the
  Windows SDK, *C++ Clang Compiler for Windows* and *C++ CMake tools*
  (provides Clang, CMake and Ninja)
- Git, PowerShell
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
4. Run the game from the repository root (it finds `game\` there), or start
   `out\build\win-amd64-relwithdebinfo\StuntmanLauncher.exe`:
   ```powershell
   .\out\build\win-amd64-relwithdebinfo\stuntman.exe --log_file=stuntman.log
   ```

## PC menu

The game opens on a full-screen start screen in the style of the game's own
menus: **Play**, **Video**, **Controls**, **Game**, **Achievements** and
**Quit**. While playing, **F1** (or **Back + Start** on a controller) opens
the same menu over the game, which gets no input while it is open; the first
entry resumes. It works with keyboard, mouse and controller, and every change
is saved at once to `stuntman.toml` next to the game.

- **Video:** fullscreen or windowed, resolution including ultrawide (21:9,
  32:9) and 16:10, black bars or stretch on screens that aren't 16:9 (a true
  ultrawide view needs a game patch, not done yet), 30 or 60 FPS, frame pacing,
  supersampling, texture filtering, an FPS counter (also F2).
- **Controls:** keyboard on or off, two keys per action, and a controller
  layout that swaps buttons. The menus always use the normal layout, so a bad
  layout can't lock you out.
- **Game:** language (English, French, German, Spanish, Italian), start
  screen on or off, log file.
- **Achievements:** what you unlocked and when, with the game's icons.

Settings the game reads only at startup (resolution, supersampling, monitor,
and the language once the game is running) show a **Restart now** prompt.
The menu is in Italian when the game is, English otherwise.

Default keyboard controls:

| Action | Keys |
|---|---|
| Gas / brake, reverse (RT / LT) | W / S |
| Steer | A, D or Left, Right |
| Left stick up / down | Up / Down |
| A (confirm) / B (back) | Space or Enter / Backspace |
| X / Y / LB / RB | E / Q / Z / C |
| Camera (right stick) | I J K L |
| D-pad | 1 2 3 4 |
| Start (pause) / Back | Esc or P / Tab |

## Launcher

`StuntmanLauncher.exe` installs the game (see Installing) and, once it is
installed, manages your saves: career and replays at a glance, delete single
replays, back up and restore (zips in `Documents\stuntman\backups`), start
over from scratch or reset achievements. A backup is made before anything is
deleted. The Start menu has it as **Stuntman Ignition Launcher**, with
Uninstall. Its interface is in English or Italian, following Windows.

## Layout

| Path | Contents |
|---|---|
| `stuntman_manifest.toml` | Codegen manifest: function boundaries, switch tables, mid-asm hooks |
| `src/` | Host code: app setup, game-specific hooks, the PC menu (start screen and F1) |
| `assets/fonts/` | The PC menu's fonts, built into the exe |
| `launcher/` | The installer and save manager (C#, WPF) |
| `generated/` | Codegen output (not committed) |
| `patches/sdk/` | Our patches on top of ReXGlue v0.10.0 (function discovery for MSVC C++ code, FPSCR fix, input state filter) |
| `scripts/devenv.ps1` | Loads the Visual Studio toolchain into the shell |
| `scripts/build_release.ps1` | Builds the game and copies it to an install folder (used by the installer) |
| `scripts/package_release.ps1` | Makes the release asset, `StuntmanSetup-<version>.exe` |
| `docs/NOTES.md` | Findings about the game and the state of the port |
| `tools/` | Analysis helpers |

## License

The code in this repository is under the [MIT License](LICENSE), except the
patches in `patches/sdk/`, which modify the ReXGlue SDK and follow its BSD
3-Clause license, and the fonts in `assets/fonts/` (Oswald and Big Shoulders
Stencil), which are under the SIL Open Font License (license files next to
them).

Neither covers Stuntman: Ignition. Its executable and assets belong to their
respective owners and are never included here; you need your own copy.

## Credits

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), built on
  [Xenia](https://github.com/xenia-project/xenia) and
  [XenonRecomp](https://github.com/hedge-dev/XenonRecomp)

Stuntman: Ignition is a trademark of its respective owners. This project is
not affiliated with or endorsed by them, nor by Microsoft or Xbox.
