// stuntman - controller and keyboard input around the game
//
// Hooks the input system's per-device state filter (SDK patch 0003) to:
//   - remap controller buttons (cvars pad_a ... pad_right, PC menu >
//     Controls > Controller layout);
//   - keep the game from seeing any input while the PC menu is open, and
//     after it closes until the buttons are let go, so the press that closed
//     the menu doesn't reach the game;
//   - read the controllers for the menu's own navigation, before remapping,
//     so a bad layout can always be undone.
// The keyboard is the SDK's keyboard/mouse device (cvars keybind_*); this file
// only gives it defaults suited to a driving game.

#pragma once

#include <cstdint>

namespace rex::input {
class InputSystem;
}

namespace pad {

// Controller buttons, in menu order. Bit i of a button mask is Button i.
enum Button : int {
  kA, kB, kX, kY, kLB, kRB, kLT, kRT, kBack, kStart, kLS, kRS,
  kUp, kDown, kLeft, kRight,
  kButtonCount,
};
constexpr int kNone = kButtonCount;  // a button mapped to nothing

// Short label shown in menus ("A", "LB", "D-PAD UP").
const char* ButtonLabel(int button);
// Layout setting for a physical button ("pad_a").
const char* LayoutCvar(int button);
// Value naming a target button in a layout setting ("a", "none").
const char* LayoutToken(int target);
// What a physical button currently does (a Button, or kNone).
int LayoutTarget(int button);

// Installs the state filter and loads the layout. Call before the guest polls.
void Install(rex::input::InputSystem* input);

// Re-reads the pad_* settings after the menu changed them.
void ReloadLayout();

// Gives the keyboard bindings and keyboard mode our defaults, where the
// player hasn't set them. Call once the config is loaded.
void ApplyKeyboardDefaults();
// Default value for a keybind_* setting (ours, not the SDK's).
const char* KeyboardDefault(const char* cvar);

// While open, the game gets neutral input. Closing holds each device until
// its buttons are released.
void SetMenuOpen(bool open);

// Buttons held on any controller, before remapping; the left stick also
// counts as the D-pad. Only fresh while the guest polls input, or after
// PollForMenu.
uint32_t MenuButtons();

// Polls every controller once. Only for when the guest isn't running yet
// (the start screen): the guest's own polling must not run concurrently.
void PollForMenu(rex::input::InputSystem* input);

}  // namespace pad
