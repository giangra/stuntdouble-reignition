"""Summarises frame times recorded by the benchmark harness (src/bench.h).

    python tools/frames_report.py bench/<run>/frames.txt [more.txt ...]
"""

import sys


def summary(path):
    ms = [float(l) for l in open(path) if l.strip()]
    if not ms:
        return f"{path}: no frames"
    total = sum(ms)
    srt = sorted(ms)
    p99 = srt[int(len(srt) * 0.99) - 1]
    worst1 = srt[int(len(srt) * 0.99):]
    low1 = 1000 / (sum(worst1) / len(worst1)) if worst1 else 0
    over = sum(1 for m in ms if m > 17.5) / len(ms)
    buckets = [(0, 15), (15, 17.5), (17.5, 25), (25, 34), (34, 1e9)]
    hist = "  ".join(f"{lo:g}-{hi if hi < 1e9 else '':}ms {sum(1 for m in ms if lo <= m < hi) / len(ms):5.1%}"
                     for lo, hi in buckets)
    return (f"{path}\n  {len(ms)} frames, avg {1000 * len(ms) / total:.1f} FPS, 1% low {low1:.1f} FPS, "
            f"p99 {p99:.1f} ms, frames over 17.5 ms {over:.1%}\n  {hist}")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        print(summary(p))
