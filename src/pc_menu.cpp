// stuntman - PC menu. See pc_menu.h.

#include "pc_menu.h"

#include "pad_input.h"
#include "ui_theme.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/overlay/achievement_icon_cache.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <string_view>
#include <utility>

REXCVAR_DEFINE_BOOL(start_screen, true, "Stuntman",
                    "Show the start screen (Play / settings / achievements) when the game starts");
REXCVAR_DEFINE_BOOL(show_fps, false, "Stuntman", "Show the frame rate counter");

namespace {

using theme::Alpha;

// ---- Language --------------------------------------------------------------

// The menu follows the game's language: Italian, or English for the others.
bool g_italian = false;
const char* tr(const char* en, const char* it) { return g_italian ? it : en; }

// ---- Settings --------------------------------------------------------------

std::string Get(std::string_view name) { return rex::cvar::GetFlagByName(name); }
void Set(std::string_view name, std::string_view value) { rex::cvar::SetFlagByName(name, value); }
int GetInt(std::string_view name) { return std::atoi(Get(name).c_str()); }

// Settings the runtime reads only at startup.
constexpr std::string_view kRestartSettings[] = {
    "window_width", "window_height", "video_mode_width", "video_mode_height",
    "resolution_scale", "present_effect", "monitor",
};

std::map<std::string, std::string, std::less<>>& StartupValues() {
  static std::map<std::string, std::string, std::less<>> values;
  return values;
}

// ---- Display modes ---------------------------------------------------------

struct Size {
  int w, h;
  bool operator==(const Size&) const = default;
};

Size Desktop() {
  DEVMODEW mode{};
  mode.dmSize = sizeof(mode);
  if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmPelsWidth > 0) {
    return {int(mode.dmPelsWidth), int(mode.dmPelsHeight)};
  }
  return {1920, 1080};
}

bool Is16x9(Size s) { return std::abs(s.w * 9 - s.h * 16) <= 16; }

// The largest 16:9 size that fits, rounded to even pixels.
Size Fit16x9(Size s) {
  if (Is16x9(s)) return s;
  if (s.w * 9 > s.h * 16) return {(s.h * 16 / 9) & ~1, s.h};
  return {s.w, (s.w * 9 / 16) & ~1};
}

std::string AspectName(Size s) {
  const double r = double(s.w) / s.h;
  if (Is16x9(s)) return "16:9";
  if (std::abs(r - 1.6) < 0.02) return "16:10";
  if (std::abs(r - 4.0 / 3.0) < 0.02) return "4:3";
  if (r > 3.3) return "32:9";
  if (r > 2.2) return "21:9";
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%.2f:1", r);
  return buf;
}

// Common sizes that fit on the desktop, plus the desktop itself.
std::vector<Size> Resolutions() {
  static constexpr Size kPresets[] = {
      {1280, 720},  {1600, 900},  {1920, 1080}, {2560, 1440}, {3840, 2160},
      {2560, 1080}, {3440, 1440}, {3840, 1600}, {5120, 2160}, {3840, 1080},
      {5120, 1440}, {1920, 1200}, {2560, 1600},
  };
  const Size desktop = Desktop();
  std::vector<Size> list;
  for (Size p : kPresets) {
    if (p.w <= desktop.w && p.h <= desktop.h) list.push_back(p);
  }
  if (std::find(list.begin(), list.end(), desktop) == list.end()) list.push_back(desktop);
  std::sort(list.begin(), list.end(), [](Size a, Size b) {
    return a.w * a.h != b.w * b.h ? a.w * a.h < b.w * b.h : a.w < b.w;
  });
  return list;
}

Size CurrentResolution() {
  Size s{GetInt("window_width"), GetInt("window_height")};
  return s.w > 0 && s.h > 0 ? s : Desktop();
}

// "bars" keeps the game's 16:9 picture with black bars around it; "stretch"
// renders at the full size (the game keeps a 16:9 view, so it is stretched).
std::string CurrentAspect() {
  const Size window = CurrentResolution();
  const Size mode{GetInt("video_mode_width"), GetInt("video_mode_height")};
  return !Is16x9(window) && mode == window ? "stretch" : "bars";
}

// Same rules as the old launcher: the guest video mode is the window size, or
// the largest 16:9 area in it.
void ApplyResolution(Size window, const std::string& aspect) {
  const Size mode = aspect == "stretch" ? window : Fit16x9(window);
  Set("resolution", "");
  Set("window_width", std::to_string(window.w));
  Set("window_height", std::to_string(window.h));
  Set("video_mode_width", std::to_string(mode.w));
  Set("video_mode_height", std::to_string(mode.h));
  Set("present_letterbox", "true");
}

// ---- Keys --------------------------------------------------------------------

struct KeyName {
  ImGuiKey key;
  const char* name;  // as rex::ui::ParseVirtualKey reads it
};

// Keys that can be bound. Modifiers are left out: the SDK's keyboard device
// treats them as prefixes, so a bare Shift would never fire.
constexpr KeyName kKeyNames[] = {
    {ImGuiKey_A, "A"}, {ImGuiKey_B, "B"}, {ImGuiKey_C, "C"}, {ImGuiKey_D, "D"},
    {ImGuiKey_E, "E"}, {ImGuiKey_F, "F"}, {ImGuiKey_G, "G"}, {ImGuiKey_H, "H"},
    {ImGuiKey_I, "I"}, {ImGuiKey_J, "J"}, {ImGuiKey_K, "K"}, {ImGuiKey_L, "L"},
    {ImGuiKey_M, "M"}, {ImGuiKey_N, "N"}, {ImGuiKey_O, "O"}, {ImGuiKey_P, "P"},
    {ImGuiKey_Q, "Q"}, {ImGuiKey_R, "R"}, {ImGuiKey_S, "S"}, {ImGuiKey_T, "T"},
    {ImGuiKey_U, "U"}, {ImGuiKey_V, "V"}, {ImGuiKey_W, "W"}, {ImGuiKey_X, "X"},
    {ImGuiKey_Y, "Y"}, {ImGuiKey_Z, "Z"},
    {ImGuiKey_0, "0"}, {ImGuiKey_1, "1"}, {ImGuiKey_2, "2"}, {ImGuiKey_3, "3"},
    {ImGuiKey_4, "4"}, {ImGuiKey_5, "5"}, {ImGuiKey_6, "6"}, {ImGuiKey_7, "7"},
    {ImGuiKey_8, "8"}, {ImGuiKey_9, "9"},
    {ImGuiKey_Space, "Space"}, {ImGuiKey_Enter, "Return"}, {ImGuiKey_Tab, "Tab"},
    {ImGuiKey_Backspace, "Backspace"}, {ImGuiKey_Delete, "Delete"},
    {ImGuiKey_Insert, "Insert"}, {ImGuiKey_Home, "Home"}, {ImGuiKey_End, "End"},
    {ImGuiKey_PageUp, "PageUp"}, {ImGuiKey_PageDown, "PageDown"},
    {ImGuiKey_LeftArrow, "Left"}, {ImGuiKey_RightArrow, "Right"},
    {ImGuiKey_UpArrow, "Up"}, {ImGuiKey_DownArrow, "Down"},
    {ImGuiKey_Minus, "Minus"}, {ImGuiKey_Equal, "Plus"}, {ImGuiKey_Comma, "Comma"},
    {ImGuiKey_Period, "Period"}, {ImGuiKey_Semicolon, "Semicolon"},
    {ImGuiKey_Slash, "Slash"}, {ImGuiKey_Backslash, "Backslash"},
    {ImGuiKey_LeftBracket, "LBracket"}, {ImGuiKey_RightBracket, "RBracket"},
    {ImGuiKey_Apostrophe, "Quote"}, {ImGuiKey_GraveAccent, "Backtick"},
    {ImGuiKey_Keypad0, "Numpad0"}, {ImGuiKey_Keypad1, "Numpad1"},
    {ImGuiKey_Keypad2, "Numpad2"}, {ImGuiKey_Keypad3, "Numpad3"},
    {ImGuiKey_Keypad4, "Numpad4"}, {ImGuiKey_Keypad5, "Numpad5"},
    {ImGuiKey_Keypad6, "Numpad6"}, {ImGuiKey_Keypad7, "Numpad7"},
    {ImGuiKey_Keypad8, "Numpad8"}, {ImGuiKey_Keypad9, "Numpad9"},
    {ImGuiKey_KeypadAdd, "NumpadPlus"}, {ImGuiKey_KeypadSubtract, "NumpadMinus"},
    {ImGuiKey_KeypadMultiply, "NumpadStar"}, {ImGuiKey_KeypadDivide, "NumpadSlash"},
    {ImGuiKey_F5, "F5"}, {ImGuiKey_F6, "F6"}, {ImGuiKey_F8, "F8"}, {ImGuiKey_F9, "F9"},
    {ImGuiKey_F10, "F10"}, {ImGuiKey_F11, "F11"}, {ImGuiKey_F12, "F12"},
    {ImGuiKey_Escape, "Escape"},
};

// How a key name is shown on a key cap.
std::string KeyLabel(std::string_view name) {
  if (name == "Return") return tr("ENTER", "INVIO");
  if (name == "Escape") return "ESC";
  if (name == "Space") return tr("SPACE", "SPAZIO");
  if (name == "Backspace") return "BACKSPACE";
  if (name == "Left") return tr("LEFT", "SINISTRA");
  if (name == "Right") return tr("RIGHT", "DESTRA");
  if (name == "Up") return tr("UP", "SU");
  if (name == "Down") return tr("DOWN", "GIÙ");
  std::string out(name);
  for (char& c : out) c = char(std::toupper(static_cast<unsigned char>(c)));
  if (out.starts_with("NUMPAD")) out = "NUM " + out.substr(6);
  return out;
}

std::vector<std::string> SplitKeys(const std::string& value) {
  std::vector<std::string> keys;
  size_t start = 0;
  while (start <= value.size()) {
    size_t comma = value.find(',', start);
    if (comma == std::string::npos) comma = value.size();
    std::string key = value.substr(start, comma - start);
    key.erase(0, key.find_first_not_of(' '));
    key.erase(key.find_last_not_of(' ') + 1);
    if (!key.empty()) keys.push_back(key);
    start = comma + 1;
  }
  return keys;
}

// ---- Keyboard and controller pages ------------------------------------------

struct KeyAction {
  const char* cvar;
  const char* en;
  const char* it;
};

// The game's own control names (text/menu.bin), with the pad button they are on.
constexpr KeyAction kKeyActions[] = {
    {"keybind_right_trigger", "GAS (RT)", "ACCELERATORE (RT)"},
    {"keybind_left_trigger", "BRAKE / REVERSE (LT)", "FRENO / RETROMARCIA (LT)"},
    {"keybind_lstick_left", "STEER LEFT", "STERZA A SINISTRA"},
    {"keybind_lstick_right", "STEER RIGHT", "STERZA A DESTRA"},
    {"keybind_lstick_up", "LEFT STICK UP", "LEVETTA SINISTRA SU"},
    {"keybind_lstick_down", "LEFT STICK DOWN", "LEVETTA SINISTRA GIÙ"},
    {"keybind_a", "A (CONFIRM)", "A (CONFERMA)"},
    {"keybind_b", "B (BACK)", "B (INDIETRO)"},
    {"keybind_x", "X", "X"},
    {"keybind_y", "Y", "Y"},
    {"keybind_left_shoulder", "LB", "LB"},
    {"keybind_right_shoulder", "RB", "RB"},
    {"keybind_lstick_press", "LEFT STICK CLICK", "CLIC LEVETTA SINISTRA"},
    {"keybind_rstick_press", "RIGHT STICK CLICK", "CLIC LEVETTA DESTRA"},
    {"keybind_rstick_up", "CAMERA UP", "TELECAMERA SU"},
    {"keybind_rstick_down", "CAMERA DOWN", "TELECAMERA GIÙ"},
    {"keybind_rstick_left", "CAMERA LEFT", "TELECAMERA SINISTRA"},
    {"keybind_rstick_right", "CAMERA RIGHT", "TELECAMERA DESTRA"},
    {"keybind_dpad_up", "D-PAD UP", "CROCE SU"},
    {"keybind_dpad_down", "D-PAD DOWN", "CROCE GIÙ"},
    {"keybind_dpad_left", "D-PAD LEFT", "CROCE SINISTRA"},
    {"keybind_dpad_right", "D-PAD RIGHT", "CROCE DESTRA"},
    {"keybind_back", "BACK", "BACK"},
    {"keybind_start", "START (PAUSE)", "START (PAUSA)"},
};

const char* PadButtonLabel(int button) {
  switch (button) {
    case pad::kUp: return tr("D-PAD UP", "CROCE SU");
    case pad::kDown: return tr("D-PAD DOWN", "CROCE GIÙ");
    case pad::kLeft: return tr("D-PAD LEFT", "CROCE SINISTRA");
    case pad::kRight: return tr("D-PAD RIGHT", "CROCE DESTRA");
    case pad::kLS: return tr("LEFT STICK CLICK", "CLIC LEVETTA SX");
    case pad::kRS: return tr("RIGHT STICK CLICK", "CLIC LEVETTA DX");
    case pad::kNone: return tr("NOTHING", "NIENTE");
    default: return pad::ButtonLabel(button);
  }
}

// ---- Achievements --------------------------------------------------------------

struct Achievement {
  rex::system::AchievementInfo info;
  uint64_t unlocked = 0;  // FILETIME, 0 if locked
};

std::vector<Achievement> LoadAchievements(rex::Runtime* runtime) {
  std::vector<Achievement> list;
  auto* ks = runtime ? runtime->kernel_state() : nullptr;
  if (!ks) return list;
  for (auto& info : ks->loaded_achievements()) {
    const uint64_t t = ks->GetAchievementUnlockTime(info.id);
    list.push_back({std::move(info), t});
  }
  // Newest unlocks first, then the locked ones in game order.
  std::stable_sort(list.begin(), list.end(), [](const Achievement& a, const Achievement& b) {
    if ((a.unlocked != 0) != (b.unlocked != 0)) return a.unlocked != 0;
    return a.unlocked > b.unlocked;
  });
  return list;
}

std::string FormatFiletime(uint64_t filetime) {
  if (!filetime) return "";
  const time_t t = time_t((filetime - 116444736000000000ull) / 10000000ull);
  tm local{};
  localtime_s(&local, &t);
  char buf[32];
  std::strftime(buf, sizeof(buf), g_italian ? "%d/%m/%Y %H:%M" : "%Y-%m-%d %H:%M", &local);
  return buf;
}

// Achievements are listed by page; the page keeps them while it is open.
std::vector<Achievement> g_achievements;

// ---- Drawing helpers ------------------------------------------------------------

float Ease(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

ImVec2 RotatePoint(ImVec2 p, ImVec2 center, float angle) {
  const float c = std::cos(angle), s = std::sin(angle);
  const float x = p.x - center.x, y = p.y - center.y;
  return ImVec2(center.x + x * c - y * s, center.y + x * s + y * c);
}

bool Inside(ImVec2 p, ImVec2 min, ImVec2 max) {
  return p.x >= min.x && p.x < max.x && p.y >= min.y && p.y < max.y;
}

// Page titles shrink to stay in the left column (slant included).
float TitleSize(ImFont* font, const std::string& title, float width, float s) {
  const float size = 104.0f * s;
  const float w = theme::Measure(font, size, title).x * 1.08f + 20.0f * s;
  return w > width ? std::max(56.0f * s, size * width / w) : size;
}

struct Prompt {
  const char* pad;
  const char* key;
  std::string label;
};

}  // namespace

// ==== PcMenu =========================================================================

PcMenu::PcMenu(rex::ui::ImGuiDrawer* drawer, Mode mode, std::filesystem::path config_path,
               rex::Runtime* runtime, rex::ui::ImmediateDrawer* immediate_drawer, Hooks hooks)
    : ImGuiDialog(drawer),
      mode_(mode),
      config_path_(std::move(config_path)),
      runtime_(runtime),
      immediate_drawer_(immediate_drawer),
      hooks_(std::move(hooks)) {
  BuildPages();
  pad::SetMenuOpen(true);
  // Buttons already held (the Back + Start that opened the menu) don't count.
  pad_held_ = pad::MenuButtons();
}

PcMenu::~PcMenu() = default;

void PcMenu::CaptureStartupValues() {
  auto& startup = StartupValues();
  for (auto name : kRestartSettings) startup[std::string(name)] = Get(name);
  CaptureLanguage();
}

void PcMenu::ApplyFirstRunDefaults() {
  using rex::cvar::Source;
  if (rex::cvar::GetFlagSource("window_width") == Source::kDefault &&
      rex::cvar::GetFlagSource("video_mode_width") == Source::kDefault &&
      rex::cvar::GetFlagSource("resolution") == Source::kDefault) {
    ApplyResolution(Desktop(), "bars");
  }
  if (rex::cvar::GetFlagSource("user_language") == Source::kDefault) {
    const char* id = "1";
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
      case LANG_ITALIAN: id = "6"; break;
      case LANG_FRENCH: id = "4"; break;
      case LANG_GERMAN: id = "3"; break;
      case LANG_SPANISH: id = "5"; break;
    }
    Set("user_language", id);
  }
}

void PcMenu::CaptureLanguage() { StartupValues()["user_language"] = Get("user_language"); }

bool PcMenu::PendingRestart() const {
  for (const auto& [name, value] : StartupValues()) {
    // Before the game is launched the language still applies by itself.
    if (name == "user_language" && mode_ == Mode::kStart) continue;
    if (Get(name) != value) return true;
  }
  return false;
}

void PcMenu::SaveConfig() { rex::cvar::SaveConfig(config_path_); }

void PcMenu::Restart() {
  SaveConfig();
  wchar_t exe[MAX_PATH];
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring command_line = GetCommandLineW();
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  if (CreateProcessW(exe, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,
                     &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    REXLOG_INFO("PC menu: restarting to apply settings");
    if (hooks_.quit) hooks_.quit();
  } else {
    REXLOG_ERROR("PC menu: restart failed ({})", GetLastError());
  }
}

void PcMenu::RequestClose() {
  if (mode_ != Mode::kPause || closing_ || IsCapturing()) return;
  closing_ = true;
  close_time_ = time_;
}

void PcMenu::OnClose() {
  pad::SetMenuOpen(false);
  if (hooks_.closed) hooks_.closed();
}

int PcMenu::PageIndex(const std::string& name) const {
  for (size_t i = 0; i < pages_.size(); ++i) {
    if (pages_[i].name == name) return int(i);
  }
  return -1;
}

void PcMenu::OpenPage(const std::string& name) {
  const int index = PageIndex(name);
  if (index < 0 || IsCapturing()) return;
  stack_ = {0};
  page_ = 0;
  page_time_ = 0.0;
  banner_y_ = -1.0f;
  if (index != 0) Enter(index);
}

void PcMenu::Enter(int page) {
  if (pages_[page].achievements) g_achievements = LoadAchievements(runtime_);
  stack_.push_back(page);
  page_ = page;
  page_time_ = 0.0;
  banner_y_ = -1.0f;
  key_slot_ = 0;
  quit_armed_ = false;
}

void PcMenu::Back() {
  quit_armed_ = false;
  if (stack_.size() > 1) {
    stack_.pop_back();
    page_ = stack_.back();
    page_time_ = 0.0;
    banner_y_ = -1.0f;
    return;
  }
  if (mode_ == Mode::kPause) RequestClose();
}

// ---- Pages -------------------------------------------------------------------------

void PcMenu::BuildPages() {
  language_ = Get("user_language");
  g_italian = language_ == "6";

  // Keep the place across a rebuild (language change).
  std::vector<std::pair<std::string, int>> place;
  for (int index : stack_) place.emplace_back(pages_[index].name, pages_[index].selected);

  pages_.clear();
  auto on_off = [] {
    return std::vector<Choice>{{tr("ON", "SÌ"), "true"}, {tr("OFF", "NO"), "false"}};
  };

  // Root.
  {
    Page p{"root", ""};
    if (mode_ == Mode::kStart) {
      Row play{Row::Kind::kAction, tr("PLAY", "GIOCA"),
               tr("Start the game.", "Avvia il gioco.")};
      play.action = [this] {
        if (closing_) return;
        playing_ = true;
        closing_ = true;
        close_time_ = time_;
      };
      p.rows.push_back(std::move(play));
    } else {
      Row resume{Row::Kind::kAction, tr("RESUME", "RIPRENDI"),
                 tr("Back to the game.", "Torna al gioco.")};
      resume.action = [this] { RequestClose(); };
      p.rows.push_back(std::move(resume));
    }
    p.rows.push_back({Row::Kind::kPage, "VIDEO",
                      tr("Resolution, display mode, frame rate and image quality.",
                         "Risoluzione, modalità video, frame rate e qualità dell'immagine.")});
    p.rows.back().page = 1;
    p.rows.push_back({Row::Kind::kPage, tr("CONTROLS", "COMANDI"),
                      tr("Keyboard keys and controller layout.",
                         "Tasti della tastiera e layout del controller.")});
    p.rows.back().page = 2;
    p.rows.push_back({Row::Kind::kPage, tr("GAME", "GIOCO"),
                      tr("Language, start screen and log file.",
                         "Lingua, schermata iniziale e file di log.")});
    p.rows.back().page = 5;
    p.rows.push_back({Row::Kind::kPage, tr("ACHIEVEMENTS", "OBIETTIVI"),
                      tr("The achievements you have unlocked.", "Gli obiettivi che hai sbloccato.")});
    p.rows.back().page = 6;
    Row quit{Row::Kind::kAction, mode_ == Mode::kStart ? tr("QUIT", "ESCI")
                                                       : tr("QUIT GAME", "ESCI DAL GIOCO"),
             mode_ == Mode::kStart
                 ? tr("Close Stuntman: Ignition.", "Chiudi Stuntman: Ignition.")
                 : tr("Close the game. Progress since the last save is lost.",
                      "Chiudi il gioco. I progressi dall'ultimo salvataggio vanno persi.")};
    quit.quit = true;
    p.rows.push_back(std::move(quit));
    pages_.push_back(std::move(p));
  }

  // 1: Video.
  {
    Page p{"video", "VIDEO"};
    Row mode{Row::Kind::kChoice, tr("DISPLAY MODE", "MODALITÀ VIDEO"),
             tr("Fullscreen covers the whole screen (Alt+Enter also switches).",
                "Schermo intero occupa tutto lo schermo (anche Alt+Invio cambia).")};
    mode.cvar = "fullscreen";
    mode.choices = {{tr("FULLSCREEN", "SCHERMO INTERO"), "true"}, {tr("WINDOWED", "FINESTRA"), "false"}};
    p.rows.push_back(std::move(mode));

    Row res{Row::Kind::kChoice, tr("RESOLUTION", "RISOLUZIONE"),
            tr("The size the game renders at. Applies after a restart.",
               "La dimensione a cui il gioco disegna. Si applica al riavvio.")};
    const Size desktop = Desktop();
    for (Size r : Resolutions()) {
      std::string label = std::to_string(r.w) + " x " + std::to_string(r.h) + "  " + AspectName(r);
      if (r == desktop) label += tr("  (NATIVE)", "  (NATIVA)");
      res.choices.push_back({label, std::to_string(r.w) + "x" + std::to_string(r.h)});
    }
    res.get = [] {
      const Size r = CurrentResolution();
      return std::to_string(r.w) + "x" + std::to_string(r.h);
    };
    res.set = [](const std::string& v) {
      const Size r{std::atoi(v.c_str()), std::atoi(v.c_str() + v.find('x') + 1)};
      ApplyResolution(r, CurrentAspect());
    };
    p.rows.push_back(std::move(res));

    Row aspect{Row::Kind::kChoice, tr("WIDE SCREENS", "SCHERMI LARGHI"),
               tr("The game is made for 16:9. Black bars keep its picture as it is; stretch fills "
                  "the screen but widens everything. Applies after a restart.",
                  "Il gioco è fatto per il 16:9. Le bande nere mantengono l'immagine originale; "
                  "allarga riempie lo schermo ma deforma tutto. Si applica al riavvio.")};
    aspect.choices = {{tr("BLACK BARS", "BANDE NERE"), "bars"}, {tr("STRETCH", "ALLARGA"), "stretch"}};
    aspect.get = [] { return CurrentAspect(); };
    aspect.set = [](const std::string& v) { ApplyResolution(CurrentResolution(), v); };
    aspect.enabled = [] { return !Is16x9(CurrentResolution()); };
    p.rows.push_back(std::move(aspect));

    Row fps{Row::Kind::kChoice, "FRAME RATE",
            tr("60 FPS doubles the console's 30. The game runs at normal speed at both.",
               "60 FPS raddoppia i 30 della console. Il gioco va a velocità normale con entrambi.")};
    fps.cvar = "target_fps";
    fps.choices = {{tr("30 FPS (ORIGINAL)", "30 FPS (ORIGINALE)"), "30"}, {"60 FPS", "60"}};
    p.rows.push_back(std::move(fps));

    Row pacing{Row::Kind::kChoice, tr("FRAME PACING", "CADENZA FOTOGRAMMI"),
               tr("Smooth times every frame with a precise PC timer, so one that runs a little "
                  "long doesn't wait for the next refresh. Console waits for the refresh like "
                  "the Xbox 360, which stutters when a frame is late.",
                  "Fluida temporizza ogni fotogramma con un timer preciso del PC, così uno che "
                  "dura un po' di più non aspetta il refresh successivo. Console aspetta il "
                  "refresh come l'Xbox 360 e scatta quando un fotogramma è in ritardo.")};
    pacing.cvar = "frame_limiter";
    pacing.choices = {{tr("SMOOTH", "FLUIDA"), "true"}, {"CONSOLE", "false"}};
    p.rows.push_back(std::move(pacing));

    Row scale{Row::Kind::kChoice, "SUPERSAMPLING",
              tr("Draws the game at 2 or 3 times the resolution, for a sharper picture. Needs a "
                 "faster graphics card: from 1440p up, 3X is too heavy for 60 FPS even on high-end "
                 "cards. Applies after a restart.",
                 "Disegna il gioco a 2 o 3 volte la risoluzione, per un'immagine più nitida. "
                 "Serve una scheda video più potente: da 1440p in su il 3X è troppo pesante per i "
                 "60 FPS anche sulle schede di fascia alta. Si applica al riavvio.")};
    scale.cvar = "resolution_scale";
    scale.choices = {{"OFF", "1"}, {"2X", "2"}, {"3X", "3"}};
    p.rows.push_back(std::move(scale));

    Row aniso{Row::Kind::kChoice, tr("TEXTURE FILTERING", "FILTRO TEXTURE"),
              tr("Keeps roads and ground sharp in the distance.",
                 "Mantiene nitide strade e terreno in lontananza.")};
    aniso.cvar = "anisotropic_override";
    aniso.choices = {{tr("GAME DEFAULT", "COME IL GIOCO"), "-1"}, {"OFF", "0"}, {"2X", "2"},
                     {"4X", "3"}, {"8X", "4"}, {"16X", "5"}};
    p.rows.push_back(std::move(aniso));

    Row filter{Row::Kind::kChoice, tr("SCALING FILTER", "FILTRO DI SCALA"),
               tr("How the picture is scaled to the screen. CAS sharpens, FSR upscales. Applies "
                  "after a restart.",
                  "Come l'immagine viene adattata allo schermo. CAS aumenta la nitidezza, FSR "
                  "ingrandisce. Si applica al riavvio.")};
    filter.cvar = "present_effect";
    filter.choices = {{tr("BILINEAR (ORIGINAL)", "BILINEARE (ORIGINALE)"), "bilinear"}};
    for (const auto& entry : rex::cvar::GetRegistry()) {
      if (entry.name != "present_effect") continue;
      for (const auto& v : entry.constraints.allowed_values) {
        if (v == "cas") filter.choices.push_back({"AMD FIDELITYFX CAS", "cas"});
        if (v == "fsr") filter.choices.push_back({"AMD FIDELITYFX FSR", "fsr"});
      }
    }
    if (filter.choices.size() > 1) p.rows.push_back(std::move(filter));

    if (GetSystemMetrics(SM_CMONITORS) > 1) {
      Row monitor{Row::Kind::kChoice, "MONITOR",
                  tr("The screen the game opens on. Applies after a restart.",
                     "Lo schermo su cui si apre il gioco. Si applica al riavvio.")};
      monitor.cvar = "monitor";
      monitor.choices.push_back({tr("PRIMARY", "PRINCIPALE"), "0"});
      for (int i = 1; i <= GetSystemMetrics(SM_CMONITORS); ++i) {
        monitor.choices.push_back({"MONITOR " + std::to_string(i), std::to_string(i)});
      }
      p.rows.push_back(std::move(monitor));
    }

    Row counter{Row::Kind::kChoice, tr("FPS COUNTER", "CONTATORE FPS"),
                tr("Shows the game's frame rate in a corner. F2 also switches it.",
                   "Mostra il frame rate del gioco in un angolo. Anche F2 lo attiva.")};
    counter.cvar = "show_fps";
    counter.choices = on_off();
    p.rows.push_back(std::move(counter));
    pages_.push_back(std::move(p));
  }

  // 2: Controls.
  {
    Page p{"controls", tr("CONTROLS", "COMANDI")};
    Row keyboard{Row::Kind::kChoice, tr("KEYBOARD", "TASTIERA"),
                 tr("Play with the keyboard. A controller works either way.",
                    "Gioca con la tastiera. Il controller funziona in ogni caso.")};
    keyboard.cvar = "mnk_mode";
    keyboard.choices = on_off();
    p.rows.push_back(std::move(keyboard));
    p.rows.push_back({Row::Kind::kPage, tr("KEYBOARD KEYS", "TASTI DELLA TASTIERA"),
                      tr("Change which key does what. Each action can have two keys.",
                         "Cambia cosa fa ogni tasto. Ogni azione può avere due tasti.")});
    p.rows.back().page = 3;
    p.rows.back().enabled = [] { return Get("mnk_mode") == "true"; };
    p.rows.push_back({Row::Kind::kPage, tr("CONTROLLER LAYOUT", "LAYOUT DEL CONTROLLER"),
                      tr("Swap the controller's buttons. The game's own options also offer "
                         "trigger or button gas.",
                         "Scambia i tasti del controller. Le opzioni del gioco offrono anche "
                         "l'acceleratore sul grilletto o su un tasto.")});
    p.rows.back().page = 4;
    pages_.push_back(std::move(p));
  }

  // 3: Keyboard keys.
  {
    Page p{"keyboard", tr("KEYBOARD KEYS", "TASTI DELLA TASTIERA")};
    for (const auto& a : kKeyActions) {
      Row row{Row::Kind::kKey, tr(a.en, a.it),
              tr("Pick key 1 or 2 with left / right, then confirm and press the new key. "
                 "Delete clears it.",
                 "Scegli il tasto 1 o 2 con sinistra / destra, poi conferma e premi il nuovo "
                 "tasto. Canc lo cancella.")};
      row.cvar = a.cvar;
      p.rows.push_back(std::move(row));
    }
    Row reset{Row::Kind::kAction, tr("RESTORE DEFAULT KEYS", "RIPRISTINA I TASTI PREDEFINITI"),
              tr("W / S gas and brake, A / D steer, arrows for the menus, Esc pauses.",
                 "W / S acceleratore e freno, A / D sterzo, frecce per i menu, Esc mette in pausa.")};
    reset.action = [this] {
      for (const auto& a : kKeyActions) Set(a.cvar, pad::KeyboardDefault(a.cvar));
      SaveConfig();
      flash_ = 1.0f;
    };
    p.rows.push_back(std::move(reset));
    pages_.push_back(std::move(p));
  }

  // 4: Controller layout.
  {
    Page p{"controller", tr("CONTROLLER LAYOUT", "LAYOUT DEL CONTROLLER")};
    for (int b = 0; b < pad::kButtonCount; ++b) {
      Row row{Row::Kind::kPadButton, PadButtonLabel(b),
              tr("Left / right picks what this button does. The menus always use the normal "
                 "layout.",
                 "Sinistra / destra sceglie cosa fa questo tasto. I menu usano sempre il layout "
                 "normale.")};
      row.pad_button = b;
      p.rows.push_back(std::move(row));
    }
    Row reset{Row::Kind::kAction, tr("RESTORE DEFAULT LAYOUT", "RIPRISTINA IL LAYOUT"),
              tr("Every button back to itself.", "Ogni tasto torna a fare se stesso.")};
    reset.action = [this] {
      for (int b = 0; b < pad::kButtonCount; ++b) Set(pad::LayoutCvar(b), pad::LayoutToken(b));
      pad::ReloadLayout();
      SaveConfig();
      flash_ = 1.0f;
    };
    p.rows.push_back(std::move(reset));
    pages_.push_back(std::move(p));
  }

  // 5: Game.
  {
    Page p{"game", tr("GAME", "GIOCO")};
    Row lang{Row::Kind::kChoice, tr("LANGUAGE", "LINGUA"),
             mode_ == Mode::kStart
                 ? tr("Text and speech. The disc has these five.",
                      "Testi e parlato. Il disco contiene queste cinque.")
                 : tr("Text and speech. Applies after a restart.",
                      "Testi e parlato. Si applica al riavvio.")};
    lang.cvar = "user_language";
    lang.choices = {{"ENGLISH", "1"}, {"FRANÇAIS", "4"}, {"DEUTSCH", "3"}, {"ESPAÑOL", "5"},
                    {"ITALIANO", "6"}};
    p.rows.push_back(std::move(lang));

    Row start{Row::Kind::kChoice, tr("START SCREEN", "SCHERMATA INIZIALE"),
              tr("This menu when the game starts. Off starts the game directly.",
                 "Questo menu all'avvio. Se spento il gioco parte subito.")};
    start.cvar = "start_screen";
    start.choices = on_off();
    p.rows.push_back(std::move(start));

    Row log{Row::Kind::kChoice, tr("LOG FILE", "FILE DI LOG"),
            tr("Writes stuntman.log next to the game, for bug reports. Applies after a restart.",
               "Scrive stuntman.log accanto al gioco, per segnalare problemi. Si applica al "
               "riavvio.")};
    log.choices = on_off();
    log.get = [] { return Get("log_file").empty() ? std::string("false") : std::string("true"); };
    log.set = [](const std::string& v) { Set("log_file", v == "true" ? "stuntman.log" : ""); };
    p.rows.push_back(std::move(log));
    pages_.push_back(std::move(p));
  }

  // 6: Achievements.
  {
    Page p{"achievements", tr("ACHIEVEMENTS", "OBIETTIVI")};
    p.achievements = true;
    pages_.push_back(std::move(p));
  }

  // Restore the place.
  stack_.clear();
  for (const auto& [name, selected] : place) {
    const int index = PageIndex(name);
    if (index < 0) continue;
    stack_.push_back(index);
    pages_[index].selected = std::min(selected, std::max(0, int(pages_[index].rows.size()) - 1));
  }
  if (stack_.empty()) stack_.push_back(0);
  page_ = stack_.back();
}

// ---- Values ----------------------------------------------------------------------

bool PcMenu::RowEnabled(const Row& row) const { return !row.enabled || row.enabled(); }

std::string PcMenu::RowValue(const Row& row) const {
  switch (row.kind) {
    case Row::Kind::kChoice: {
      const std::string value = row.get ? row.get() : Get(row.cvar);
      for (const auto& c : row.choices) {
        if (c.value == value) return c.label;
      }
      return value;
    }
    case Row::Kind::kPadButton:
      return PadButtonLabel(pad::LayoutTarget(row.pad_button));
    default:
      return "";
  }
}

void PcMenu::Change(Row& row, int direction) {
  if (!RowEnabled(row)) return;
  if (row.kind == Row::Kind::kChoice) {
    const std::string value = row.get ? row.get() : Get(row.cvar);
    const int n = int(row.choices.size());
    int index = -1;
    for (int i = 0; i < n; ++i) {
      if (row.choices[i].value == value) index = i;
    }
    index = index < 0 ? 0 : (index + direction + n) % n;
    if (row.set) {
      row.set(row.choices[index].value);
    } else {
      Set(row.cvar, row.choices[index].value);
    }
  } else if (row.kind == Row::Kind::kPadButton) {
    const int n = pad::kButtonCount + 1;  // every button, and nothing
    const int target = (pad::LayoutTarget(row.pad_button) + direction + n) % n;
    Set(pad::LayoutCvar(row.pad_button), pad::LayoutToken(target));
    pad::ReloadLayout();
  } else {
    return;
  }
  SaveConfig();
  flash_ = 1.0f;
}

void PcMenu::Activate(Row& row, int direction) {
  if (!RowEnabled(row)) return;
  switch (row.kind) {
    case Row::Kind::kAction:
      if (row.quit) {
        if (mode_ == Mode::kPause && !quit_armed_) {
          quit_armed_ = true;
          return;
        }
        SaveConfig();
        if (hooks_.quit) hooks_.quit();
        return;
      }
      if (row.action) row.action();
      break;
    case Row::Kind::kPage:
      Enter(row.page);
      break;
    case Row::Kind::kKey:
      capture_slot_ = key_slot_;
      capture_frames_ = 0;
      break;
    default:
      Change(row, direction);
      break;
  }
}

void PcMenu::Capture() {
  // Ignore the frame of the press that started the capture.
  if (++capture_frames_ < 2) return;
  Page& page = pages_[page_];
  Row& row = page.rows[page.selected];
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    capture_slot_ = -1;
    return;
  }
  for (const auto& k : kKeyNames) {
    if (k.key == ImGuiKey_Escape || !ImGui::IsKeyPressed(k.key, false)) continue;
    auto keys = SplitKeys(Get(row.cvar));
    keys.resize(2);
    // The same key twice in one action is pointless: move it.
    for (auto& existing : keys) {
      if (existing == k.name) existing.clear();
    }
    keys[capture_slot_] = k.name;
    std::string value;
    for (const auto& key : keys) {
      if (key.empty()) continue;
      if (!value.empty()) value += ",";
      value += key;
    }
    Set(row.cvar, value);
    SaveConfig();
    flash_ = 1.0f;
    capture_slot_ = -1;
    return;
  }
}

// ---- Input -------------------------------------------------------------------------

PcMenu::Nav PcMenu::ReadNav(ImGuiIO& io) {
  Nav nav;
  auto key = [](ImGuiKey k, bool repeat = true) { return ImGui::IsKeyPressed(k, repeat); };
  nav.up = key(ImGuiKey_UpArrow);
  nav.down = key(ImGuiKey_DownArrow);
  nav.left = key(ImGuiKey_LeftArrow);
  nav.right = key(ImGuiKey_RightArrow);
  nav.confirm = key(ImGuiKey_Enter, false) || key(ImGuiKey_KeypadEnter, false) ||
                key(ImGuiKey_Space, false);
  nav.back = key(ImGuiKey_Escape, false) || key(ImGuiKey_Backspace, false);
  nav.clear = key(ImGuiKey_Delete, false);
  nav.restart = key(ImGuiKey_R, false);
  if (nav.up || nav.down || nav.left || nav.right || nav.confirm || nav.back || nav.clear) {
    pad_prompts_ = false;
  }

  // Controller: edges, and auto-repeat on the directions.
  const uint32_t buttons = pad::MenuButtons();
  const uint32_t pressed = buttons & ~pad_held_;
  constexpr uint32_t kDirs = (1u << pad::kUp) | (1u << pad::kDown) | (1u << pad::kLeft) |
                             (1u << pad::kRight);
  uint32_t fire = pressed;
  if (pressed & kDirs) {
    repeat_at_ = time_ + 0.38;
  } else if ((buttons & kDirs) && time_ >= repeat_at_) {
    fire |= buttons & kDirs;
    repeat_at_ = time_ + 0.085;
  }
  pad_held_ = buttons;
  if (fire) pad_prompts_ = true;
  auto bit = [fire](int b) { return (fire & (1u << b)) != 0; };
  nav.up |= bit(pad::kUp);
  nav.down |= bit(pad::kDown);
  nav.left |= bit(pad::kLeft) || bit(pad::kLB);
  nav.right |= bit(pad::kRight) || bit(pad::kRB);
  nav.confirm |= bit(pad::kA) || (bit(pad::kStart) && mode_ == Mode::kStart);
  nav.back |= bit(pad::kB);
  nav.clear |= bit(pad::kX);
  nav.restart |= bit(pad::kY);
  if (bit(pad::kStart) && mode_ == Mode::kPause && !IsCapturing()) RequestClose();

  // Mouse: hover selects, click confirms, right click goes back, the wheel scrolls.
  // The cursor where the menu opened doesn't count as a move.
  if (last_mouse_.x < 0.0f) last_mouse_ = io.MousePos;
  mouse_moved_ = io.MousePos.x != last_mouse_.x || io.MousePos.y != last_mouse_.y;
  last_mouse_ = io.MousePos;
  if (IsCapturing()) return nav;
  const ImVec2 mouse = RotatePoint(io.MousePos, hit_center_, -hit_angle_);
  const HitRow* hit = nullptr;
  for (const auto& h : hits_) {
    if (Inside(mouse, h.min, h.max)) hit = &h;
  }
  Page& page = pages_[page_];
  if (hit && mouse_moved_ && page.selected != hit->row) {
    page.selected = hit->row;
    quit_armed_ = false;
  }
  if (mouse_moved_) pad_prompts_ = false;
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hit) {
    page.selected = hit->row;
    if (!page.rows.empty()) {
      Row& row = page.rows[hit->row];
      const float s = theme::Scale(io.DisplaySize);
      const float right_x = io.DisplaySize.x - 110.0f * s;
      int direction = 1;
      if (row.kind == Row::Kind::kKey) {
        // Key 2 is the rightmost cap.
        key_slot_ = mouse.x > right_x - 170.0f * s ? 1 : 0;
      } else if (row.kind == Row::Kind::kChoice || row.kind == Row::Kind::kPadButton) {
        // Clicking the left part of the value goes back through the choices.
        const float value_w = theme::Measure(theme::MenuFont(), 36.0f * s, RowValue(row)).x;
        const float value_x = right_x - 34.0f * s - value_w;
        if (mouse.x > value_x - 50.0f * s && mouse.x < value_x + value_w * 0.5f) direction = -1;
      }
      Activate(row, direction);
    }
  }
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) nav.back = true;
  if (io.MouseWheel > 0.0f) nav.up = true;
  if (io.MouseWheel < 0.0f) nav.down = true;
  return nav;
}

void PcMenu::Handle(const Nav& nav) {
  Page& page = pages_[page_];
  const int count = page.achievements ? int(g_achievements.size()) : int(page.rows.size());
  if (count > 0 && (nav.up || nav.down)) {
    const int step = nav.up ? -1 : 1;
    int next = page.selected;
    for (int tries = 0; tries < count; ++tries) {
      next = (next + step + count) % count;
      if (page.achievements || RowEnabled(page.rows[next])) break;
    }
    page.selected = next;
    quit_armed_ = false;
  }
  if (nav.back) {
    Back();
    return;
  }
  if (nav.restart && PendingRestart()) {
    Restart();
    return;
  }
  if (page.achievements || page.rows.empty()) return;
  page.selected = std::clamp(page.selected, 0, int(page.rows.size()) - 1);
  Row& row = page.rows[page.selected];
  if (nav.left || nav.right) {
    if (row.kind == Row::Kind::kKey) {
      key_slot_ = nav.left ? 0 : 1;
    } else if (row.kind == Row::Kind::kChoice || row.kind == Row::Kind::kPadButton) {
      Change(row, nav.left ? -1 : 1);
    }
  }
  if (nav.confirm) Activate(row, 1);
  if (nav.clear) {
    if (row.kind == Row::Kind::kKey) {
      auto keys = SplitKeys(Get(row.cvar));
      if (key_slot_ < int(keys.size())) keys.erase(keys.begin() + key_slot_);
      std::string value;
      for (const auto& k : keys) value += (value.empty() ? "" : ",") + k;
      Set(row.cvar, value);
      SaveConfig();
      flash_ = 1.0f;
    } else if (row.kind == Row::Kind::kPadButton) {
      Set(pad::LayoutCvar(row.pad_button), pad::LayoutToken(row.pad_button));
      pad::ReloadLayout();
      SaveConfig();
      flash_ = 1.0f;
    }
  }
}

// ---- Frame -------------------------------------------------------------------------

void PcMenu::OnDraw(ImGuiIO& io) {
  const float dt = std::min(io.DeltaTime, 0.1f);
  time_ += dt;
  page_time_ += dt;
  flash_ = std::max(0.0f, flash_ - dt * 3.0f);
  if (Get("user_language") != language_) BuildPages();
  if (mode_ == Mode::kStart && !closing_ && hooks_.poll_pads) hooks_.poll_pads();

  float fade = Ease(float(time_ / 0.3));
  if (closing_) {
    const float t = float((time_ - close_time_) / (playing_ ? 0.45 : 0.22));
    fade = std::min(fade, 1.0f - std::clamp(t, 0.0f, 1.0f));
    if (t >= 1.0f) {
      // Close deletes the dialog: take what is needed first.
      auto play = playing_ ? hooks_.play : nullptr;
      Close();
      if (play) play();
      return;
    }
  }

  const ImVec2 display = io.DisplaySize;
  const float s = theme::Scale(display);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(display);
  ImGui::Begin("##stuntman_pc_menu", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
  ImDrawList* dl = ImGui::GetWindowDrawList();

  if (!closing_) {
    if (IsCapturing()) {
      ReadNav(io);
      Capture();
    } else {
      Handle(ReadNav(io));
    }
  }

  hits_.clear();
  hit_angle_ = 0.0f;
  DrawBackground(dl, display, s, fade);
  const Page& page = pages_[page_];
  if (page_ == 0) {
    DrawRoot(dl, io, s, fade);
  } else if (page.achievements) {
    DrawAchievements(dl, io, s, fade);
  } else {
    DrawList(dl, io, s, fade);
  }
  DrawPrompts(dl, display, s, fade);
  ImGui::End();
}

// ---- Drawing -------------------------------------------------------------------------

void PcMenu::DrawBackground(ImDrawList* dl, ImVec2 display, float s, float fade) {
  if (mode_ == Mode::kStart) {
    // A warm, dirty backdrop with film strips running across it.
    const float a = fade;
    dl->AddRectFilledMultiColor(ImVec2(0, 0), display, Alpha(IM_COL32(52, 42, 30, 255), a),
                                Alpha(IM_COL32(34, 27, 20, 255), a),
                                Alpha(IM_COL32(10, 8, 7, 255), a),
                                Alpha(IM_COL32(16, 13, 10, 255), a));
    for (int i = 0; i < 6; ++i) {
      dl->AddCircleFilled(ImVec2(display.x * 0.28f, display.y * 0.42f), (520.0f - i * 70.0f) * s,
                          Alpha(IM_COL32(150, 120, 80, 255), a * 0.035f), 48);
    }
    theme::Star(dl, ImVec2(display.x * 0.8f, display.y * 0.55f), 520.0f * s, 0.25f,
                Alpha(IM_COL32(255, 184, 0, 255), a * 0.035f), Alpha(theme::kYellow, a * 0.06f),
                3.0f * s);
    theme::FilmStrip(dl, ImVec2(-200.0f * s, 120.0f * s), display.x + 600.0f * s, 250.0f * s,
                     -0.06f, float(time_) * 26.0f * s, a * 0.9f);
    theme::FilmStrip(dl, ImVec2(display.x * 0.30f, display.y - 300.0f * s), display.x, 210.0f * s,
                     0.04f, -float(time_) * 18.0f * s, a * 0.75f);
    theme::Grime(dl, ImVec2(0, 0), display, s, 7, a * 0.7f);
    // Vignette.
    const ImU32 clear = Alpha(theme::kInk, 0.0f), dark = Alpha(theme::kInk, a * 0.8f);
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(display.x * 0.18f, display.y), dark, clear,
                                clear, dark);
    dl->AddRectFilledMultiColor(ImVec2(display.x * 0.82f, 0), display, clear, dark, dark, clear);
  } else {
    // Over the game: darken it, more on the left where the text is.
    dl->AddRectFilled(ImVec2(0, 0), display, Alpha(theme::kInk, fade * 0.55f));
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(display.x * 0.6f, display.y),
                                Alpha(theme::kInk, fade * 0.6f), Alpha(theme::kInk, 0.0f),
                                Alpha(theme::kInk, 0.0f), Alpha(theme::kInk, fade * 0.6f));
    theme::FilmStrip(dl, ImVec2(-200.0f * s, 96.0f * s), display.x + 600.0f * s, 150.0f * s,
                     -0.035f, float(time_) * 20.0f * s, fade * 0.45f);
  }
  theme::Letterbox(dl, display, s, fade);
}

void PcMenu::DrawRoot(ImDrawList* dl, ImGuiIO& io, float s, float fade) {
  const ImVec2 display = io.DisplaySize;
  ImFont* font = theme::MenuFont();
  Page& page = pages_[0];

  // Title on the left.
  if (mode_ == Mode::kStart) {
    const ImVec2 logo_pos(130.0f * s, 250.0f * s);
    const ImVec2 size = theme::Logo(dl, logo_pos, s, fade);
    theme::Text(dl, font, 30.0f * s, ImVec2(logo_pos.x + 10.0f * s, logo_pos.y + size.y + 24.0f * s),
                Alpha(theme::kDim, fade), tr("RECOMPILED FOR PC", "RICOMPILATO PER PC"), 0.15f);
  } else {
    theme::ShadowText(dl, font, 120.0f * s, ImVec2(130.0f * s, 230.0f * s),
                      Alpha(theme::kWhite, fade), tr("PAUSE", "PAUSA"), 0.2f, 6.0f * s);
    theme::Text(dl, font, 34.0f * s, ImVec2(140.0f * s, 370.0f * s), Alpha(theme::kYellow, fade),
                tr("PC MENU", "MENU PC"), 0.15f);
  }

  // The list: rows stepping left as they go down, tilted like the game's.
  const int n = int(page.rows.size());
  const float row_h = 78.0f * s;
  const float top = std::max(300.0f * s, display.y * 0.5f - n * row_h * 0.5f + 20.0f * s);
  const float base_x = display.x - 760.0f * s;
  hit_angle_ = -0.06f;
  hit_center_ = ImVec2(display.x - 380.0f * s, top + n * row_h * 0.5f);
  const int vtx_start = dl->VtxBuffer.Size;

  const float target_y = top + page.selected * row_h;
  banner_y_ = banner_y_ < 0 ? target_y : banner_y_ + (target_y - banner_y_) * std::min(1.0f, io.DeltaTime * 18.0f);

  auto row_x = [&](int i) {
    const float slide = 1.0f - Ease(float((page_time_ - i * 0.045) / 0.32));
    return base_x - i * 16.0f * s + slide * 800.0f * s;
  };
  for (int i = 0; i < n; ++i) {
    const Row& row = page.rows[i];
    const float x = row_x(i), y = top + i * row_h;
    hits_.push_back({ImVec2(x - 40.0f * s, y), ImVec2(display.x, y + row_h), i});
    if (i == page.selected) continue;
    theme::Strip(dl, ImVec2(x, y + 6.0f * s), ImVec2(display.x + 20.0f, y + row_h - 8.0f * s),
                 Alpha(theme::kStrip, fade * 0.85f), 16.0f * s);
    const float size = 54.0f * s;
    theme::ShadowText(dl, font, size, ImVec2(x + 30.0f * s, y + (row_h - size) * 0.42f),
                      Alpha(row.quit ? theme::kDim : theme::kWhite, fade), row.label, 0.14f,
                      3.0f * s);
  }
  // The selected row on top: the yellow banner.
  if (n > 0) {
    const Row& row = page.rows[page.selected];
    const float x = row_x(page.selected);
    const float y = banner_y_;
    theme::RaggedBanner(dl, ImVec2(x - 70.0f * s, y - 8.0f * s),
                        ImVec2(display.x + 60.0f * s, y + row_h + 4.0f * s),
                        Alpha(theme::kYellow, fade), 11 + page.selected);
    const float size = 66.0f * s;
    const std::string label = row.quit && quit_armed_
                                  ? std::string(tr("PRESS AGAIN TO QUIT", "PREMI ANCORA PER USCIRE"))
                                  : row.label;
    theme::Text(dl, font, size, ImVec2(x + 4.0f * s, y + (row_h - size) * 0.4f),
                Alpha(theme::kRed, fade), label, 0.14f);
  }
  theme::Rotate(dl, vtx_start, hit_center_, hit_angle_);

  // The selected entry's help under the title.
  if (n > 0 && !page.rows[page.selected].help.empty()) {
    theme::WrappedText(dl, font, 30.0f * s, ImVec2(140.0f * s, display.y - 250.0f * s),
                       Alpha(theme::kDim, fade), page.rows[page.selected].help,
                       std::min(640.0f * s, display.x * 0.35f));
  }
}

void PcMenu::DrawKeys(ImDrawList* dl, const Row& row, bool selected, ImVec2 right, float h,
                      float s, float alpha) {
  auto keys = SplitKeys(Get(row.cvar));
  keys.resize(2);
  float x = right.x;
  for (int slot = 1; slot >= 0; --slot) {
    const bool capturing = selected && capture_slot_ == slot;
    std::string label;
    if (capturing) {
      label = std::fmod(time_, 0.8) < 0.5 ? tr("PRESS A KEY", "PREMI UN TASTO") : "";
    } else {
      label = keys[slot].empty() ? "-" : KeyLabel(keys[slot]);
    }
    ImFont* font = theme::MenuFont();
    const float cap_h = h * 0.62f;
    const float w = std::max(150.0f * s, theme::Measure(font, cap_h * 0.6f, label).x + cap_h * 0.5f);
    const ImVec2 pos(x - w, right.y - cap_h * 0.5f);
    if (capturing) {
      dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + cap_h), Alpha(theme::kRed, alpha),
                        cap_h * 0.18f);
      const ImVec2 t = theme::Measure(font, cap_h * 0.55f, label);
      theme::Text(dl, font, cap_h * 0.55f, ImVec2(pos.x + (w - t.x) * 0.5f, pos.y + cap_h * 0.15f),
                  Alpha(theme::kWhite, alpha), label);
    } else if (keys[slot].empty()) {
      dl->AddRect(pos, ImVec2(pos.x + w, pos.y + cap_h),
                  Alpha(selected ? theme::kRed : theme::kDim, alpha * 0.6f), cap_h * 0.18f, 0,
                  2.0f * s);
      const ImVec2 t = theme::Measure(font, cap_h * 0.55f, label);
      theme::Text(dl, font, cap_h * 0.55f, ImVec2(pos.x + (w - t.x) * 0.5f, pos.y + cap_h * 0.15f),
                  Alpha(selected ? theme::kRed : theme::kDim, alpha), label);
    } else {
      // Key caps are drawn at their natural width, centred in the slot.
      const float natural = std::max(cap_h, theme::Measure(font, cap_h * 0.6f, label).x + cap_h * 0.5f);
      theme::KeyCap(dl, ImVec2(pos.x + (w - natural) * 0.5f, pos.y), cap_h, label, alpha);
    }
    if (selected && key_slot_ == slot && !capturing) {
      dl->AddRect(ImVec2(pos.x - 5.0f * s, pos.y - 5.0f * s),
                  ImVec2(pos.x + w + 5.0f * s, pos.y + cap_h + 5.0f * s), Alpha(theme::kRed, alpha),
                  cap_h * 0.22f, 0, 4.0f * s);
    }
    x -= w + 22.0f * s;
  }
}

void PcMenu::DrawList(ImDrawList* dl, ImGuiIO& io, float s, float fade) {
  const ImVec2 display = io.DisplaySize;
  ImFont* font = theme::MenuFont();
  Page& page = pages_[page_];

  // Title and help on the left.
  const float left_w = std::min(600.0f * s, display.x * 0.32f);
  const float title_size = TitleSize(font, page.title, left_w, s);
  const ImVec2 title_pos(120.0f * s, 120.0f * s + (104.0f * s - title_size) * 0.8f);
  const ImVec2 title = theme::Measure(font, title_size, page.title);
  theme::RaggedBanner(dl, ImVec2(title_pos.x - 30.0f * s, title_pos.y + title_size * 1.02f),
                      ImVec2(title_pos.x + std::min(title.x + 60.0f * s, left_w + 40.0f * s),
                             title_pos.y + title_size * 1.02f + 14.0f * s),
                      Alpha(theme::kYellow, fade), 3);
  theme::ShadowText(dl, font, title_size, title_pos, Alpha(theme::kWhite, fade), page.title, 0.18f,
                    6.0f * s);

  if (!page.rows.empty()) {
    const Row& row = page.rows[page.selected];
    float y = 320.0f * s;
    y += theme::WrappedText(dl, font, 32.0f * s, ImVec2(130.0f * s, y), Alpha(theme::kWhite, fade * 0.9f),
                            row.help, left_w);
    if (!RowEnabled(row) && row.kind == Row::Kind::kChoice && row.cvar.empty() && row.get) {
      theme::WrappedText(dl, font, 28.0f * s, ImVec2(130.0f * s, y + 16.0f * s),
                         Alpha(theme::kDim, fade),
                         tr("Only for screens that aren't 16:9.", "Solo per schermi non 16:9."),
                         left_w);
    }
  }

  // The rows.
  const float list_x = std::max(display.x * 0.40f, display.x - 1180.0f * s);
  const float right_x = display.x - 110.0f * s;
  const float row_h = 66.0f * s;
  const float top = 170.0f * s;
  const float bottom = display.y - 200.0f * s;
  const int visible = std::max(1, int((bottom - top) / row_h));
  const int n = int(page.rows.size());
  float target = page.scroll;
  if (page.selected < target) target = float(page.selected);
  if (page.selected >= target + visible) target = float(page.selected - visible + 1);
  target = std::clamp(target, 0.0f, float(std::max(0, n - visible)));
  page.scroll += (target - page.scroll) * std::min(1.0f, io.DeltaTime * 14.0f);
  if (std::abs(target - page.scroll) < 0.01f) page.scroll = target;

  const float target_y = top + (page.selected - page.scroll) * row_h;
  banner_y_ = banner_y_ < 0 ? target_y
                            : banner_y_ + (target_y - banner_y_) * std::min(1.0f, io.DeltaTime * 20.0f);

  dl->PushClipRect(ImVec2(0, top - 14.0f * s), ImVec2(display.x, bottom + 14.0f * s), true);
  const float label_size = 40.0f * s;
  auto draw_row = [&](int i, bool selected) {
    const Row& row = page.rows[i];
    const float slide = 1.0f - Ease(float((page_time_ - (i - page.scroll) * 0.03) / 0.28));
    const float x = list_x + slide * 700.0f * s;
    const float y = selected ? banner_y_ : top + (i - page.scroll) * row_h;
    const bool enabled = RowEnabled(row);
    const float a = fade * (enabled ? 1.0f : 0.4f);
    if (selected) {
      theme::RaggedBanner(dl, ImVec2(x - 50.0f * s, y - 6.0f * s),
                          ImVec2(display.x + 60.0f * s, y + row_h + 2.0f * s),
                          Alpha(theme::kYellow, fade), 31 + i);
      if (flash_ > 0.0f) {
        dl->AddRectFilled(ImVec2(x - 30.0f * s, y), ImVec2(display.x, y + row_h - 4.0f * s),
                          Alpha(IM_COL32(255, 255, 255, 255), flash_ * 0.25f));
      }
    } else {
      theme::Strip(dl, ImVec2(x, y + 5.0f * s), ImVec2(display.x + 20.0f, y + row_h - 5.0f * s),
                   Alpha(theme::kStrip, fade * 0.8f), 12.0f * s);
    }
    const ImU32 text = selected ? theme::kRed : theme::kWhite;
    const float ty = y + (row_h - label_size) * 0.38f;
    if (selected) {
      theme::Text(dl, font, label_size, ImVec2(x + 8.0f * s, ty), Alpha(text, a), row.label, 0.12f);
    } else {
      theme::ShadowText(dl, font, label_size, ImVec2(x + 30.0f * s, ty), Alpha(text, a), row.label,
                        0.12f, 2.5f * s);
    }

    const float mid = y + row_h * 0.5f;
    if (row.kind == Row::Kind::kKey) {
      DrawKeys(dl, row, selected, ImVec2(right_x, mid), row_h, s, a);
    } else if (row.kind == Row::Kind::kPage) {
      theme::Triangle(dl, ImVec2(right_x - 10.0f * s, mid), 22.0f * s, true,
                      Alpha(selected ? theme::kRed : theme::kYellow, a));
    } else if (row.kind == Row::Kind::kChoice || row.kind == Row::Kind::kPadButton) {
      const std::string value = RowValue(row);
      const float vs = 36.0f * s;
      const ImVec2 v = theme::Measure(font, vs, value);
      const float vx = right_x - 34.0f * s - v.x;
      ImU32 value_color = selected ? theme::kRed : theme::kYellow;
      if (!selected && row.kind == Row::Kind::kPadButton) {
        value_color = pad::LayoutTarget(row.pad_button) == row.pad_button ? theme::kDim
                                                                          : theme::kYellow;
      }
      theme::Text(dl, font, vs, ImVec2(vx, y + (row_h - vs) * 0.4f), Alpha(value_color, a), value,
                  0.1f);
      if (selected && enabled) {
        theme::Triangle(dl, ImVec2(vx - 26.0f * s, mid), 20.0f * s, false, Alpha(theme::kRed, a));
        theme::Triangle(dl, ImVec2(right_x - 8.0f * s, mid), 20.0f * s, true, Alpha(theme::kRed, a));
      }
    }
    if (y + row_h > top - 1.0f && y < bottom + 1.0f) {
      hits_.push_back({ImVec2(x - 30.0f * s, std::max(y, top)),
                       ImVec2(display.x, std::min(y + row_h, bottom)), i});
    }
  };
  for (int i = 0; i < n; ++i) {
    const float y = top + (i - page.scroll) * row_h;
    if (i == page.selected || y + row_h < top - row_h || y > bottom + row_h) continue;
    draw_row(i, false);
  }
  if (n > 0) draw_row(page.selected, true);
  dl->PopClipRect();

  // More rows above or below.
  if (page.scroll > 0.05f) {
    dl->AddTriangleFilled(ImVec2(display.x - 160.0f * s, top - 30.0f * s),
                          ImVec2(display.x - 140.0f * s, top - 30.0f * s),
                          ImVec2(display.x - 150.0f * s, top - 44.0f * s),
                          Alpha(theme::kYellow, fade));
  }
  if (page.scroll + visible < n - 0.05f) {
    dl->AddTriangleFilled(ImVec2(display.x - 160.0f * s, bottom + 30.0f * s),
                          ImVec2(display.x - 140.0f * s, bottom + 30.0f * s),
                          ImVec2(display.x - 150.0f * s, bottom + 44.0f * s),
                          Alpha(theme::kYellow, fade));
  }
}

void PcMenu::DrawAchievements(ImDrawList* dl, ImGuiIO& io, float s, float fade) {
  const ImVec2 display = io.DisplaySize;
  ImFont* font = theme::MenuFont();
  ImFont* stencil = theme::StencilFont();
  Page& page = pages_[page_];
  const auto& list = g_achievements;
  const int n = int(list.size());
  page.selected = std::clamp(page.selected, 0, std::max(0, n - 1));

  // Title, totals and the selected achievement on the left.
  const float left_w = std::min(600.0f * s, display.x * 0.32f);
  const float title_size = TitleSize(font, page.title, left_w, s);
  const ImVec2 title_pos(120.0f * s, 120.0f * s + (104.0f * s - title_size) * 0.8f);
  const ImVec2 title = theme::Measure(font, title_size, page.title);
  theme::RaggedBanner(dl, ImVec2(title_pos.x - 30.0f * s, title_pos.y + title_size * 1.02f),
                      ImVec2(title_pos.x + std::min(title.x + 60.0f * s, left_w + 40.0f * s),
                             title_pos.y + title_size * 1.02f + 14.0f * s),
                      Alpha(theme::kYellow, fade), 3);
  theme::ShadowText(dl, font, title_size, title_pos, Alpha(theme::kWhite, fade), page.title, 0.18f,
                    6.0f * s);
  if (n == 0) {
    theme::WrappedText(dl, font, 34.0f * s, ImVec2(130.0f * s, 320.0f * s), Alpha(theme::kDim, fade),
                       tr("No achievements found.", "Nessun obiettivo trovato."), left_w);
    return;
  }

  int unlocked = 0, score = 0, total = 0;
  for (const auto& a : list) {
    total += int(a.info.gamerscore);
    if (a.unlocked) {
      ++unlocked;
      score += int(a.info.gamerscore);
    }
  }
  float y = 310.0f * s;
  const std::string count = std::to_string(unlocked) + " / " + std::to_string(n);
  theme::ShadowText(dl, stencil, 92.0f * s, ImVec2(130.0f * s, y), Alpha(theme::kWhite, fade), count,
                    0.0f, 4.0f * s);
  y += 100.0f * s;
  theme::Text(dl, font, 40.0f * s, ImVec2(134.0f * s, y), Alpha(theme::kYellow, fade),
              std::to_string(score) + " / " + std::to_string(total) + " G", 0.1f);
  y += 62.0f * s;
  dl->AddRectFilled(ImVec2(134.0f * s, y), ImVec2(134.0f * s + left_w, y + 12.0f * s),
                    Alpha(IM_COL32(60, 54, 46, 255), fade));
  dl->AddRectFilled(ImVec2(134.0f * s, y),
                    ImVec2(134.0f * s + left_w * float(unlocked) / float(n), y + 12.0f * s),
                    Alpha(theme::kYellow, fade));
  y += 52.0f * s;
  const auto& sel = list[page.selected];
  y += theme::WrappedText(dl, font, 38.0f * s, ImVec2(134.0f * s, y), Alpha(theme::kWhite, fade),
                          sel.info.label, left_w) + 10.0f * s;
  const std::string& desc = sel.unlocked || sel.info.unachieved_description.empty()
                                ? sel.info.description
                                : sel.info.unachieved_description;
  y += theme::WrappedText(dl, font, 30.0f * s, ImVec2(134.0f * s, y), Alpha(theme::kDim, fade), desc,
                          left_w) + 14.0f * s;
  theme::Text(dl, font, 28.0f * s, ImVec2(134.0f * s, y),
              Alpha(sel.unlocked ? theme::kYellow : theme::kDim, fade),
              sel.unlocked ? std::string(tr("UNLOCKED ", "SBLOCCATO IL ")) + FormatFiletime(sel.unlocked)
                           : std::string(tr("LOCKED", "BLOCCATO")));

  // The list.
  if (!icons_ && immediate_drawer_ && runtime_) {
    icons_ = std::make_unique<rex::ui::AchievementIconCache>(immediate_drawer_, runtime_);
  }
  const float list_x = std::max(display.x * 0.40f, display.x - 1180.0f * s);
  const float right_x = display.x - 110.0f * s;
  const float row_h = 108.0f * s;
  const float top = 170.0f * s;
  const float bottom = display.y - 200.0f * s;
  const int visible = std::max(1, int((bottom - top) / row_h));
  float target = page.scroll;
  if (page.selected < target) target = float(page.selected);
  if (page.selected >= target + visible) target = float(page.selected - visible + 1);
  target = std::clamp(target, 0.0f, float(std::max(0, n - visible)));
  page.scroll += (target - page.scroll) * std::min(1.0f, io.DeltaTime * 14.0f);
  if (std::abs(target - page.scroll) < 0.01f) page.scroll = target;

  dl->PushClipRect(ImVec2(0, top - 10.0f * s), ImVec2(display.x, bottom + 10.0f * s), true);
  for (int i = 0; i < n; ++i) {
    const float ry = top + (i - page.scroll) * row_h;
    if (ry + row_h < top - row_h || ry > bottom + row_h) continue;
    const auto& a = list[i];
    const bool selected = i == page.selected;
    const float slide = 1.0f - Ease(float((page_time_ - (i - page.scroll) * 0.03) / 0.28));
    const float x = list_x + slide * 700.0f * s;
    if (selected) {
      theme::RaggedBanner(dl, ImVec2(x - 50.0f * s, ry - 4.0f * s),
                          ImVec2(display.x + 60.0f * s, ry + row_h), Alpha(theme::kYellow, fade),
                          51 + i);
    } else {
      theme::Strip(dl, ImVec2(x, ry + 6.0f * s), ImVec2(display.x + 20.0f, ry + row_h - 6.0f * s),
                   Alpha(theme::kStrip, fade * 0.8f), 12.0f * s);
    }
    const float icon = 80.0f * s;
    const ImVec2 icon_min(x + 26.0f * s, ry + (row_h - icon) * 0.5f);
    rex::ui::ImmediateTexture* texture = icons_ ? icons_->GetIcon(a.info) : nullptr;
    if (texture) {
      dl->AddImage(reinterpret_cast<ImTextureID>(texture), icon_min,
                   ImVec2(icon_min.x + icon, icon_min.y + icon), ImVec2(0, 0), ImVec2(1, 1),
                   a.unlocked ? Alpha(IM_COL32_WHITE, fade)
                              : Alpha(IM_COL32(90, 86, 80, 255), fade * 0.8f));
    } else {
      dl->AddRectFilled(icon_min, ImVec2(icon_min.x + icon, icon_min.y + icon),
                        Alpha(IM_COL32(50, 46, 40, 255), fade));
    }
    const ImU32 name_color = selected ? theme::kRed : (a.unlocked ? theme::kWhite : theme::kDim);
    const float tx = icon_min.x + icon + 24.0f * s;
    const float g_w = 140.0f * s;
    dl->PushClipRect(ImVec2(tx, ry), ImVec2(right_x - g_w, ry + row_h), true);
    theme::Text(dl, font, 38.0f * s, ImVec2(tx, ry + 14.0f * s), Alpha(name_color, fade), a.info.label);
    const std::string& line = a.unlocked || a.info.unachieved_description.empty()
                                  ? a.info.description
                                  : a.info.unachieved_description;
    theme::Text(dl, font, 26.0f * s, ImVec2(tx, ry + 60.0f * s),
                Alpha(selected ? theme::kInk : theme::kDim, fade * 0.9f), line);
    dl->PopClipRect();
    const std::string g = std::to_string(a.info.gamerscore) + "G";
    const ImVec2 gs = theme::Measure(stencil, 50.0f * s, g);
    theme::Text(dl, stencil, 50.0f * s, ImVec2(right_x - gs.x, ry + (row_h - 50.0f * s) * 0.45f),
                Alpha(selected ? theme::kRed : (a.unlocked ? theme::kYellow : theme::kDim), fade), g);
    if (ry + row_h > top && ry < bottom) {
      hits_.push_back({ImVec2(x - 30.0f * s, std::max(ry, top)),
                       ImVec2(display.x, std::min(ry + row_h, bottom)), i});
    }
  }
  dl->PopClipRect();
}

void PcMenu::DrawPrompts(ImDrawList* dl, ImVec2 display, float s, float fade) {
  std::vector<Prompt> prompts;
  const Page& page = pages_[page_];
  const Row* row = !page.achievements && !page.rows.empty() ? &page.rows[page.selected] : nullptr;
  if (IsCapturing()) {
    prompts.push_back({nullptr, "ESC", tr("CANCEL", "ANNULLA")});
  } else {
    if (row) {
      const char* what = tr("SELECT", "SELEZIONA");
      if (row->kind == Row::Kind::kChoice || row->kind == Row::Kind::kPadButton) {
        what = tr("CHANGE", "CAMBIA");
      } else if (row->kind == Row::Kind::kKey) {
        what = tr("CHANGE KEY", "CAMBIA TASTO");
      }
      prompts.push_back({"A", g_italian ? "INVIO" : "ENTER", what});
    }
    if (page_ != 0 || mode_ == Mode::kPause) {
      prompts.push_back({"B", "ESC",
                         page_ == 0 ? std::string(tr("RESUME", "RIPRENDI"))
                                    : std::string(tr("BACK", "INDIETRO"))});
    }
    if (row && row->kind == Row::Kind::kKey) {
      prompts.push_back({"X", g_italian ? "CANC" : "DEL", tr("CLEAR", "CANCELLA")});
    }
    if (row && row->kind == Row::Kind::kPadButton) {
      prompts.push_back({"X", g_italian ? "CANC" : "DEL", tr("RESET", "RIPRISTINA")});
    }
    if (PendingRestart()) {
      prompts.push_back({"Y", "R", tr("RESTART NOW", "RIAVVIA ORA")});
    }
  }

  const float h = 44.0f * s;
  const float y = display.y - 70.0f * s - h - 22.0f * s;
  float x = 130.0f * s;
  ImFont* font = theme::MenuFont();
  for (const auto& p : prompts) {
    const float w = pad_prompts_ ? theme::PadGlyph(dl, ImVec2(x, y), h, p.pad ? p.pad : "", fade)
                                 : theme::KeyCap(dl, ImVec2(x, y), h, p.key, fade);
    x += w + 14.0f * s;
    theme::ShadowText(dl, font, 34.0f * s, ImVec2(x, y + h * 0.05f), Alpha(theme::kWhite, fade),
                      p.label, 0.12f, 2.0f * s);
    x += theme::Measure(font, 34.0f * s, p.label).x + 46.0f * s;
  }

  // Restart notice above the prompts.
  if (PendingRestart() && !IsCapturing()) {
    theme::ShadowText(dl, font, 30.0f * s, ImVec2(134.0f * s, y - 52.0f * s),
                      Alpha(theme::kYellow, fade),
                      mode_ == Mode::kPause
                          ? tr("SOME CHANGES APPLY AFTER A RESTART (UNSAVED PROGRESS IS LOST)",
                               "ALCUNE MODIFICHE SI APPLICANO AL RIAVVIO (I PROGRESSI NON SALVATI "
                               "VANNO PERSI)")
                          : tr("SOME CHANGES APPLY AFTER A RESTART",
                               "ALCUNE MODIFICHE SI APPLICANO AL RIAVVIO"),
                      0.1f, 2.0f * s);
  }

  // How to get back here, on the start screen.
  if (mode_ == Mode::kStart && page_ == 0) {
    const char* hint = tr("IN GAME: F1 OR BACK + START OPENS THIS MENU",
                          "IN GIOCO: F1 O BACK + START APRE QUESTO MENU");
    const ImVec2 t = theme::Measure(font, 26.0f * s, hint);
    theme::Text(dl, font, 26.0f * s, ImVec2(display.x - 120.0f * s - t.x, y + h * 0.2f),
                Alpha(theme::kDim, fade), hint);
  }
}
