// stuntman - controller and keyboard input around the game. See pad_input.h.

#include "pad_input.h"

#include "bench.h"

#include <rex/cvar.h>
#include <rex/input/input.h>
#include <rex/input/input_system.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#define STUNTMAN_PAD_CVAR(name, def, what)                                          \
  REXCVAR_DEFINE_STRING(name, def, "Stuntman/Controller",                           \
                        "What the " what " does: a, b, x, y, lb, rb, lt, rt, back, " \
                        "start, ls, rs, up, down, left, right or none")

STUNTMAN_PAD_CVAR(pad_a, "a", "A button");
STUNTMAN_PAD_CVAR(pad_b, "b", "B button");
STUNTMAN_PAD_CVAR(pad_x, "x", "X button");
STUNTMAN_PAD_CVAR(pad_y, "y", "Y button");
STUNTMAN_PAD_CVAR(pad_lb, "lb", "left bumper");
STUNTMAN_PAD_CVAR(pad_rb, "rb", "right bumper");
STUNTMAN_PAD_CVAR(pad_lt, "lt", "left trigger");
STUNTMAN_PAD_CVAR(pad_rt, "rt", "right trigger");
STUNTMAN_PAD_CVAR(pad_back, "back", "Back button");
STUNTMAN_PAD_CVAR(pad_start, "start", "Start button");
STUNTMAN_PAD_CVAR(pad_ls, "ls", "left stick click");
STUNTMAN_PAD_CVAR(pad_rs, "rs", "right stick click");
STUNTMAN_PAD_CVAR(pad_up, "up", "D-pad up");
STUNTMAN_PAD_CVAR(pad_down, "down", "D-pad down");
STUNTMAN_PAD_CVAR(pad_left, "left", "D-pad left");
STUNTMAN_PAD_CVAR(pad_right, "right", "D-pad right");

namespace pad {

namespace {

using Clock = std::chrono::steady_clock;
using rex::input::DeviceId;
using rex::input::DeviceInfo;
using rex::input::X_INPUT_STATE;

struct ButtonInfo {
  const char* label;
  const char* token;  // value in the pad_* settings
  const char* cvar;
  uint16_t xinput;    // 0 for the triggers
};

constexpr ButtonInfo kButtons[kButtonCount] = {
    {"A", "a", "pad_a", rex::input::X_INPUT_GAMEPAD_A},
    {"B", "b", "pad_b", rex::input::X_INPUT_GAMEPAD_B},
    {"X", "x", "pad_x", rex::input::X_INPUT_GAMEPAD_X},
    {"Y", "y", "pad_y", rex::input::X_INPUT_GAMEPAD_Y},
    {"LB", "lb", "pad_lb", rex::input::X_INPUT_GAMEPAD_LEFT_SHOULDER},
    {"RB", "rb", "pad_rb", rex::input::X_INPUT_GAMEPAD_RIGHT_SHOULDER},
    {"LT", "lt", "pad_lt", 0},
    {"RT", "rt", "pad_rt", 0},
    {"BACK", "back", "pad_back", rex::input::X_INPUT_GAMEPAD_BACK},
    {"START", "start", "pad_start", rex::input::X_INPUT_GAMEPAD_START},
    {"LS", "ls", "pad_ls", rex::input::X_INPUT_GAMEPAD_LEFT_THUMB},
    {"RS", "rs", "pad_rs", rex::input::X_INPUT_GAMEPAD_RIGHT_THUMB},
    {"D-PAD UP", "up", "pad_up", rex::input::X_INPUT_GAMEPAD_DPAD_UP},
    {"D-PAD DOWN", "down", "pad_down", rex::input::X_INPUT_GAMEPAD_DPAD_DOWN},
    {"D-PAD LEFT", "left", "pad_left", rex::input::X_INPUT_GAMEPAD_DPAD_LEFT},
    {"D-PAD RIGHT", "right", "pad_right", rex::input::X_INPUT_GAMEPAD_DPAD_RIGHT},
};

// Our keyboard layout: WASD to drive, arrows for the menus and the left stick.
struct KeyDefault {
  const char* cvar;
  const char* keys;
};
constexpr KeyDefault kKeyDefaults[] = {
    {"keybind_right_trigger", "W"},
    {"keybind_left_trigger", "S"},
    {"keybind_lstick_left", "A,Left"},
    {"keybind_lstick_right", "D,Right"},
    {"keybind_lstick_up", "Up"},
    {"keybind_lstick_down", "Down"},
    {"keybind_a", "Space,Return"},
    {"keybind_b", "Backspace"},
    {"keybind_x", "E"},
    {"keybind_y", "Q"},
    {"keybind_left_shoulder", "Z"},
    {"keybind_right_shoulder", "C"},
    {"keybind_lstick_press", "R"},
    {"keybind_rstick_press", "F"},
    {"keybind_rstick_up", "I"},
    {"keybind_rstick_down", "K"},
    {"keybind_rstick_left", "J"},
    {"keybind_rstick_right", "L"},
    {"keybind_dpad_up", "1"},
    {"keybind_dpad_down", "2"},
    {"keybind_dpad_left", "3"},
    {"keybind_dpad_right", "4"},
    {"keybind_back", "Tab"},
    {"keybind_start", "Escape,P"},
};

constexpr uint8_t kTriggerPressed = 30;  // XINPUT_GAMEPAD_TRIGGER_THRESHOLD
constexpr int16_t kStickPressed = 16000;
constexpr auto kFresh = std::chrono::milliseconds(250);

// Layout: target button for each physical one. Written by the UI thread,
// read on the guest's input thread.
std::array<std::atomic<uint8_t>, kButtonCount> g_layout;
std::atomic<bool> g_identity{true};

std::atomic<bool> g_menu_open{false};
std::atomic<uint32_t> g_hold_epoch{0};  // bumped when the menu closes

struct DeviceState {
  uint32_t buttons = 0;  // raw, as a Button mask
  Clock::time_point seen;
  uint32_t released_epoch = 0;  // last hold this device came out of
};
std::mutex g_devices_mutex;
std::unordered_map<DeviceId, DeviceState> g_devices;

uint32_t RawMask(const X_INPUT_STATE& state) {
  const auto& g = state.gamepad;
  const uint16_t buttons = g.buttons;
  uint32_t mask = 0;
  for (int i = 0; i < kButtonCount; ++i) {
    if (kButtons[i].xinput && (buttons & kButtons[i].xinput)) mask |= 1u << i;
  }
  if (g.left_trigger > kTriggerPressed) mask |= 1u << kLT;
  if (g.right_trigger > kTriggerPressed) mask |= 1u << kRT;
  return mask;
}

bool Idle(const X_INPUT_STATE& state) {
  const auto& g = state.gamepad;
  return RawMask(state) == 0 && std::abs(int(int16_t(g.thumb_lx))) < kStickPressed &&
         std::abs(int(int16_t(g.thumb_ly))) < kStickPressed &&
         std::abs(int(int16_t(g.thumb_rx))) < kStickPressed &&
         std::abs(int(int16_t(g.thumb_ry))) < kStickPressed;
}

void Neutral(X_INPUT_STATE& state) {
  std::memset(&state.gamepad, 0, sizeof(state.gamepad));
}

void Remap(X_INPUT_STATE& state) {
  auto& g = state.gamepad;
  const uint16_t in = g.buttons;
  const uint8_t in_lt = g.left_trigger, in_rt = g.right_trigger;
  uint16_t out = in & rex::input::X_INPUT_GAMEPAD_GUIDE;
  uint8_t out_lt = 0, out_rt = 0;
  for (int i = 0; i < kButtonCount; ++i) {
    uint8_t value;  // how far this button is pressed, 0-255
    if (i == kLT) {
      value = in_lt;
    } else if (i == kRT) {
      value = in_rt;
    } else {
      value = (in & kButtons[i].xinput) ? 255 : 0;
    }
    if (!value) continue;
    const int target = g_layout[i].load(std::memory_order_relaxed);
    if (target == kLT) {
      out_lt = std::max(out_lt, value);
    } else if (target == kRT) {
      out_rt = std::max(out_rt, value);
    } else if (target < kButtonCount && value > kTriggerPressed) {
      out |= kButtons[target].xinput;
    }
  }
  g.buttons = out;
  g.left_trigger = out_lt;
  g.right_trigger = out_rt;
}

void Filter(const DeviceInfo& device, X_INPUT_STATE& state) {
  const bool menu = g_menu_open.load(std::memory_order_relaxed);
  const uint32_t epoch = g_hold_epoch.load(std::memory_order_relaxed);
  bool hold = false;
  {
    std::lock_guard lock(g_devices_mutex);
    auto& d = g_devices[device.id];
    // The keyboard is read through ImGui by the menu.
    d.buttons = device.synthetic ? 0 : RawMask(state);
    if (!device.synthetic) {
      const auto& g = state.gamepad;
      const int lx = int16_t(g.thumb_lx), ly = int16_t(g.thumb_ly);
      if (ly > kStickPressed) d.buttons |= 1u << kUp;
      if (ly < -kStickPressed) d.buttons |= 1u << kDown;
      if (lx < -kStickPressed) d.buttons |= 1u << kLeft;
      if (lx > kStickPressed) d.buttons |= 1u << kRight;
    }
    d.seen = Clock::now();
    if (!menu && d.released_epoch != epoch) {
      if (Idle(state)) {
        d.released_epoch = epoch;
      } else {
        hold = true;
      }
    }
  }
  if (menu || hold) {
    Neutral(state);
    return;
  }
  if (!device.synthetic && !g_identity.load(std::memory_order_relaxed)) Remap(state);
  bench::Inject(state);
}

int ParseTarget(std::string_view token, int fallback) {
  if (token == "none") return kNone;
  for (int i = 0; i < kButtonCount; ++i) {
    if (token == kButtons[i].token) return i;
  }
  return fallback;
}

}  // namespace

const char* ButtonLabel(int button) {
  return button >= 0 && button < kButtonCount ? kButtons[button].label : "-";
}

const char* LayoutCvar(int button) { return kButtons[button].cvar; }

const char* LayoutToken(int target) {
  return target >= 0 && target < kButtonCount ? kButtons[target].token : "none";
}

int LayoutTarget(int button) { return g_layout[button].load(std::memory_order_relaxed); }

void Install(rex::input::InputSystem* input) {
  ReloadLayout();
  if (input) input->SetStateFilter(&Filter);
}

void ReloadLayout() {
  bool identity = true;
  for (int i = 0; i < kButtonCount; ++i) {
    const int target = ParseTarget(rex::cvar::GetFlagByName(kButtons[i].cvar), i);
    g_layout[i].store(uint8_t(target), std::memory_order_relaxed);
    identity &= target == i;
  }
  g_identity.store(identity, std::memory_order_relaxed);
}

void ApplyKeyboardDefaults() {
  for (const auto& k : kKeyDefaults) {
    if (rex::cvar::GetFlagSource(k.cvar) == rex::cvar::Source::kDefault) {
      rex::cvar::SetFlagByName(k.cvar, k.keys);
    }
  }
  if (rex::cvar::GetFlagSource("mnk_mode") == rex::cvar::Source::kDefault) {
    rex::cvar::SetFlagByName("mnk_mode", "true");
  }
}

const char* KeyboardDefault(const char* cvar) {
  for (const auto& k : kKeyDefaults) {
    if (std::string_view(k.cvar) == cvar) return k.keys;
  }
  return "";
}

void SetMenuOpen(bool open) {
  if (!open && g_menu_open.load()) g_hold_epoch.fetch_add(1);
  g_menu_open.store(open);
}

uint32_t MenuButtons() {
  const auto now = Clock::now();
  uint32_t mask = 0;
  std::lock_guard lock(g_devices_mutex);
  for (const auto& [id, d] : g_devices) {
    if (now - d.seen < kFresh) mask |= d.buttons;
  }
  return mask;
}

void PollForMenu(rex::input::InputSystem* input) {
  if (!input) return;
  for (uint32_t user = 0; user < rex::input::kMaxGuestUsers; ++user) {
    X_INPUT_STATE state{};
    input->GetState(user, &state);
  }
}

}  // namespace pad
