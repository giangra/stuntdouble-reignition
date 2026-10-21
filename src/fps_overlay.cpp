// stuntman - frame rate counter. See fps_overlay.h.

#include "fps_overlay.h"

#include "frame_rate.h"
#include "ui_theme.h"

#include <rex/cvar.h>

#include <imgui.h>

#include <cmath>
#include <cstdlib>
#include <string>

void FpsOverlay::OnDraw(ImGuiIO& io) {
  if (tick_) tick_();
  if (rex::cvar::GetFlagByName("show_fps") != "true") return;
  const float fps = GuestFps();
  const float s = theme::Scale(io.DisplaySize);
  ImDrawList* dl = ImGui::GetForegroundDrawList();
  const std::string number = fps > 0.0f ? std::to_string(int(std::lround(fps))) : "--";
  const float size = 54.0f * s;
  const float label_size = 22.0f * s;
  const ImVec2 n = theme::Measure(theme::StencilFont(), size, number);
  const ImVec2 l = theme::Measure(theme::MenuFont(), label_size, "FPS");
  const float right = io.DisplaySize.x - 24.0f * s;
  const ImVec2 pos(right - n.x - l.x - 8.0f * s, 18.0f * s);
  dl->AddRectFilled(ImVec2(pos.x - 14.0f * s, pos.y - 4.0f * s),
                    ImVec2(right + 10.0f * s, pos.y + size + 2.0f * s),
                    theme::Alpha(theme::kInk, 0.55f), 6.0f * s);
  // Green at the target rate, yellow below it.
  const int target = std::atoi(rex::cvar::GetFlagByName("target_fps").c_str());
  const ImU32 color = fps >= target - 2 ? IM_COL32(150, 230, 90, 255) : theme::kYellow;
  theme::ShadowText(dl, theme::StencilFont(), size, pos, color, number, 0.0f, 3.0f * s);
  theme::Text(dl, theme::MenuFont(), label_size, ImVec2(right - l.x, pos.y + size - label_size * 1.4f),
              theme::kWhite, "FPS");
}
