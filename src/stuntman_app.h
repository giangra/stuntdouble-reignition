// stuntman - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/input/input_system.h>
#include <rex/rex_app.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/window.h>

#include <cstdlib>
#include <filesystem>

#include "bench.h"
#include "fps_overlay.h"
#include "frame_rate.h"
#include "pad_input.h"
#include "pc_menu.h"
#include "ui_theme.h"

class StuntmanApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<StuntmanApp>(new StuntmanApp(ctx, "stuntman",
        PPCImageConfig));
  }

  // The config is loaded by now; the window doesn't exist yet.
  void OnPostInitLogging() override {
    PcMenu::ApplyFirstRunDefaults();
    SyncFramePacing();
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    // Xenos GPU emulation (D3D12 on Windows) from the rexgpu-xenos plugin
    config.gpu_plugin = "xenos";
  }

  // The PC menu's fonts, built into the exe (src/stuntman.rc).
  void OnConfigureFonts(ImFontAtlas* atlas) override { theme::LoadFonts(atlas); }

  // F1 opens the PC menu over the game, F2 the frame rate counter. The
  // config is loaded by now, but the runtime doesn't exist yet.
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    pad::ApplyKeyboardDefaults();
    PcMenu::CaptureStartupValues();
    rex::ui::RegisterBind("bind_stuntman_menu", "F1", "Open the PC menu", [this] { ToggleMenu(); });
    rex::ui::RegisterBind("bind_stuntman_fps", "F2", "Show the frame rate", [this] {
      if (menu_ && menu_->IsCapturing()) return;
      const bool on = rex::cvar::GetFlagByName("show_fps") == "true";
      rex::cvar::SetFlagByName("show_fps", on ? "false" : "true");
      rex::cvar::SaveConfig(config_path_);
    });
    new FpsOverlay(drawer, [this] { Tick(); });  // owned by the drawer
  }

  void OnPostSetup() override {
    if (window()) window()->SetTitle("Stuntman: Ignition");
    if (auto* rt = runtime()) {
      input_ = static_cast<rex::input::InputSystem*>(rt->input_system());
      pad::Install(input_);
    }
  }

  // With the start screen on, the game is launched only when Play is chosen.
  void LaunchModule() override {
    if (launched_ || rex::cvar::GetFlagByName("start_screen") != "true" || !imgui_drawer() ||
        (std::getenv("STUNTMAN_BENCH") && !std::getenv("STUNTMAN_MENU_PAGE"))) {
      Launch();
      return;
    }
    PcMenu::Hooks hooks;
    hooks.play = [this] { Launch(); };
    hooks.quit = [this] { Quit(); };
    hooks.closed = [this] { menu_ = nullptr; };
    hooks.poll_pads = [this] { pad::PollForMenu(input_); };
    menu_ = new PcMenu(imgui_drawer(), PcMenu::Mode::kStart, config_path_, runtime(),
                       immediate_drawer(), std::move(hooks));
    // For screenshots of the menu pages: STUNTMAN_MENU_PAGE=video, keyboard...
    if (const char* page = std::getenv("STUNTMAN_MENU_PAGE")) {
      menu_->OpenPage(page);
      bench::Start();  // its "shot" steps capture the page
    }
  }

  void OnShutdown() override {
    rex::ui::UnregisterBind("bind_stuntman_menu");
    rex::ui::UnregisterBind("bind_stuntman_fps");
  }

  // Game data lives in a "game" folder next to the exe or in the working
  // directory (the repo root during development).
  void OnConfigurePaths(rex::PathConfig& paths) override {
    config_path_ = paths.config_path;
    if (!paths.game_data_root.empty()) return;
    for (const auto& dir : {rex::filesystem::GetExecutableFolder() / "game",
                            std::filesystem::current_path() / "game"}) {
      std::error_code ec;
      if (std::filesystem::exists(dir / "default.xex", ec)) {
        paths.game_data_root = dir;
        return;
      }
    }
  }

 private:
  void Launch() {
    if (launched_) return;
    launched_ = true;
    PcMenu::CaptureLanguage();
    bench::Start();
    rex::ReXApp::LaunchModule();
  }

  void Quit() {
    if (window()) window()->RequestClose();
  }

  void ToggleMenu() {
    if (menu_) {
      if (menu_->mode() == PcMenu::Mode::kPause && !menu_->IsCapturing()) menu_->RequestClose();
      return;
    }
    if (!launched_ || !imgui_drawer()) return;
    PcMenu::Hooks hooks;
    hooks.quit = [this] { Quit(); };
    hooks.closed = [this] { menu_ = nullptr; };
    menu_ = new PcMenu(imgui_drawer(), PcMenu::Mode::kPause, config_path_, runtime(),
                       immediate_drawer(), std::move(hooks));
  }

  // Every frame: Back + Start on a controller toggles the PC menu.
  void Tick() {
    const uint32_t buttons = pad::MenuButtons();
    constexpr uint32_t kCombo = (1u << pad::kBack) | (1u << pad::kStart);
    const bool held = (buttons & kCombo) == kCombo;
    if (held && !combo_held_) ToggleMenu();
    combo_held_ = held;
  }

  std::filesystem::path config_path_;
  rex::input::InputSystem* input_ = nullptr;
  PcMenu* menu_ = nullptr;  // owned by the ImGui drawer; deletes itself on close
  bool launched_ = false;
  bool combo_held_ = false;
};
