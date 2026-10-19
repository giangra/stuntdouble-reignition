"""Prints the PowerPC listing of the guest function containing an address.

The generated C++ keeps each original instruction as a `// ` comment. This
rebuilds their addresses (4 bytes each, resynced at every `loc_XXXXXXXX:`
label) and prints `address: instruction`, one per line.

Usage: python tools/ppc_listing.py <address> [--calls]
  --calls  only print branches to other functions (bl) and imports
"""

import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, "generated", "default")

FUNC_RE = re.compile(r"^(?:DEFINE_REX_FUNC|PPC_FUNC_IMPL\(__imp__)\(?(\w+)\)")
INIT_RE = re.compile(r"\{ 0x([0-9A-F]{8}), (\w+) \}")
LABEL_RE = re.compile(r"^loc_([0-9A-F]{8}):")
INSN_RE = re.compile(r"^\s*// (.+)$")
IMPORT_RE = re.compile(r"^\s*(__imp__\w+)\(ctx, base\);")


def load_function_starts():
    starts = {}
    with open(os.path.join(GEN, "stuntman_init.cpp"), encoding="utf-8") as f:
        for m in INIT_RE.finditer(f.read()):
            starts[m.group(2)] = int(m.group(1), 16)
    return starts


def find_function(addr, starts):
    # The function with the highest start <= addr.
    best = max((s, n) for n, s in starts.items() if s <= addr)
    return best[1], best[0]


def listing(name, start):
    for path in glob.glob(os.path.join(GEN, "stuntman_recomp.*.cpp")):
        with open(path, encoding="utf-8") as f:
            lines = f.readlines()
        for i, line in enumerate(lines):
            m = FUNC_RE.match(line)
            if m and m.group(1) == name:
                return list(walk(lines, i + 1, start))
    return []


def walk(lines, i, addr):
    for line in lines[i:]:
        if FUNC_RE.match(line):
            return
        m = LABEL_RE.match(line)
        if m:
            addr = int(m.group(1), 16)
            continue
        m = IMPORT_RE.match(line)
        if m:
            yield addr - 4, f"    -> {m.group(1)}"
            continue
        m = INSN_RE.match(line)
        if m:
            yield addr, m.group(1)
            addr += 4


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    addr = int(sys.argv[1], 16)
    calls_only = "--calls" in sys.argv
    starts = load_function_starts()
    name, start = find_function(addr, starts)
    print(f"{name} @ {start:08X}")
    for a, text in listing(name, start):
        if calls_only and not (text.startswith("bl ") or text.startswith("    ->")):
            continue
        mark = ">>" if a == addr else "  "
        print(f"{mark}{a:08X}: {text}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
