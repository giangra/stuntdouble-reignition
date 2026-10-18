"""Finds code addresses that the game builds in registers with lis + addi/ori.

Walks the PowerPC comments in generated/default/*.cpp, tracks the value each
`lis` puts in a register, and reports every `addi`/`ori` (and bare `lis`)
whose result lands in the code section but is not a known function. Those are
callbacks and vtable entries the codegen can't see, which at runtime show up
as "Call to unresolved function at guest address".

Usage: python tools/find_code_constants.py [address ...]
  With addresses, only report how those are built.
"""

import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN = os.path.join(ROOT, "generated", "default")

FUNC_RE = re.compile(r"^DEFINE_REX_FUNC\((\w+)\)")
LIS_RE = re.compile(r"^\s*// lis (r\d+),(-?\d+)$")
ADD_RE = re.compile(r"^\s*// (addi|ori) (r\d+),(r\d+),(-?\d+)$")
INIT_RE = re.compile(r"\{ 0x([0-9A-F]{8}), (\w+) \}")


def main():
    wanted = {int(a, 16) for a in sys.argv[1:]}

    known = set()
    with open(os.path.join(GEN, "stuntman_init.cpp"), encoding="utf-8") as f:
        for m in INIT_RE.finditer(f.read()):
            known.add(int(m.group(1), 16))

    code_base, code_size = 0x82210000, 0
    with open(os.path.join(GEN, "stuntman_pch.h"), encoding="utf-8") as f:
        for line in f:
            m = re.search(r"REX_CODE_BASE\s+0x([0-9A-Fa-f]+)", line)
            if m:
                code_base = int(m.group(1), 16)
            m = re.search(r"REX_CODE_SIZE\s+0x([0-9A-Fa-f]+)", line)
            if m:
                code_size = int(m.group(1), 16)
    code_end = code_base + code_size

    hits = {}
    for path in sorted(glob.glob(os.path.join(GEN, "stuntman_recomp.*.cpp"))):
        func = None
        regs = {}
        with open(path, encoding="utf-8") as f:
            for line in f:
                m = FUNC_RE.match(line)
                if m:
                    func, regs = m.group(1), {}
                    continue
                m = LIS_RE.match(line)
                if m:
                    regs[m.group(1)] = (int(m.group(2)) << 16) & 0xFFFFFFFF
                    continue
                m = ADD_RE.match(line)
                if m and m.group(3) in regs:
                    op, dst, src, imm = m.groups()
                    imm = int(imm)
                    base = regs[src]
                    value = (base + imm) & 0xFFFFFFFF if op == "addi" else base | (imm & 0xFFFF)
                    if code_base <= value < code_end and value % 4 == 0:
                        hits.setdefault(value, set()).add(func)
                    if dst != src:
                        regs.pop(dst, None)

    for value in sorted(hits):
        if wanted and value not in wanted:
            continue
        if not wanted and value in known:
            continue
        users = sorted(hits[value])
        print(f"0x{value:08X} {'known' if value in known else 'NEW  '} "
              f"{len(users)} fn: {' '.join(users[:4])}")
    if wanted:
        for value in sorted(wanted - hits.keys()):
            print(f"0x{value:08X} not built by lis+addi/ori")


if __name__ == "__main__":
    main()
