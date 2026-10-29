// =============================================================================
//  AMIN Keyboard & Clipboard - core/settings.h
//  User settings, persisted as JSON next to the clipboard history.
// =============================================================================
#pragma once

#include <string>

namespace core {

enum class UiLanguage { Persian = 0, English = 1 };

struct Settings {
    int version = 1;

    // First run wizard
    bool wizard_done = false;

    // Startup / shell integration
    bool start_with_windows = false;      // HKCU\...\Run value (per user, no admin)
    bool minimize_to_tray = true;
    bool show_tray_icon = true;

    // Clipboard
    bool clipboard_monitoring = true;     // false = paused by the user
    bool hide_clipboard_after_copy = true;
    bool restore_focus_after_copy = true;
    bool keep_history_on_exit = true;

    // Keyboard
    bool keyboard_always_on_top = true;
    bool keyboard_no_focus = false;       // WS_EX_NOACTIVATE: never steals focus
    bool keyboard_show_preview = true;

    // Global shortcuts. "auto" = try Win+V, then Ctrl+Alt+V, then Ctrl+Shift+V.
    std::string clipboard_hotkey = "auto";
    bool clipboard_hotkey_enabled = true;
    std::string keyboard_hotkey = "Ctrl+Alt+K";
    bool keyboard_hotkey_enabled = true;

    // UI
    int ui_language = 0;                  // core::UiLanguage
    int keyboard_language = 0;            // 0 = Persian, 1 = English
    int last_hotkey_result = 0;           // 0 unknown, 1 registered, 2 failed (display only)
};

Settings default_settings();

std::string settings_to_json(const Settings& s);
// Missing keys keep their default value, so forward/backward compatible.
bool settings_from_json(const std::string& json_text, Settings& out, std::string& err);

// Normalizes user supplied values (hotkey names, language indices, ...).
void sanitize_settings(Settings& s);

// Valid hotkey spec helpers are in hotkey.h; this is the list shown in the UI.
bool is_persian_ui(const Settings& s);

}  // namespace core
