// stuntman - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/filesystem.h>
#include <rex/rex_app.h>
#include <rex/ui/keybinds.h>

#include <filesystem>

#include "achievement_catalog.h"
#include "settings_menu.h"

class StuntmanApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<StuntmanApp>(new StuntmanApp(ctx, "stuntman",
        PPCImageConfig));
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}

  void OnPreSetup(rex::RuntimeConfig& config) override {
    // Xenos GPU emulation (D3D12 on Windows) from the rexgpu-xenos plugin
    config.gpu_plugin = "xenos";
  }

  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // F1 toggles our settings menu. The menu deletes itself when closed.
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    SettingsMenu::CaptureStartupValues();
    rex::ui::RegisterBind("bind_stuntman_settings", "F1", "Toggle Stuntman settings menu",
                          [this, drawer] {
                            if (settings_menu_) {
                              settings_menu_->RequestClose();
                            } else {
                              settings_menu_ = new SettingsMenu(
                                  drawer, config_path_, [this] { settings_menu_ = nullptr; });
                            }
                          });
  }

  // Achievement text and icons for the launcher (see achievement_catalog.h).
  void OnPreLaunchModule() override {
    if (auto* rt = runtime()) WriteAchievementCatalog(rt->kernel_state(), rt->user_data_root());
  }

  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;

  void OnShutdown() override {
    delete settings_menu_;
    settings_menu_ = nullptr;
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
  std::filesystem::path config_path_;
  SettingsMenu* settings_menu_ = nullptr;
};
