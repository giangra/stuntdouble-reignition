// stuntman - PC menu
//
// One menu, drawn in the style of the game's front end (ui_theme.h), used in
// two ways:
//   - start screen: full screen when the game starts, before the game itself
//     is launched. Play starts it; Video, Controls, Game and Achievements are
//     there too, and Quit closes the program. Nothing runs underneath, so a
//     language change applies at once.
//   - pause menu: F1 (or Back + Start) over the running game, which gets no
//     input while it is open. The first entry resumes the game.
// Usable with keyboard, mouse and controller. Every change is applied and
// saved to stuntman.toml at once; settings the runtime reads only at startup
// offer a restart.

#pragma once

#include <rex/ui/imgui_dialog.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rex {
class Runtime;
}
namespace rex::ui {
class AchievementIconCache;
class ImmediateDrawer;
}

class PcMenu : public rex::ui::ImGuiDialog {
 public:
  enum class Mode { kStart, kPause };

  struct Hooks {
    std::function<void()> play;       // start screen: launch the game
    std::function<void()> quit;       // close the program
    std::function<void()> closed;     // the dialog is about to delete itself
    std::function<void()> poll_pads;  // start screen: read the controllers
  };

  PcMenu(rex::ui::ImGuiDrawer* drawer, Mode mode, std::filesystem::path config_path,
         rex::Runtime* runtime, rex::ui::ImmediateDrawer* immediate_drawer, Hooks hooks);
  ~PcMenu() override;

  Mode mode() const { return mode_; }

  // Plays the closing animation, then closes (pause menu only).
  void RequestClose();
  // Opens a page by name ("video", "controls", "achievements"...).
  void OpenPage(const std::string& name);
  // True while waiting for a key to bind: shortcut keys must not fire.
  bool IsCapturing() const { return capture_slot_ >= 0; }

  // Records the values the game started with, for the restart notice. Call
  // once the config is loaded, and again (language only) when the game is
  // launched from the start screen.
  static void CaptureStartupValues();
  static void CaptureLanguage();

  // First start (nothing in stuntman.toml yet): render at the desktop's
  // resolution, in Windows' language. Call after the config is loaded and
  // before the window is created.
  static void ApplyFirstRunDefaults();

 protected:
  void OnDraw(ImGuiIO& io) override;
  void OnClose() override;

 private:
  struct Choice {
    std::string label;
    std::string value;
  };

  struct Row {
    enum class Kind { kAction, kChoice, kPage, kKey, kPadButton };
    Kind kind = Kind::kAction;
    std::string label;
    std::string help;
    std::string cvar;                 // kChoice / kKey: the setting
    std::vector<Choice> choices;      // kChoice
    std::function<std::string()> get;               // kChoice: custom read
    std::function<void(const std::string&)> set;    // kChoice: custom write
    std::function<bool()> enabled;    // greyed out when it returns false
    std::function<void()> action;     // kAction
    int page = -1;                    // kPage
    int pad_button = -1;              // kPadButton
    bool quit = false;                // the Quit entry (asks twice in game)
  };

  struct Page {
    std::string name;
    std::string title;
    std::vector<Row> rows;
    int selected = 0;
    float scroll = 0.0f;
    bool achievements = false;
  };

  struct Nav {
    bool up = false, down = false, left = false, right = false;
    bool confirm = false, back = false, clear = false, restart = false;
  };

  void BuildPages();
  int PageIndex(const std::string& name) const;
  void Enter(int page);
  void Back();

  Nav ReadNav(ImGuiIO& io);
  void Handle(const Nav& nav);
  void Activate(Row& row, int direction);
  void Change(Row& row, int direction);
  void Capture();
  void SaveConfig();
  bool RowEnabled(const Row& row) const;
  std::string RowValue(const Row& row) const;
  bool PendingRestart() const;
  void Restart();

  void DrawBackground(ImDrawList* dl, ImVec2 display, float s, float fade);
  void DrawRoot(ImDrawList* dl, ImGuiIO& io, float s, float fade);
  void DrawList(ImDrawList* dl, ImGuiIO& io, float s, float fade);
  void DrawAchievements(ImDrawList* dl, ImGuiIO& io, float s, float fade);
  void DrawPrompts(ImDrawList* dl, ImVec2 display, float s, float fade);
  void DrawKeys(ImDrawList* dl, const Row& row, bool selected, ImVec2 right, float h, float s,
                float alpha);

  Mode mode_;
  std::filesystem::path config_path_;
  rex::Runtime* runtime_;
  rex::ui::ImmediateDrawer* immediate_drawer_;
  Hooks hooks_;
  std::unique_ptr<rex::ui::AchievementIconCache> icons_;

  std::vector<Page> pages_;
  std::vector<int> stack_;  // pages opened from the root, current last
  int page_ = 0;
  std::string language_;    // user_language the pages were built for

  int capture_slot_ = -1;   // keyboard page: which key of the row is rebound
  int key_slot_ = 0;        // keyboard page: key 1 or key 2 selected
  int capture_frames_ = 0;
  bool quit_armed_ = false;
  bool closing_ = false;
  bool playing_ = false;

  double time_ = 0.0;
  double page_time_ = 0.0;
  double close_time_ = 0.0;
  float banner_y_ = -1.0f;  // animated selection position
  float flash_ = 0.0f;

  // Controller navigation: edge detection and auto-repeat.
  uint32_t pad_held_ = 0;
  double repeat_at_ = 0.0;
  bool pad_prompts_ = false;  // show controller buttons rather than keys

  // Mouse: rows under the cursor in this frame, and its last position.
  struct HitRow {
    ImVec2 min, max;
    int row;
  };
  std::vector<HitRow> hits_;
  float hit_angle_ = 0.0f;
  ImVec2 hit_center_;
  ImVec2 last_mouse_ = ImVec2(-1, -1);
  bool mouse_moved_ = false;
};
