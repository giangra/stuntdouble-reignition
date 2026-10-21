// stuntman - look of the PC menu. See ui_theme.h.

#include "ui_theme.h"

#include "resources.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace theme {

namespace {

ImFont* g_menu_font = nullptr;
ImFont* g_stencil_font = nullptr;

uint32_t Hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}

// Deterministic noise in [0, 1).
float Noise(uint32_t seed, uint32_t i) {
  return float(Hash(seed * 0x9E3779B9u + i) & 0xFFFFFF) / float(0x1000000);
}

ImFont* LoadResourceFont(ImFontAtlas* atlas, int id, float bake_size) {
  HMODULE module = GetModuleHandleW(nullptr);
  HRSRC res = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));  // RT_RCDATA
  if (!res) return nullptr;
  HGLOBAL handle = LoadResource(module, res);
  void* data = handle ? LockResource(handle) : nullptr;
  const DWORD size = SizeofResource(module, res);
  if (!data || !size) return nullptr;
  ImFontConfig config;
  config.FontDataOwnedByAtlas = false;  // stays in the executable image
  config.OversampleH = 1;
  config.OversampleV = 1;
  return atlas->AddFontFromMemoryTTF(data, int(size), bake_size, &config,
                                     atlas->GetGlyphRangesDefault());
}

}  // namespace

ImU32 Alpha(ImU32 color, float alpha) {
  const uint32_t a = (color >> IM_COL32_A_SHIFT) & 0xFF;
  const uint32_t scaled = uint32_t(std::clamp(alpha, 0.0f, 1.0f) * float(a) + 0.5f);
  return (color & ~IM_COL32_A_MASK) | (scaled << IM_COL32_A_SHIFT);
}

void LoadFonts(ImFontAtlas* atlas) {
  // Baked large and drawn smaller, so the text stays sharp up to 4K.
  g_menu_font = LoadResourceFont(atlas, IDR_FONT_MENU, 72.0f);
  g_stencil_font = LoadResourceFont(atlas, IDR_FONT_STENCIL, 72.0f);
}

ImFont* MenuFont() { return g_menu_font ? g_menu_font : ImGui::GetFont(); }
ImFont* StencilFont() { return g_stencil_font ? g_stencil_font : MenuFont(); }

float Scale(ImVec2 display) {
  // Fit 1920x1080 inside the display, so 21:9 and 4:3 both keep the layout.
  return std::max(0.25f, std::min(display.x / 1920.0f, display.y / 1080.0f));
}

ImVec2 Measure(ImFont* font, float size, std::string_view text) {
  return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.data(), text.data() + text.size());
}

void Text(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color,
          std::string_view text, float slant) {
  const int start = dl->VtxBuffer.Size;
  dl->AddText(font, size, pos, color, text.data(), text.data() + text.size());
  if (slant != 0.0f) {
    const float base = pos.y + size * 0.8f;
    for (int i = start; i < dl->VtxBuffer.Size; ++i) {
      auto& v = dl->VtxBuffer[i];
      v.pos.x += (base - v.pos.y) * slant;
    }
  }
}

void ShadowText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color,
                std::string_view text, float slant, float shadow) {
  const float a = float((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
  Text(dl, font, size, ImVec2(pos.x + shadow, pos.y + shadow), Alpha(kInk, 0.85f * a), text,
       slant);
  Text(dl, font, size, pos, color, text, slant);
}

float WrappedText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color,
                  std::string_view text, float width) {
  dl->AddText(font, size, pos, color, text.data(), text.data() + text.size(), width);
  return font->CalcTextSizeA(size, FLT_MAX, width, text.data(), text.data() + text.size()).y;
}

void Rotate(ImDrawList* dl, int vtx_start, ImVec2 center, float angle) {
  const float c = std::cos(angle), s = std::sin(angle);
  for (int i = vtx_start; i < dl->VtxBuffer.Size; ++i) {
    auto& v = dl->VtxBuffer[i];
    const float x = v.pos.x - center.x, y = v.pos.y - center.y;
    v.pos = ImVec2(center.x + x * c - y * s, center.y + x * s + y * c);
  }
}

void RaggedBanner(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, uint32_t seed) {
  const float h = max.y - min.y;
  const float amp = h * 0.10f;
  const float slant = h * 0.35f;
  std::vector<ImVec2> pts;
  // Top edge, left to right: slightly torn.
  const int top_steps = 18;
  for (int i = 0; i <= top_steps; ++i) {
    const float t = float(i) / top_steps;
    pts.emplace_back(min.x + slant + t * (max.x - min.x - slant),
                     min.y + Noise(seed, i) * amp * 0.6f);
  }
  // Right edge, down: ragged.
  const int side_steps = 9;
  for (int i = 1; i < side_steps; ++i) {
    const float t = float(i) / side_steps;
    pts.emplace_back(max.x - Noise(seed + 1, i) * amp * 2.5f, min.y + t * h);
  }
  // Bottom edge, right to left: more torn, like burnt paper.
  for (int i = 0; i <= top_steps; ++i) {
    const float t = float(i) / top_steps;
    pts.emplace_back(max.x - t * (max.x - min.x), max.y - Noise(seed + 2, i) * amp * 1.4f);
  }
  // Left edge, up: a hard diagonal cut with a few chips.
  for (int i = 1; i < side_steps; ++i) {
    const float t = float(i) / side_steps;
    pts.emplace_back(min.x + t * slant + Noise(seed + 3, i) * amp, max.y - t * h);
  }
  dl->AddConcavePolyFilled(pts.data(), int(pts.size()), color);
  // Specks of the dark background showing through the paint.
  for (uint32_t i = 0; i < 14; ++i) {
    const ImVec2 p(min.x + slant + Noise(seed + 4, i) * (max.x - min.x - slant * 2.0f),
                   min.y + Noise(seed + 5, i) * h);
    dl->AddCircleFilled(p, 0.6f + Noise(seed + 6, i) * h * 0.03f, Alpha(kInk, 0.35f), 6);
  }
}

void Strip(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, float slant) {
  const ImVec2 pts[4] = {
      ImVec2(min.x + slant, min.y), ImVec2(max.x, min.y), ImVec2(max.x, max.y),
      ImVec2(min.x, max.y)};
  dl->AddConvexPolyFilled(pts, 4, color);
}

void FilmStrip(ImDrawList* dl, ImVec2 origin, float length, float height, float angle,
               float scroll, float alpha) {
  const int start = dl->VtxBuffer.Size;
  const ImVec2 a = origin, b(origin.x + length, origin.y + height);
  dl->AddRectFilled(a, b, Alpha(IM_COL32(8, 7, 6, 255), alpha));

  // Sprocket holes along both edges.
  const float hole_w = height * 0.055f, hole_h = height * 0.07f, pitch = height * 0.13f;
  const float margin = height * 0.045f;
  const float offset = std::fmod(scroll, pitch);
  for (float x = a.x - pitch + offset; x < b.x; x += pitch) {
    if (x < a.x) continue;
    const ImU32 hole = Alpha(IM_COL32(58, 50, 40, 255), alpha);
    dl->AddRectFilled(ImVec2(x, a.y + margin), ImVec2(x + hole_w, a.y + margin + hole_h), hole,
                      hole_w * 0.25f);
    dl->AddRectFilled(ImVec2(x, b.y - margin - hole_h), ImVec2(x + hole_w, b.y - margin), hole,
                      hole_w * 0.25f);
  }

  // Frames: sepia stills with a bright blast here and there.
  const float frame_top = a.y + margin * 2.0f + hole_h;
  const float frame_bottom = b.y - margin * 2.0f - hole_h;
  const float frame_h = frame_bottom - frame_top;
  const float frame_w = frame_h * 1.45f, gap = height * 0.06f;
  const float frame_pitch = frame_w + gap;
  const float frame_offset = std::fmod(scroll, frame_pitch);
  const int first = int(std::floor(scroll / frame_pitch));
  int n = 0;
  for (float x = a.x - frame_pitch + frame_offset; x < b.x; x += frame_pitch, ++n) {
    const float x0 = std::max(x, a.x), x1 = std::min(x + frame_w, b.x);
    if (x1 <= x0) continue;
    const uint32_t id = uint32_t(n - first + 1000);
    const float tone = 0.55f + Noise(77, id) * 0.45f;
    const ImU32 top = Alpha(IM_COL32(int(110 * tone), int(92 * tone), int(68 * tone), 255), alpha);
    const ImU32 bottom = Alpha(IM_COL32(int(40 * tone), int(32 * tone), int(24 * tone), 255), alpha);
    dl->AddRectFilledMultiColor(ImVec2(x0, frame_top), ImVec2(x1, frame_bottom), top, top, bottom,
                                bottom);
    // A rough horizon and a blast.
    const float horizon = frame_top + frame_h * (0.45f + Noise(78, id) * 0.2f);
    dl->AddRectFilled(ImVec2(x0, horizon), ImVec2(x1, frame_bottom),
                      Alpha(IM_COL32(20, 16, 12, 255), alpha * 0.55f));
    // A blast: a ragged cloud of light over the horizon.
    if (Noise(79, id) > 0.4f) {
      const ImVec2 c(x + frame_w * (0.2f + Noise(80, id) * 0.6f), horizon - frame_h * 0.05f);
      const float r = frame_h * (0.10f + Noise(81, id) * 0.10f);
      for (uint32_t k = 0; k < 9; ++k) {
        const ImVec2 p(c.x + (Noise(82, id * 16 + k) - 0.5f) * r * 3.2f,
                       c.y - Noise(83, id * 16 + k) * r * 1.8f);
        const float pr = r * (0.35f + Noise(84, id * 16 + k) * 0.6f);
        if (p.x - pr < x0 || p.x + pr > x1 || p.y - pr < frame_top) continue;
        const ImU32 glow = k < 3 ? IM_COL32(236, 214, 170, 255) : IM_COL32(150, 128, 96, 255);
        dl->AddCircleFilled(p, pr, Alpha(glow, alpha * (k < 3 ? 0.30f : 0.22f)), 12);
      }
    }
  }
  Rotate(dl, start, origin, angle);
}

void Letterbox(ImDrawList* dl, ImVec2 display, float s, float alpha) {
  const float bar = 70.0f * s;
  const ImU32 black = Alpha(IM_COL32(0, 0, 0, 255), alpha);
  dl->AddRectFilled(ImVec2(0, 0), ImVec2(display.x, bar), black);
  dl->AddRectFilled(ImVec2(0, display.y - bar), display, black);
  // Orange sprocket marks near the inner edges, as on the game's menus.
  const ImU32 orange = Alpha(kOrange, alpha * 0.9f);
  for (float fx : {0.18f, 0.80f}) {
    for (int row = 0; row < 2; ++row) {
      const float y = row == 0 ? bar - 12.0f * s : display.y - bar + 7.0f * s;
      for (int i = 0; i < 8; ++i) {
        const float x = display.x * fx + i * 6.0f * s;
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 3.5f * s, y + 5.0f * s), orange);
      }
    }
  }
}

void Grime(ImDrawList* dl, ImVec2 min, ImVec2 max, float s, uint32_t seed, float alpha) {
  const ImVec2 size(max.x - min.x, max.y - min.y);
  for (uint32_t i = 0; i < 220; ++i) {
    const ImVec2 p(min.x + Noise(seed, i) * size.x, min.y + Noise(seed + 1, i) * size.y);
    const float r = (0.6f + std::pow(Noise(seed + 2, i), 4.0f) * 7.0f) * s;
    dl->AddCircleFilled(p, r, Alpha(kInk, alpha * (0.3f + Noise(seed + 3, i) * 0.6f)), 8);
  }
  // Scratches.
  for (uint32_t i = 0; i < 26; ++i) {
    const ImVec2 p(min.x + Noise(seed + 4, i) * size.x, min.y + Noise(seed + 5, i) * size.y);
    const float len = (20.0f + Noise(seed + 6, i) * 120.0f) * s;
    dl->AddLine(p, ImVec2(p.x + len * 0.15f, p.y + len), Alpha(IM_COL32(255, 240, 210, 255),
                alpha * 0.08f), 1.0f * s);
  }
}

void Star(ImDrawList* dl, ImVec2 center, float radius, float angle, ImU32 fill, ImU32 outline,
          float thickness) {
  ImVec2 pts[10];
  for (int i = 0; i < 10; ++i) {
    const float r = (i & 1) ? radius * 0.42f : radius;
    const float a = angle - 1.5707963f + float(i) * 0.6283185f;
    pts[i] = ImVec2(center.x + std::cos(a) * r, center.y + std::sin(a) * r);
  }
  if (fill & IM_COL32_A_MASK) dl->AddConcavePolyFilled(pts, 10, fill);
  if (outline & IM_COL32_A_MASK) dl->AddPolyline(pts, 10, outline, ImDrawFlags_Closed, thickness);
}

void Triangle(ImDrawList* dl, ImVec2 c, float size, bool right, ImU32 color) {
  const float d = right ? 1.0f : -1.0f;
  dl->AddTriangleFilled(ImVec2(c.x + d * size * 0.5f, c.y), ImVec2(c.x - d * size * 0.4f, c.y - size * 0.55f),
                        ImVec2(c.x - d * size * 0.4f, c.y + size * 0.55f), color);
}

float PadGlyph(ImDrawList* dl, ImVec2 pos, float height, std::string_view button, float alpha) {
  ImFont* font = MenuFont();
  ImU32 face = 0;
  if (button == "A") face = IM_COL32(96, 180, 46, 255);
  if (button == "B") face = IM_COL32(214, 44, 36, 255);
  if (button == "X") face = IM_COL32(40, 110, 220, 255);
  if (button == "Y") face = IM_COL32(240, 176, 0, 255);
  if (face) {
    const float r = height * 0.5f;
    const ImVec2 c(pos.x + r, pos.y + r);
    dl->AddCircleFilled(c, r, Alpha(IM_COL32(30, 30, 30, 255), alpha), 24);
    dl->AddCircleFilled(c, r * 0.84f, Alpha(face, alpha), 24);
    const float size = height * 0.72f;
    const ImVec2 t = Measure(font, size, button);
    Text(dl, font, size, ImVec2(c.x - t.x * 0.5f, c.y - size * 0.55f), Alpha(kWhite, alpha),
         button);
    return height;
  }
  // Bumpers, triggers, Start, Back, sticks: a dark pill.
  const float size = height * 0.55f;
  const ImVec2 t = Measure(font, size, button);
  const float w = std::max(height * 1.3f, t.x + height * 0.6f);
  dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + height), Alpha(IM_COL32(48, 46, 44, 255), alpha),
                    height * 0.3f);
  dl->AddRect(pos, ImVec2(pos.x + w, pos.y + height), Alpha(IM_COL32(120, 114, 106, 255), alpha),
              height * 0.3f, 0, 1.5f);
  Text(dl, font, size, ImVec2(pos.x + (w - t.x) * 0.5f, pos.y + (height - size) * 0.45f),
       Alpha(kWhite, alpha), button);
  return w;
}

float KeyCap(ImDrawList* dl, ImVec2 pos, float height, std::string_view key, float alpha) {
  ImFont* font = MenuFont();
  const float size = height * 0.6f;
  const ImVec2 t = Measure(font, size, key);
  const float w = std::max(height, t.x + height * 0.5f);
  dl->AddRectFilled(ImVec2(pos.x, pos.y + height * 0.08f), ImVec2(pos.x + w, pos.y + height),
                    Alpha(IM_COL32(120, 112, 100, 255), alpha), height * 0.18f);
  dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + height * 0.9f),
                    Alpha(IM_COL32(232, 226, 214, 255), alpha), height * 0.18f);
  Text(dl, font, size, ImVec2(pos.x + (w - t.x) * 0.5f, pos.y + (height * 0.9f - size) * 0.4f),
       Alpha(kInk, alpha), key);
  return w;
}

ImVec2 Logo(ImDrawList* dl, ImVec2 pos, float s, float alpha) {
  ImFont* font = MenuFont();
  const float big = 168.0f * s, small = 76.0f * s;
  const ImVec2 name = Measure(font, big, "STUNTMAN");
  ShadowText(dl, font, big, pos, Alpha(kWhite, alpha), "STUNTMAN", 0.2f, 7.0f * s);
  const ImVec2 sub = Measure(font, small, "IGNITION");
  const ImVec2 sub_pos(pos.x + name.x - sub.x + 6.0f * s, pos.y + big * 0.98f);
  ShadowText(dl, font, small, sub_pos, Alpha(kYellow, alpha), "IGNITION", 0.2f, 5.0f * s);
  Star(dl, ImVec2(sub_pos.x - 52.0f * s, sub_pos.y + small * 0.55f), 40.0f * s, -0.2f,
       Alpha(kWhite, alpha), Alpha(kInk, alpha * 0.8f), 3.0f * s);
  return ImVec2(name.x + 20.0f * s, big * 0.98f + small * 1.1f);
}

}  // namespace theme
