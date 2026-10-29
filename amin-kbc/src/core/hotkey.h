// =============================================================================
//  AMIN Keyboard & Clipboard - core/hotkey.h
//  Global shortcut parsing/formatting and the candidate list used when the
//  preferred shortcut cannot be registered (e.g. Win+V is owned by Windows).
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace core {

struct HotkeySpec {
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    bool win = false;
    char key = 0;  // 'A'..'Z' or '0'..'9', upper case

    bool valid() const { return key != 0 && (ctrl || alt || shift || win); }
};

// Accepts "Ctrl+Alt+V", "ctrl+shift+v", "Win+V", "Alt+Shift+K".
// Returns false (and a short reason) for anything else.
bool parse_hotkey(const std::string& spec, HotkeySpec& out, std::string& err);

// "Ctrl+Alt+V" - modifiers first, key last, all upper case.
std::string format_hotkey(const HotkeySpec& spec);

// True when the spec is Win+V (the shortcut Windows itself grabs on 1809+).
bool is_win_v(const HotkeySpec& spec);

// The shortcuts tried in order when settings say "auto":
//   1) Win+V        (free on Windows 10 1803-1809 before clipboard history)
//   2) Ctrl+Alt+V   (recommended alternative)
//   3) Ctrl+Shift+V
//   4) Alt+Shift+V
std::vector<std::string> auto_clipboard_candidates();

// Everything the settings UI offers.
std::vector<std::string> selectable_clipboard_hotkeys();

// Why a registration failed, in the user's language ("persian" selects Farsi).
// "reason" is a raw code: "conflict", "denied", "unsupported", "other".
std::string hotkey_failure_text(const std::string& raw_reason, bool persian);

}  // namespace core
