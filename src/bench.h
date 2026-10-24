// stuntman - benchmark harness for development (environment variables only)
//
// Plays a scripted input sequence into the game, samples where every thread
// spends its time and records frame times, so performance can be measured on
// the same scene without touching the desktop's keyboard or mouse.
//
//   STUNTMAN_BENCH="8:a 12:a 14:down 15:a 30:+rt 90:quit"
//       Seconds after Play, then what to do: a button tapped (a, b, x, y, lb,
//       rb, lt, rt, back, start, up, down, left, right), "+btn" / "-btn" to
//       hold and release it, "shot" for a screenshot, "quit" to exit.
//   STUNTMAN_PROFILE="40,30"
//       Sample every thread from second 40 for 30 seconds; frame times are
//       recorded over the same window.
//   STUNTMAN_BENCH_OUT=<folder>   where results go (default: bench\)
//
// Results: profile.txt (raw samples by thread, module+offset, for
// tools/profile_report.py), frames.txt (one frame time per line, ms),
// shotN.bmp.

#pragma once

#include <cstdint>

namespace rex::input {
struct X_INPUT_STATE;
}

namespace bench {

// Starts the script and the profiler, if their variables are set. Call when
// the game is launched.
void Start();

// Adds the scripted buttons to a device's state.
void Inject(rex::input::X_INPUT_STATE& state);

// Called on every guest swap.
void OnFrame();

}  // namespace bench
