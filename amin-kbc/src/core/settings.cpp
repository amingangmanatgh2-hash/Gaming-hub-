#include "settings.h"

#include "hotkey.h"
#include "json.h"

namespace core {

Settings default_settings() {
    Settings s;
    // Persian is the primary audience for this app; both labels are shown in
    // the UI anyway (English in a smaller line).
    s.ui_language = static_cast<int>(UiLanguage::Persian);
    s.keyboard_language = 0;
    return s;
}

static int clamp_int(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void sanitize_settings(Settings& s) {
    s.version = 1;
    s.ui_language = clamp_int(s.ui_language, 0, 1);
    s.keyboard_language = clamp_int(s.keyboard_language, 0, 1);
    s.last_hotkey_result = clamp_int(s.last_hotkey_result, 0, 2);

    HotkeySpec tmp;
    std::string err;
    if (s.clipboard_hotkey != "auto" && s.clipboard_hotkey != "none" &&
        !parse_hotkey(s.clipboard_hotkey, tmp, err)) {
        s.clipboard_hotkey = "auto";
    }
    if (s.keyboard_hotkey != "none" && !parse_hotkey(s.keyboard_hotkey, tmp, err)) {
        s.keyboard_hotkey = "Ctrl+Alt+K";
    }
}

std::string settings_to_json(const Settings& s) {
    Json root = Json::make_object();
    root.set("app", Json::make_string("AMIN Keyboard & Clipboard"));
    root.set("version", Json::make_number(s.version));
    root.set("wizard_done", Json::make_bool(s.wizard_done));
    root.set("start_with_windows", Json::make_bool(s.start_with_windows));
    root.set("minimize_to_tray", Json::make_bool(s.minimize_to_tray));
    root.set("show_tray_icon", Json::make_bool(s.show_tray_icon));
    root.set("clipboard_monitoring", Json::make_bool(s.clipboard_monitoring));
    root.set("hide_clipboard_after_copy", Json::make_bool(s.hide_clipboard_after_copy));
    root.set("restore_focus_after_copy", Json::make_bool(s.restore_focus_after_copy));
    root.set("keep_history_on_exit", Json::make_bool(s.keep_history_on_exit));
    root.set("keyboard_always_on_top", Json::make_bool(s.keyboard_always_on_top));
    root.set("keyboard_no_focus", Json::make_bool(s.keyboard_no_focus));
    root.set("keyboard_show_preview", Json::make_bool(s.keyboard_show_preview));
    root.set("clipboard_hotkey", Json::make_string(s.clipboard_hotkey));
    root.set("clipboard_hotkey_enabled", Json::make_bool(s.clipboard_hotkey_enabled));
    root.set("keyboard_hotkey", Json::make_string(s.keyboard_hotkey));
    root.set("keyboard_hotkey_enabled", Json::make_bool(s.keyboard_hotkey_enabled));
    root.set("ui_language", Json::make_number(s.ui_language));
    root.set("keyboard_language", Json::make_number(s.keyboard_language));
    return root.dump(true);
}

bool settings_from_json(const std::string& json_text, Settings& out, std::string& err) {
    out = default_settings();
    Json root;
    if (!Json::parse(json_text, root, err)) return false;
    if (!root.is_object()) {
        err = "root is not an object";
        return false;
    }
    out.wizard_done = root.get_bool("wizard_done", out.wizard_done);
    out.start_with_windows = root.get_bool("start_with_windows", out.start_with_windows);
    out.minimize_to_tray = root.get_bool("minimize_to_tray", out.minimize_to_tray);
    out.show_tray_icon = root.get_bool("show_tray_icon", out.show_tray_icon);
    out.clipboard_monitoring = root.get_bool("clipboard_monitoring", out.clipboard_monitoring);
    out.hide_clipboard_after_copy =
        root.get_bool("hide_clipboard_after_copy", out.hide_clipboard_after_copy);
    out.restore_focus_after_copy =
        root.get_bool("restore_focus_after_copy", out.restore_focus_after_copy);
    out.keep_history_on_exit = root.get_bool("keep_history_on_exit", out.keep_history_on_exit);
    out.keyboard_always_on_top =
        root.get_bool("keyboard_always_on_top", out.keyboard_always_on_top);
    out.keyboard_no_focus = root.get_bool("keyboard_no_focus", out.keyboard_no_focus);
    out.keyboard_show_preview = root.get_bool("keyboard_show_preview", out.keyboard_show_preview);
    out.clipboard_hotkey = root.get_string("clipboard_hotkey", out.clipboard_hotkey);
    out.clipboard_hotkey_enabled =
        root.get_bool("clipboard_hotkey_enabled", out.clipboard_hotkey_enabled);
    out.keyboard_hotkey = root.get_string("keyboard_hotkey", out.keyboard_hotkey);
    out.keyboard_hotkey_enabled =
        root.get_bool("keyboard_hotkey_enabled", out.keyboard_hotkey_enabled);
    out.ui_language = static_cast<int>(root.get_number("ui_language", out.ui_language));
    out.keyboard_language =
        static_cast<int>(root.get_number("keyboard_language", out.keyboard_language));

    // Migration: older builds stored keyboard_language as a string.
    const Json* kl = root.find("keyboard_language");
    if (kl && kl->is_string()) {
        out.keyboard_language = (kl->string_value() == "en") ? 1 : 0;
    }
    sanitize_settings(out);
    return true;
}

bool is_persian_ui(const Settings& s) { return s.ui_language == static_cast<int>(UiLanguage::Persian); }

}  // namespace core
