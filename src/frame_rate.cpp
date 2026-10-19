// stuntman - frame rate
//
// The game asks Direct3D for a presentation interval of 2 (one frame every
// second vblank: 30 FPS). The statically linked D3D reads that render state
// (device+13572) when it queues a swap, in sub_8227F5B8 at 0x8227F66C, and
// maps it to a vblank count. The midasm hook below sits right after that
// load and turns 2 into 1 when target_fps is 60.

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc.h>

#include <chrono>
#include <cstdint>

REXCVAR_DEFINE_INT32(target_fps, 30, "Stuntman", "Frame rate cap: 30 (original) or 60")
    .range(30, 60)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(log_fps, false, "Stuntman", "Log the game's frame rate every 5 seconds");

namespace {

void CountFrame() {
  using Clock = std::chrono::steady_clock;
  static Clock::time_point window_start = Clock::now();
  static uint32_t frames = 0;
  ++frames;
  const auto now = Clock::now();
  const auto elapsed = std::chrono::duration<double>(now - window_start).count();
  if (elapsed >= 5.0) {
    REXLOG_INFO("Frame rate: {:.1f} FPS", frames / elapsed);
    frames = 0;
    window_start = now;
  }
}

}  // namespace

// r10 holds the D3D presentation interval about to be mapped to vblanks.
void StuntmanPresentInterval(PPCRegister& r10) {
  if (REXCVAR_GET(target_fps) >= 60 && r10.u32 == 2) r10.u64 = 1;
  if (REXCVAR_GET(log_fps)) CountFrame();
}
