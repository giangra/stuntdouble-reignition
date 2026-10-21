// stuntman - frame rate counter
//
// The game's frame rate (frame_rate.h) in the top right corner, in the
// stencil font of the game's HUD. Shown while show_fps is on (PC menu >
// Video, or F2). Lives for the whole session, so it also runs the app's
// per-frame checks (tick).

#pragma once

#include <rex/ui/imgui_dialog.h>

#include <functional>
#include <utility>

class FpsOverlay : public rex::ui::ImGuiDialog {
 public:
  FpsOverlay(rex::ui::ImGuiDrawer* drawer, std::function<void()> tick)
      : ImGuiDialog(drawer), tick_(std::move(tick)) {}

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  std::function<void()> tick_;
};
