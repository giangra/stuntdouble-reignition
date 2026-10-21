# Port notes

Running log of findings about Stuntman: Ignition's executable and the state
of the port. Newest first within each section.

## Game facts

- `default.xex`: Title ID `545107F4`, Media ID `64239B36`, version 0.0.0.1,
  built 2007-07-18. 12 MB.
- Disc: "Stuntman - Ignition (USA, Europe) (En,Fr,De,Es,It)". 627 files,
  5.9 GB. Assets are `.bnk` / `.mbk` banks, `.rep` replays, `.bin` text.
- Movies are Bink (`.bik`, `BIKi`); the decoder is linked into the XEX, so it
  is recompiled with the game.

## Codegen

- The game is MSVC C++ with exceptions. SDK changes in
  `patches/sdk/0001-codegen-eh-aware-function-discovery.patch` took the unresolved-branch warnings from ~1800 to 0:
  - `noreturn = true` on a `[functions]` entry ends the caller's block at a
    `bl` to it. Set on `_CxxThrowException` (`0x8255B2E8`, 801 call sites);
    without it discovery ran past each throw into the EH data and the next
    funclets, merging them.
  - `dataPointerScan` registers code addresses stored in data sections
    (vtables RTTI misses, callback tables): ~2900 functions. It skips the ones
    inside a known function (EH IP-to-state maps, catch continuations): 1930
    bogus entries. Cvar
    `data_pointer_skip_interior` (default on). It uses the PDATA extent, so
    a real function inside one is hidden: declare it in the manifest
    (`0x82234CA0` was such a case, found at runtime as "Call to unresolved
    function at guest address").
  - `functionPointerScan` (lis + addi/ori code constants) existed in the SDK
    but was never called. Now on (cvar `code_pointer_scan`): 1141 callbacks
    the game registers at runtime. Before, gameplay froze on a static frame
    (HUD drawn, nothing moving) with ~26k "Call to unresolved function"
    errors over 22 targets, all built this way (`sub_82974520` registers 8).
    Fixed in the scan: lis tracking is reset at terminators and calls (stale
    lis + unrelated addi made bogus entries), `ori` source/target fields were
    swapped, and a target inside a known function or in the same gap as the
    building instruction is a label, not an entry.
  - `tools/find_code_constants.py [addr ...]` reports how an address is built
    with lis + addi/ori in the generated code.
- First codegen (stock ReXGlue v0.10.0): one validation
  error, a `b 0x825080B8` from `0x8250819C` into no function. Declared
  `0x825080B8` as a function in the manifest.
- After that codegen succeeds in ~45 s: 315 files, 163 MB of C++.
- ~1800 "Unresolved" warnings over 542 distinct targets, emitted as
  `REX_FATAL` in the generated code (they only fire if reached). 90% are in
  `0x824F0000`-`0x8257FFFF`, likely one library with odd function layout.
  Fix them case by case when a run hits one: declare the target as a
  function (`[entrypoint.functions.0x...]`) or fix the enclosing function's
  boundaries.

## Runtime fixes

- Crash on starting a career: `0xC000008F` (`STATUS_FLOAT_INEXACT_RESULT`)
  in a `vmaddfp` in `sub_822FB898`. `mtfsf` (FPSCR write) and the flush-mode
  helpers wrote the cached MXCSR copy back whole; a context whose copy was
  never loaded holds 0, which unmasks every host FP exception. Fixed in
  `patches/sdk/0002-fpscr-keep-host-exception-masks.patch` by starting from the live MXCSR.

## Debugging a crash

A hard crash leaves nothing in the game log. Windows logs it instead:

```powershell
Get-WinEvent -FilterHashtable @{LogName='Application'; StartTime=(Get-Date).AddHours(-1)} |
  ? { $_.Message -match 'stuntman' } | select -First 2 | % Message
```

Take the exception code and "Offset errore" / "Fault offset", then:

```powershell
& "$vs\VC\Tools\Llvm\x64\bin\llvm-symbolizer.exe" --obj=out\build\win-amd64-relwithdebinfo\stuntman.exe --relative-address 0x<offset>
```

It names the `sub_XXXXXXXX` and the line in `generated/default/*.cpp`; the
`// ` comments there are the original PowerPC instructions.

## Environment gotchas (Windows)

- Git without symlink support checks out libmspack's `cabextract/` symlinks
  as text files; `setup.ps1` replaces them with copies.
- `rexglue init` resolves a `game` junction to its target path; the manifest
  was edited back to `game/`.
- The CLI takes dashed flags: `rexglue init --project-name ... --xex-path ...`.

## Status

- [x] SDK cloned, patched, `rexglue` CLI built
- [x] Project initialised, codegen passes
- [x] Full build (RelWithDebInfo, Clang 22 from VS 2026)
- [x] First run: boots and plays the attract-mode Bink movie with the Xenos
      GPU plugin (D3D12). Needs `config.gpu_plugin = "xenos"` and
      `GPU_PLUGINS xenos`, otherwise the screen stays black.
- [x] Main menu, career and quick play reached with a controller
- [x] Races load and play; saving works
- [x] Runs on stock ReXGlue v0.10.0 plus our two patches
- [x] PC menu (`src/pc_menu.cpp`, look in `src/ui_theme.cpp`), drawn with
      ImGui shapes to look like the game's front end (no game art; fonts
      Oswald and Big Shoulders Stencil, OFL, as RCDATA in `src/stuntman.rc`).
      Start screen: `LaunchModule` is overridden and only runs when Play is
      chosen, so nothing plays underneath and `user_language` (read by
      `XGetLanguage` at boot) applies at once. In game: F1 or Back + Start.
      Pages: video, controls (SDK keyboard binds `keybind_*` with our
      defaults in `src/pad_input.cpp`, controller layout `pad_*`), game,
      achievements (from `KernelState`, icons through the SDK's
      `AchievementIconCache`). Saves `stuntman.toml` on every change; the
      restart-only cvars are compared with their startup values for the
      "restart now" prompt, which relaunches the exe with the same command
      line. First start without a config: desktop resolution, Windows
      language (`PcMenu::ApplyFirstRunDefaults`). The SDK's raw cvar editor
      stays on F4.
      Input goes through `patches/sdk/0003-input-state-filter.patch`: a
      per-device callback in `InputSystem::GetState`. It remaps the pad,
      blanks every device while the menu is open and afterwards until the
      buttons are released, and records the raw buttons for the menu. Before
      Play the guest doesn't poll, so the start screen calls `GetState`
      itself (never while the guest runs: `RefreshDevices` isn't locked).
      Screenshots of the pages without touching the desktop's input:
      `STUNTMAN_MENU_PAGE=video|controls|keyboard|controller|game|achievements`
      opens the start screen on that page, and `PrintWindow` with
      `PW_RENDERFULLCONTENT` captures the window even in the background.
      SDL ignores key messages posted to an unfocused window.
- [x] Launcher (`launcher/`, WPF): now installer, Play and saves only; the
      settings and achievements tabs moved to the PC menu. Shortcuts: the
      game on the desktop and in the Start menu, the launcher in the Start
      menu. Ultrawide: the game sizes its back buffer from the guest video
      mode (`video_mode_width/height`) and renders natively at it, but keeps
      a 16:9 projection and UI, so 3440x1440 comes out stretched. "Black
      bars" mode gives it the largest 16:9 mode that fits (2560x1440) and the
      presenter letterboxes it.
- [x] Achievements. The runtime saves unlocks in
      `Documents/stuntman/achievements/545107F4.toml` (id -> FILETIME).
      Text, gamerscore and icons are in the XEX's XDBF, loaded with the
      module before the start screen shows. 49 achievements, 970 G. (The
      launcher used to read a catalog the game exported at each start;
      removed with its achievements tab, see git history.)
- [x] Saves tab in the launcher. Layout: `Documents/stuntman/<XUID>/545107F4/
      00000001/Save Data/{CareerData,replays,ghosts,Arenas,...}` plus
      `Headers/00000001/Save Data.header`. Deleting the whole `545107F4`
      folder gives a clean start: the game recreates it on boot.
- [x] Installer: the launcher exe in install mode (no stuntman.exe next to
      it). Checks the ISO (XDVDFS reader in launcher/XisoReader.cs, title
      ID from the XEX2 execution info header), installs VS 2026 Build Tools
      if no VS has VC tools + Clang + CMake, gets MinGit if Git is missing,
      clones this repo at v<version>, extracts the ISO, runs
      scripts/build_release.ps1. Fresh checkouts need codegen, then a second
      configure: sources.cmake only exists after codegen. Full install from
      scratch on the dev PC (VS and Git present): about 9 minutes.
      Test with STUNTMAN_SOURCE=<local repo> STUNTMAN_SOURCE_REF=<branch>
      and `--iso <file> --dir <folder> --auto`.
- [ ] True ultrawide: patch the projection aspect and the 2D UI layout
- [x] 60 FPS (cvar `target_fps`, PC menu > Video). The 30 FPS cap is
      D3D's presentation interval: render state at device+13572 (setters
      `sub_822801F0`, `sub_822625C0`), read when a swap is queued in
      `sub_8227F5B8` (0x8227F66C) and mapped to a vblank count (0/1->1, 2->2,
      4->3, 0x80000000->0). The swap queue is `sub_8227F420`: target vblank =
      last target + interval, shown by the vblank handler `sub_8227F320`
      (device+16524 counts vblanks). A midasm hook at 0x8227F670 turns 2
      into 1: measured 60 FPS (`log_fps = true` logs it every 5 s).
      Gameplay runs at normal speed at 60 (tested in races, 2026-10-19), so
      the simulation is time-based. Code notes (`sub_822F82F0` = seconds between two counter values,
      `sub_8234A140` = QueryPerformanceFrequency; `sub_8233AE68` uses them for
      network timeouts, not the frame step). `tools/ppc_listing.py <addr>`
      prints a function's PowerPC listing with addresses.

## Open issues

- Crash once after ~30 s in the pause menu (2026-10-18), not reproduced in a
  later session. Null read in `sub_82300CE8` (D3D `DrawVertices`): device
  field `+12428` (vertex declaration, set by `sub_822633B8`) was NULL. The
  2D/UI renderer (`0x8282xxxx`-`0x8283xxxx`) caches the current declaration in
  `0x82C6D780` and the vertex shader in `0x82C6D77C`, and skips the D3D setter
  when the cache matches; full state resets (`sub_82838168`, `sub_8225FBD8`)
  null the device without touching the cache. Crashed on a guest thread that
  logged nothing before.

- `d:\keyboard.ini` is opened and missing (dev leftover in the game).
- Xenia lists `XInputdFF*` (force feedback wheel) imports as unimplemented.
