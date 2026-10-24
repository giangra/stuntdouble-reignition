// stuntman - frame rate
//
// The game asks Direct3D for a presentation interval of 2 (one frame every
// second vblank: 30 FPS). The statically linked D3D reads that render state
// (device+13572) when it queues a swap, in sub_8227F5B8 at 0x8227F66C, and
// maps it to a vblank count. The midasm hook below sits right after that
// load and turns 2 into 1 when target_fps is 60.
//
// Pacing. On the console a swap waits for its vblank, so a frame that misses
// one by a millisecond is shown a whole refresh late (a 33 ms frame at 60,
// then a short one): that is what made 60 FPS stutter here, where the GPU
// emulation occasionally runs long. With frame_limiter (the default) the
// emulated vblank runs freely (the SDK's vsync cvar off) and the hook holds
// the game's render thread instead, so swaps come exactly one period apart
// and a long frame only delays the next by what it overran. The game's
// simulation is time-based, so its speed doesn't depend on either.

#include "frame_rate.h"

#include "bench.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#include <windows.h>

REXCVAR_DEFINE_INT32(target_fps, 30, "Stuntman", "Frame rate cap: 30 (original) or 60")
    .range(30, 60)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(log_fps, false, "Stuntman", "Log the game's frame rate every 5 seconds");

REXCVAR_DEFINE_BOOL(frame_limiter, true, "Stuntman",
                    "Pace frames with a precise PC timer at target_fps instead of the emulated "
                    "vblank (smoother)")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace {

using Clock = std::chrono::steady_clock;

std::atomic<float> g_fps{0.0f};

// Called once per guest swap: measures for the FPS counter (every 0.5 s) and
// the log (every 5 s).
void CountFrame() {
  bench::OnFrame();
  static Clock::time_point counter_start = Clock::now();
  static Clock::time_point log_start = counter_start;
  static uint32_t counter_frames = 0;
  static uint32_t log_frames = 0;
  ++counter_frames;
  ++log_frames;
  const auto now = Clock::now();
  const auto counter_elapsed = std::chrono::duration<double>(now - counter_start).count();
  if (counter_elapsed >= 0.5) {
    g_fps.store(float(counter_frames / counter_elapsed), std::memory_order_relaxed);
    counter_frames = 0;
    counter_start = now;
  }
  const auto log_elapsed = std::chrono::duration<double>(now - log_start).count();
  if (log_elapsed >= 5.0) {
    if (REXCVAR_GET(log_fps)) REXLOG_INFO("Frame rate: {:.1f} FPS", log_frames / log_elapsed);
    log_frames = 0;
    log_start = now;
  }
}

// Holds the guest's render thread so swaps come one period apart. A frame that
// ran long isn't followed by a wait for the next vblank (a 33 ms frame at 60):
// the schedule restarts from it.
void Pace() {
  static Clock::time_point next{};
  static HANDLE timer =
      CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  const auto period = std::chrono::duration_cast<Clock::duration>(
      std::chrono::duration<double>(1.0 / std::max(1, REXCVAR_GET(target_fps))));
  auto now = Clock::now();
  if (next == Clock::time_point{} || now > next + period) {
    next = now + period;
    return;
  }
  // Sleep to about 1 ms before, then spin: timer wakeups are a little late.
  const auto early = next - std::chrono::milliseconds(1);
  if (now < early && timer) {
    LARGE_INTEGER due;
    due.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(early - now).count() / 100;
    if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer, INFINITE);
  }
  while (Clock::now() < next) std::this_thread::yield();
  next += period;
}

}  // namespace

float GuestFps() { return g_fps.load(std::memory_order_relaxed); }

void SyncFramePacing() {
  // vsync lives in the GPU plugin: by name.
  const char* want = REXCVAR_GET(frame_limiter) ? "false" : "true";
  if (rex::cvar::GetFlagByName("vsync") != want) rex::cvar::SetFlagByName("vsync", want);
}

// r10 holds the D3D presentation interval about to be mapped to vblanks.
void StuntmanPresentInterval(PPCRegister& r10) {
  if (REXCVAR_GET(target_fps) >= 60 && r10.u32 == 2) r10.u64 = 1;
  SyncFramePacing();
  if (REXCVAR_GET(frame_limiter)) Pace();
  CountFrame();
}
