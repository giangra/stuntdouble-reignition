// stuntman - in-game settings menu
//
// A short list of the options a player cares about (graphics, display,
// language), on top of the SDK's cvars. Saves to the same stuntman.toml the
// runtime loads at startup. The SDK's raw cvar editor stays on F4.

#pragma once

#include <rex/ui/imgui_dialog.h>

#include <filesystem>
#include <functional>
#include <string>

class SettingsMenu : public rex::ui::ImGuiDialog {
 public:
  // on_closed runs when the menu deletes itself (Close button or toggle).
  SettingsMenu(rex::ui::ImGuiDrawer* drawer, std::filesystem::path config_path,
               std::function<void()> on_closed);

  // Records the values the game started with, to flag the settings that
  // only take effect after a restart. Call once, after the config is loaded.
  static void CaptureStartupValues();

  void RequestClose() { Close(); }

 protected:
  void OnDraw(ImGuiIO& io) override;
  void OnClose() override;

 private:
  void DrawGraphics();
  void DrawDisplay();
  void DrawGame();
  bool PendingRestart() const;

  std::filesystem::path config_path_;
  std::function<void()> on_closed_;
  std::string status_;
};
