# Stunt Double: Reignition

A PC port of **Stuntman: Ignition** (Xbox 360, 2007) built by static
recompilation with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).
The game's PowerPC executable is translated ahead of time into C++ and
compiled natively: no emulator, no JIT.

> **Status: early development.** Nothing is playable yet.

This repository contains **no game files** and no recompiled game code. You
need your own legally dumped copy of the Xbox 360 disc.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 or 2026 with *Desktop development with C++*, the
  Windows SDK, *C++ Clang Compiler for Windows* and *C++ CMake tools*
  (provides Clang, CMake and Ninja)
- Git, Python 3, PowerShell 7
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
4. Run from the repository root:
   ```powershell
   .\out\build\win-amd64-relwithdebinfo\stuntman.exe --log_file=stuntman.log
   ```

## Layout

| Path | Contents |
|---|---|
| `stuntman_manifest.toml` | Codegen manifest: function boundaries, switch tables, mid-asm hooks |
| `src/` | Host code: app setup and game-specific hooks |
| `generated/` | Codegen output (not committed) |
| `patches/sdk/` | Our patches on top of ReXGlue v0.10.0 (function discovery for MSVC C++ code, FPSCR fix) |
| `scripts/devenv.ps1` | Loads the Visual Studio toolchain into the shell |
| `docs/NOTES.md` | Findings about the game and the state of the port |
| `tools/` | Analysis helpers |

## License

MIT for the code in this repository; the patches in `patches/sdk/` follow
ReXGlue's BSD 3-Clause license. See [LICENSE](LICENSE). The game itself is not
covered and is never included.

## Credits

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), built on
  [Xenia](https://github.com/xenia-project/xenia) and
  [XenonRecomp](https://github.com/hedge-dev/XenonRecomp)

Stuntman: Ignition is a trademark of its respective owners. This project is
not affiliated with or endorsed by them, nor by Microsoft or Xbox.
