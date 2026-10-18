// stuntman - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/filesystem.h>
#include <rex/rex_app.h>

#include <filesystem>

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
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}

  // Game data lives in a "game" folder next to the exe or in the working
  // directory (the repo root during development).
  void OnConfigurePaths(rex::PathConfig& paths) override {
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
};
