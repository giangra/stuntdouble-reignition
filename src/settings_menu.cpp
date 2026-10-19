// stuntman - in-game settings menu. See settings_menu.h.

#include "settings_menu.h"

#include <rex/cvar.h>

#include <imgui.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {

// Settings the runtime reads only at startup. Changing them shows a
// "restart to apply" note until the game is relaunched.
constexpr std::string_view kRestartSettings[] = {
    "resolution_scale",
    "present_effect",
    "present_cas_additional_sharpness",
    "present_fsr_sharpness_reduction",
    "monitor",
    "user_language",
};

// Every setting the menu shows, for "Defaults".
constexpr std::string_view kMenuSettings[] = {
    "resolution_scale",
    "anisotropic_override",
    "present_effect",
    "present_cas_additional_sharpness",
    "present_fsr_sharpness_reduction",
    "fullscreen",
    "vsync",
    "present_letterbox",
    "monitor",
    "user_language",
};

std::map<std::string, std::string, std::less<>>& StartupValues() {
  static std::map<std::string, std::string, std::less<>> values;
  return values;
}

const rex::cvar::FlagEntry* FindFlag(std::string_view name) {
  for (const auto& entry : rex::cvar::GetRegistry()) {
    if (entry.name == name) return &entry;
  }
  return nullptr;
}

std::string Get(std::string_view name) { return rex::cvar::GetFlagByName(name); }

void Set(std::string_view name, std::string_view value) {
  rex::cvar::SetFlagByName(name, value);
}

struct Choice {
  int value;
  const char* label;
};

// A combo over fixed integer values. Unknown current values show as-is.
void IntCombo(const char* label, std::string_view flag, std::span<const Choice> choices) {
  const int current = std::atoi(Get(flag).c_str());
  std::string preview = std::to_string(current);
  for (const auto& c : choices) {
    if (c.value == current) preview = c.label;
  }
  if (ImGui::BeginCombo(label, preview.c_str())) {
    for (const auto& c : choices) {
      const bool selected = c.value == current;
      if (ImGui::Selectable(c.label, selected)) Set(flag, std::to_string(c.value));
      if (selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
}

void BoolCheckbox(const char* label, std::string_view flag) {
  bool value = Get(flag) == "true";
  if (ImGui::Checkbox(label, &value)) Set(flag, value ? "true" : "false");
}

// A slider over a double cvar's own range.
void DoubleSlider(const char* label, std::string_view flag) {
  const auto* entry = FindFlag(flag);
  if (!entry) return;
  const float min = static_cast<float>(entry->constraints.min.value_or(0.0));
  const float max = static_cast<float>(entry->constraints.max.value_or(1.0));
  float value = static_cast<float>(std::atof(Get(flag).c_str()));
  if (ImGui::SliderFloat(label, &value, min, max, "%.2f")) {
    Set(flag, std::to_string(std::clamp(value, min, max)));
  }
}

void Hint(const char* text) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", text);
}

void RestartBadge(std::string_view flag) {
  const auto& startup = StartupValues();
  auto it = startup.find(flag);
  if (it != startup.end() && it->second != Get(flag)) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "restart");
  }
}

const char* EffectLabel(std::string_view effect) {
  if (effect == "bilinear") return "Bilinear (original)";
  if (effect == "cas") return "AMD FidelityFX CAS (sharpen)";
  if (effect == "fsr") return "AMD FidelityFX FSR 1";
  if (effect == "fsr2") return "AMD FidelityFX FSR 2";
  if (effect == "fsr3") return "AMD FidelityFX FSR 3";
  return nullptr;
}

}  // namespace

SettingsMenu::SettingsMenu(rex::ui::ImGuiDrawer* drawer, std::filesystem::path config_path,
                           std::function<void()> on_closed)
    : ImGuiDialog(drawer),
      config_path_(std::move(config_path)),
      on_closed_(std::move(on_closed)) {}

void SettingsMenu::CaptureStartupValues() {
  auto& startup = StartupValues();
  for (auto name : kRestartSettings) {
    startup.emplace(std::string(name), Get(name));
  }
}

bool SettingsMenu::PendingRestart() const {
  for (const auto& [name, value] : StartupValues()) {
    if (Get(name) != value) return true;
  }
  return false;
}

void SettingsMenu::OnClose() {
  if (on_closed_) on_closed_();
}

void SettingsMenu::OnDraw(ImGuiIO& io) {
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                          ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(0.92f);

  bool open = true;
  if (!ImGui::Begin("Stuntman: Ignition settings##stuntman", &open,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    if (!open) Close();
    return;
  }

  if (ImGui::BeginTabBar("##tabs")) {
    if (ImGui::BeginTabItem("Graphics")) {
      DrawGraphics();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Display")) {
      DrawDisplay();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Game")) {
      DrawGame();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  // Bottom bar
  ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing() * 2.2f);
  ImGui::Separator();
  if (PendingRestart()) {
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f),
                       "Some changes apply after you save and restart the game.");
  } else if (!status_.empty()) {
    ImGui::TextDisabled("%s", status_.c_str());
  } else {
    ImGui::TextDisabled("F1 opens and closes this menu.");
  }

  if (ImGui::Button("Save")) {
    rex::cvar::SaveConfig(config_path_);
    status_ = "Saved to " + config_path_.filename().string() + ".";
  }
  ImGui::SameLine();
  if (ImGui::Button("Defaults")) {
    for (auto name : kMenuSettings) {
      if (const auto* entry = FindFlag(name)) Set(name, entry->default_value);
    }
    status_ = "Defaults restored. Save to keep them.";
  }
  ImGui::SameLine();
  if (ImGui::Button("Close")) open = false;

  ImGui::End();
  if (!open) Close();
}

void SettingsMenu::DrawGraphics() {
  static constexpr Choice kScales[] = {
      {1, "1x - 1280x720 (original)"},
      {2, "2x - 2560x1440"},
      {3, "3x - 3840x2160"},
  };
  IntCombo("Internal resolution", "resolution_scale", kScales);
  RestartBadge("resolution_scale");
  Hint("Renders the game at a multiple of the console's 720p.\nHigher values need a faster GPU.");

  static constexpr Choice kAniso[] = {
      {-1, "Game default"}, {0, "Off"}, {2, "2x"}, {3, "4x"}, {4, "8x"}, {5, "16x"},
  };
  IntCombo("Anisotropic filtering", "anisotropic_override", kAniso);
  Hint("Keeps the road and ground textures sharp at a distance.");

  ImGui::Spacing();
  ImGui::SeparatorText("Upscaling");

  const std::string effect = Get("present_effect");
  const char* effect_label = EffectLabel(effect);
  if (ImGui::BeginCombo("Output filter", effect_label ? effect_label : effect.c_str())) {
    if (const auto* entry = FindFlag("present_effect")) {
      for (const auto& option : entry->constraints.allowed_values) {
        const char* label = EffectLabel(option);
        const bool selected = option == effect;
        if (ImGui::Selectable(label ? label : option.c_str(), selected)) {
          Set("present_effect", option);
        }
        if (selected) ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  RestartBadge("present_effect");
  Hint("How the rendered image is scaled to the window.");

  if (effect == "cas") {
    DoubleSlider("Extra sharpness", "present_cas_additional_sharpness");
    RestartBadge("present_cas_additional_sharpness");
  } else if (effect.starts_with("fsr")) {
    DoubleSlider("Sharpness reduction", "present_fsr_sharpness_reduction");
    RestartBadge("present_fsr_sharpness_reduction");
    Hint("In stops: 0 is the sharpest.");
  }
}

void SettingsMenu::DrawDisplay() {
  BoolCheckbox("Fullscreen", "fullscreen");
  BoolCheckbox("VSync", "vsync");
  Hint("Syncs frames to the monitor's refresh to avoid tearing.");
  BoolCheckbox("Keep 16:9 (black bars on other aspect ratios)", "present_letterbox");

  static constexpr Choice kMonitors[] = {
      {0, "Default"}, {1, "Monitor 1"}, {2, "Monitor 2"}, {3, "Monitor 3"}, {4, "Monitor 4"},
  };
  IntCombo("Monitor", "monitor", kMonitors);
  RestartBadge("monitor");
}

void SettingsMenu::DrawGame() {
  // The disc carries text and speech for these five (game/text/*).
  static constexpr Choice kLanguages[] = {
      {1, "English"}, {4, "Francais"}, {3, "Deutsch"}, {5, "Espanol"}, {6, "Italiano"},
  };
  IntCombo("Language", "user_language", kLanguages);
  RestartBadge("user_language");
  Hint("The console language the game reads at startup.");

  ImGui::Spacing();
  ImGui::SeparatorText("Frame rate");
  ImGui::BeginDisabled();
  int fps = 30;
  ImGui::RadioButton("30 FPS (original)", &fps, 30);
  ImGui::SameLine();
  ImGui::RadioButton("60 FPS", &fps, 60);
  ImGui::EndDisabled();
  ImGui::TextDisabled("60 FPS needs a game patch that is still in the works.");
}
