// stuntman - frame rate
//
// The game asks Direct3D for a presentation interval of 2 (one frame every
// second vblank: 30 FPS). The statically linked D3D reads that render state
// (device+13572) when it queues a swap, in sub_8227F5B8 at 0x8227F66C, and
// maps it to a vblank count. The midasm hook below sits right after that
// load and turns 2 into 1 when target_fps is 60.

#include "frame_rate.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#include <atomic>
#include <chrono>
#include <cstdint>

REXCVAR_DEFINE_INT32(target_fps, 30, "Stuntman", "Frame rate cap: 30 (original) or 60")
    .range(30, 60)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(log_fps, false, "Stuntman", "Log the game's frame rate every 5 seconds");

namespace {

using Clock = std::chrono::steady_clock;

std::atomic<float> g_fps{0.0f};

// Called once per guest swap: measures for the FPS counter (every 0.5 s) and
// the log (every 5 s).
void CountFrame() {
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

}  // namespace

float GuestFps() { return g_fps.load(std::memory_order_relaxed); }

// r10 holds the D3D presentation interval about to be mapped to vblanks.
void StuntmanPresentInterval(PPCRegister& r10) {
  if (REXCVAR_GET(target_fps) >= 60 && r10.u32 == 2) r10.u64 = 1;
  CountFrame();
}
