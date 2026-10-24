// stuntman - fixes for bugs in the game's own code
//
// Null vertex declaration in DrawVertices (sub_82300CE8). The 2D/UI renderer
// (0x8282xxxx-0x8283xxxx) caches the vertex declaration and vertex shader it
// last set, at 0x82C6D780 and 0x82C6D77C, and skips the D3D setters when they
// match. Full device state resets (sub_82838168, sub_8225FBD8) clear the
// device's declaration (device+12428) without touching that cache, so the next
// UI draw can reach DrawVertices with none and read through null at
// 0x82300F9C. Seen in the pause menu and when a scene restarts, more often at
// 60 FPS. The hook after the declaration is loaded (0x82300F28) skips that
// one draw (jumps to the epilogue) and empties the UI renderer's cache, so its
// next draw sets the declaration and shader again.

#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/system/kernel_state.h>

#include <cstdint>

namespace {

constexpr uint32_t kUiCachedDeclaration = 0x82C6D780;
constexpr uint32_t kUiCachedVertexShader = 0x82C6D77C;

}  // namespace

// r22 holds the device's current vertex declaration. True skips the draw.
bool StuntmanDrawWithoutDeclaration(PPCRegister& r22) {
  if (r22.u32 != 0) return false;
  if (auto* memory = rex::system::kernel_memory()) {
    *memory->TranslateVirtual<uint32_t*>(kUiCachedDeclaration) = 0;
    *memory->TranslateVirtual<uint32_t*>(kUiCachedVertexShader) = 0;
  }
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_WARN("DrawVertices without a vertex declaration: draw skipped, UI cache reset");
  }
  return true;
}
