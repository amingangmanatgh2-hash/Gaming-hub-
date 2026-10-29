#include "hotkey.h"

#include <cctype>
#include <cstring>

namespace core {

static std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool parse_hotkey(const std::string& spec, HotkeySpec& out, std::string& err) {
    err.clear();
    out = HotkeySpec();
    if (spec.empty()) {
        err = "empty shortcut";
        return false;
    }
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i <= spec.size(); ++i) {
        if (i == spec.size() || spec[i] == '+') {
            std::string p = spec.substr(start, i - start);
            // Allow "Ctrl + Alt + V" with spaces around '+'.
            while (!p.empty() && (p.front() == ' ' || p.front() == '\t')) p.erase(p.begin());
            while (!p.empty() && (p.back() == ' ' || p.back() == '\t')) p.pop_back();
            if (p.empty()) {
                err = "empty part in shortcut";
                return false;
            }
            parts.push_back(p);
            start = i + 1;
        }
    }
    if (parts.size() < 2) {
        err = "shortcut needs at least one modifier and a key";
        return false;
    }
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        const std::string m = lower(parts[i]);
        if (m == "ctrl" || m == "control") {
            if (out.ctrl) {
                err = "duplicate modifier";
                return false;
            }
            out.ctrl = true;
        } else if (m == "alt") {
            if (out.alt) {
                err = "duplicate modifier";
                return false;
            }
            out.alt = true;
        } else if (m == "shift") {
            if (out.shift) {
                err = "duplicate modifier";
                return false;
            }
            out.shift = true;
        } else if (m == "win" || m == "super" || m == "meta" || m == "windows") {
            if (out.win) {
                err = "duplicate modifier";
                return false;
            }
            out.win = true;
        } else {
            err = "unknown modifier '" + parts[i] + "'";
            return false;
        }
    }
    const std::string key = parts.back();
    if (key.size() != 1) {
        err = "shortcut key must be a single letter or digit";
        return false;
    }
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
        err = "shortcut key must be A-Z or 0-9";
        return false;
    }
    out.key = c;
    if (!out.valid()) {
        err = "invalid shortcut";
        return false;
    }
    // Reserved by Windows itself as a system-wide kill switch.
    if (out.ctrl && out.alt && c == 'D' && !out.shift && !out.win) {
        err = "Ctrl+Alt+D is reserved by Windows";
        return false;
    }
    return true;
}

std::string format_hotkey(const HotkeySpec& spec) {
    std::string out;
    if (spec.ctrl) out += "Ctrl+";
    if (spec.alt) out += "Alt+";
    if (spec.shift) out += "Shift+";
    if (spec.win) out += "Win+";
    if (spec.key) out.push_back(spec.key);
    return out;
}

bool is_win_v(const HotkeySpec& spec) {
    return spec.win && !spec.ctrl && !spec.alt && !spec.shift && spec.key == 'V';
}

std::vector<std::string> auto_clipboard_candidates() {
    return {"Win+V", "Ctrl+Alt+V", "Ctrl+Shift+V", "Alt+Shift+V"};
}

std::vector<std::string> selectable_clipboard_hotkeys() {
    return {"auto", "Win+V", "Ctrl+Alt+V", "Ctrl+Shift+V", "Alt+Shift+V", "none"};
}

std::string hotkey_failure_text(const std::string& raw_reason, bool persian) {
    if (raw_reason == "conflict") {
        return persian ? u8"این کلید میان\u200cبر توسط برنامه دیگری (یا ویندوز) گرفته شده است."
                       : "This shortcut is already owned by another app (or by Windows).";
    }
    if (raw_reason == "denied") {
        return persian ? u8"ویندوز اجازه ثبت این کلید میان\u200cبر را نداد."
                       : "Windows refused to register this shortcut.";
    }
    if (raw_reason == "unsupported") {
        return persian ? u8"این ترکیب کلید پشتیبانی نمی\u200cشود."
                       : "This key combination is not supported.";
    }
    return persian ? u8"ثبت کلید میان\u200cبر با خطا مواجه شد."
                   : "Registering the shortcut failed.";
}

}  // namespace core
