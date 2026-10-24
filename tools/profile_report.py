"""Summarises a profile written by the benchmark harness (src/bench.h).

    python tools/profile_report.py bench/<run>/profile.txt [--build DIR] [--top N]

For each thread that was busy (not waiting in the kernel) it prints how much
of the time it ran and its hottest functions. --callers NAME instead prints
the call stacks through functions whose name contains NAME. Code in stuntman.exe is named
through llvm-symbolizer and stuntman.pdb (generated functions show up as
sub_XXXXXXXX), system DLLs through their export tables.
"""

import argparse
import collections
import glob
import os
import subprocess
import sys

# Instruction pointers in these mean the thread is blocked, not running.
WAIT_PREFIXES = ("NtWait", "NtDelayExecution", "NtRemoveIoCompletion", "NtUserMsgWaitForMultipleObjects",
                 "ZwWait", "NtSignalAndWaitForSingleObject", "NtAlertThreadByThreadId",
                 "NtWaitForAlertByThreadId", "NtUserGetMessage", "NtUserWaitMessage", "NtYieldExecution",
                 "NtReadFile", "NtDeviceIoControlFile", "NtFsControlFile")


def find_llvm_symbolizer():
    for pattern in (r"C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\Llvm\x64\bin\llvm-symbolizer.exe",
                    r"C:\Program Files (x86)\Microsoft Visual Studio\*\*\VC\Tools\Llvm\x64\bin\llvm-symbolizer.exe"):
        hits = sorted(glob.glob(pattern))
        if hits:
            return hits[-1]
    return "llvm-symbolizer"


def parse(path):
    """Threads with their samples: (count, [(module, offset), ...] innermost first)."""
    threads = []
    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith("thread "):
            _, tid, total, name = line.rstrip("\n").split(" ", 3)
            threads.append({"tid": tid, "total": int(total), "name": name, "samples": []})
        elif line.startswith("  ") and threads:
            count, rest = line.strip().split(" ", 1)
            frames = []
            for frame in rest.split(" <- "):
                mod, off = frame.split()
                frames.append((mod, int(off, 16)))
            threads[-1]["samples"].append((int(count), frames))
    return threads


class ExportSymbols:
    def __init__(self):
        self.cache = {}

    def lookup(self, module, offset):
        if module not in self.cache:
            self.cache[module] = self._load(module)
        table = self.cache[module]
        best = None
        for rva, name in table:
            if rva > offset:
                break
            best = name
        return best

    @staticmethod
    def _load(module):
        try:
            import pefile
        except ImportError:
            return []
        for folder in (r"C:\Windows\System32", r"C:\Windows\System32\DriverStore"):
            path = os.path.join(folder, module)
            if os.path.exists(path):
                break
        else:
            return []
        try:
            pe = pefile.PE(path, fast_load=True)
            pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
            exports = getattr(pe, "DIRECTORY_ENTRY_EXPORT", None)
            if not exports:
                return []
            return sorted((s.address, s.name.decode()) for s in exports.symbols if s.name)
        except Exception:
            return []


def symbolize(exe, offsets, symbolizer):
    if not offsets:
        return {}
    text = "\n".join(hex(o) for o in offsets) + "\n"
    out = subprocess.run([symbolizer, f"--obj={exe}", "--relative-address", "--functions=linkage",
                          "--no-inlines"], input=text, capture_output=True, text=True).stdout
    blocks = [b for b in out.split("\n\n") if b.strip()]
    names = {}
    for off, block in zip(offsets, blocks):
        lines = block.strip().splitlines()
        names[off] = lines[0] if lines else "?"
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("profile")
    ap.add_argument("--build", default="out/build/win-amd64-relwithdebinfo")
    ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--min-busy", type=float, default=0.05, help="skip threads busy less than this")
    ap.add_argument("--callers", help="show the call stacks through functions whose name contains this")
    ap.add_argument("--thread", help="with --callers: only threads whose name contains this")
    args = ap.parse_args()

    threads = parse(args.profile)
    all_frames = {f for t in threads for _, frames in t["samples"] for f in frames}
    # Our modules have PDBs: the exe in the build folder, the SDK's DLLs where
    # the SDK builds them.
    sdk_out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "thirdparty", "rexglue-sdk", "out",
                           "win-amd64")
    symbolizer = find_llvm_symbolizer()
    names = {}
    for mod in {m for m, _ in all_frames}:
        for folder in (args.build, sdk_out):
            binary = os.path.join(folder, mod)
            if os.path.exists(binary) and os.path.exists(os.path.splitext(binary)[0] + ".pdb"):
                offs = sorted({off for m, off in all_frames if m == mod})
                names[mod] = symbolize(binary, offs, symbolizer)
                break
    exports = ExportSymbols()

    def name_of(frame):
        mod, off = frame
        if mod in names:
            name = names[mod].get(off, "?")
            return name if mod == "stuntman.exe" else f"{mod}!{name}"
        sym = exports.lookup(mod, off)
        return f"{mod}!{sym}" if sym else f"{mod}+{off:#x}"

    def is_wait(frame):
        mod = frame[0]
        if mod not in ("ntdll.dll", "win32u.dll"):
            return False
        short = name_of(frame).split("!", 1)[-1]
        if short.startswith("Zw"):
            short = "Nt" + short[2:]
        return short.startswith(WAIT_PREFIXES)

    if args.callers:
        stacks = collections.Counter()
        total = 0
        for t in threads:
            if args.thread and args.thread not in t["name"]:
                continue
            for n, frames in t["samples"]:
                for i, f in enumerate(frames):
                    if args.callers in name_of(f):
                        stacks[tuple(short_name(name_of(g)) for g in frames[i:])] += n
                        total += n
                        break
        print(f"{total} samples with {args.callers!r} on the stack")
        for stack, n in stacks.most_common(args.top):
            print(f"\n{n:6d}  " + "\n        <- ".join(stack))
        return

    report = []
    for t in threads:
        funcs = collections.Counter()
        busy = 0
        for n, frames in t["samples"]:
            if is_wait(frames[0]):
                continue
            funcs[name_of(frames[0])] += n
            busy += n
        report.append((busy / max(1, t["total"]), t, funcs, busy))

    report.sort(key=lambda r: -r[0])
    for frac, t, funcs, busy in report:
        if frac < args.min_busy:
            continue
        print(f"\n== {t['name']} (tid {t['tid']}): busy {frac:.0%} of {t['total']} samples")
        for name, n in funcs.most_common(args.top):
            print(f"   {n / t['total']:6.1%}  {short_name(name)}")


def short_name(name):
    """Drops argument lists from C++ names."""
    depth = 0
    out = []
    for ch in name:
        if ch == "(":
            depth += 1
            if depth == 1:
                out.append("()")
        elif ch == ")":
            depth -= 1
        elif depth == 0:
            out.append(ch)
    return "".join(out)


if __name__ == "__main__":
    sys.exit(main())
