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
- [ ] Main menu reached with a controller
- [ ] In-game: first stunt loads

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
