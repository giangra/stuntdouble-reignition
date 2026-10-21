// stuntman - look of the PC menu
//
// Drawing helpers that copy the feel of the game's own front end: black
// letterbox bars with orange sprocket marks, sepia film strips, white
// condensed capitals on dark slanted strips, and a ragged yellow banner with
// dark red text on the selected item. Everything is drawn with ImGui shapes;
// no game art is used. Sizes are in 1080p units, scaled by Theme::s.

#pragma once

#include <imgui.h>

#include <cstdint>
#include <string_view>

namespace theme {

// Colours (ImGui ABGR via IM_COL32).
constexpr ImU32 kYellow = IM_COL32(255, 184, 0, 255);
constexpr ImU32 kOrange = IM_COL32(232, 120, 20, 255);
constexpr ImU32 kRed = IM_COL32(150, 18, 14, 255);
constexpr ImU32 kWhite = IM_COL32(244, 240, 232, 255);
constexpr ImU32 kDim = IM_COL32(170, 160, 146, 255);
constexpr ImU32 kInk = IM_COL32(10, 9, 8, 255);
constexpr ImU32 kStrip = IM_COL32(16, 14, 12, 225);

ImU32 Alpha(ImU32 color, float alpha);

// Fonts built into the executable (resources.h). Load from
// ReXApp::OnConfigureFonts; both fall back to ImGui's default font.
void LoadFonts(ImFontAtlas* atlas);
ImFont* MenuFont();     // Oswald Bold: menu items and text
ImFont* StencilFont();  // Big Shoulders Stencil: numbers, like the game's HUD

// Pixels per 1080p unit for a display of this size.
float Scale(ImVec2 display);

// Text with an optional slant (k > 0 leans right, like the game's menus).
ImVec2 Measure(ImFont* font, float size, std::string_view text);
void Text(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color,
          std::string_view text, float slant = 0.0f);
// White text with a hard drop shadow.
void ShadowText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color,
                std::string_view text, float slant, float shadow);
// Text that wraps at width; returns its height.
float WrappedText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color,
                  std::string_view text, float width);

// Rotates everything drawn since vtx_start around center (radians).
void Rotate(ImDrawList* dl, int vtx_start, ImVec2 center, float angle);

// Shapes.
void RaggedBanner(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, uint32_t seed);
void Strip(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, float slant);
void FilmStrip(ImDrawList* dl, ImVec2 origin, float length, float height, float angle,
               float scroll, float alpha);
void Letterbox(ImDrawList* dl, ImVec2 display, float s, float alpha);
void Grime(ImDrawList* dl, ImVec2 min, ImVec2 max, float s, uint32_t seed, float alpha);
void Star(ImDrawList* dl, ImVec2 center, float radius, float angle, ImU32 fill, ImU32 outline,
          float thickness);
void Triangle(ImDrawList* dl, ImVec2 center, float size, bool right, ImU32 color);

// Button prompts: a controller button ("A", "B", "X", "Y", "LB", "START"...)
// or a keyboard key ("ENTER"). Return the width drawn.
float PadGlyph(ImDrawList* dl, ImVec2 pos, float height, std::string_view button, float alpha);
float KeyCap(ImDrawList* dl, ImVec2 pos, float height, std::string_view key, float alpha);

// The game's name, as drawn on the start screen. Returns its size.
ImVec2 Logo(ImDrawList* dl, ImVec2 pos, float s, float alpha);

}  // namespace theme
