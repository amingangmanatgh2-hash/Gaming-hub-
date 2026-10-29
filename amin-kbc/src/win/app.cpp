// =============================================================================
//  AMIN Keyboard & Clipboard - src/win/app.cpp
//  Application state, persistence, window orchestration and the hidden message
//  window that receives hotkeys, tray clicks and clipboard notifications.
// =============================================================================
#include "amin.h"

#include <ctime>

namespace amin {

const wchar_t* const kAppName = L"AMIN Keyboard & Clipboard";
const wchar_t* const kAppNameFa = L"\u0635\u0641\u062d\u0647 \u06a9\u0644\u06cc\u062f \u0648 "
                                  L"\u06a9\u0644\u06cc\u067e\u200c\u0628\u0648\u0631\u062f "
                                  L"\u0627\u0645\u06cc\u0646";
const wchar_t* const kAppVersion = L"1.0.0";
const wchar_t* const kMutexName = L"Local\\AminKeyboardClipboard.SingleInstance";
const wchar_t* const kRunValueName = L"AminKeyboardClipboard";
const wchar_t* const kDataFolderName = L"AminKeyboardClipboard";
const wchar_t* const kClassDashboard = L"Amin.Dashboard";
const wchar_t* const kClassKeyboard = L"Amin.Keyboard";
const wchar_t* const kClassClipboard = L"Amin.Clipboard";
const wchar_t* const kClassMessage = L"Amin.Message";

App g_app;

static UINT g_taskbar_created_msg = 0;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
static int64_t now_seconds() { return static_cast<int64_t>(std::time(nullptr)); }

static bool write_probe(const std::wstring& dir) {
    const std::wstring probe = path_join(dir, L".write-test");
    HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    DeleteFileW(probe.c_str());
    return true;
}

void app_init_paths() {
    wchar_t appdata[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appdata))) {
        // Extremely unlikely; fall back next to the executable.
        g_app.data_dir = path_join(exe_dir(), L"data");
        g_app.data_error = L"CSIDL_APPDATA unavailable";
    } else {
        g_app.data_dir = path_join(appdata, kDataFolderName);
    }
    g_app.settings_path = path_join(g_app.data_dir, L"settings.json");
    g_app.history_path = path_join(g_app.data_dir, L"history.json");
    g_app.log_path = path_join(g_app.data_dir, L"app.log");

    g_app.data_writable = ensure_directory(g_app.data_dir) && write_probe(g_app.data_dir);
    if (!g_app.data_writable) {
        g_app.data_error = win32_error_text(GetLastError());
        // Keep running, but do not pretend persistence works.
        app_log(L"data folder not writable: %s (%s)", g_app.data_dir.c_str(),
                g_app.data_error.c_str());
    }
}

// ---------------------------------------------------------------------------
//  Settings / history persistence
// ---------------------------------------------------------------------------
bool app_load_settings() {
    g_app.st = core::default_settings();
    std::string text;
    std::wstring err;
    if (file_read_utf8(g_app.settings_path, &text, &err) && !text.empty()) {
        std::string parse_err;
        core::Settings loaded;
        if (core::settings_from_json(text, loaded, parse_err)) {
            g_app.st = loaded;
        } else {
            app_log(L"settings parse error: %s", std::wstring(parse_err.begin(), parse_err.end()).c_str());
            app_set_status(core::is_persian_ui(g_app.st)
                               ? L"فایل تنظیمات خوانا نبود؛ مقادیر پیش‌فرض استفاده شد."
                               : L"Settings file was unreadable; defaults restored.",
                           color::kWarn);
        }
    }
    core::sanitize_settings(g_app.st);
    // The registry is the source of truth for the startup entry.
    g_app.st.start_with_windows = startup_is_enabled();
    g_app.kb_lang = g_app.st.keyboard_language;
    app_log(L"settings loaded (ui_lang=%d, hotkey=%s)", g_app.st.ui_language,
            g_app.st.clipboard_hotkey.c_str());
    return true;
}

void app_save_settings(bool immediate) {
    if (!g_app.data_writable) return;
    const std::string json = core::settings_to_json(g_app.st);
    std::wstring err;
    if (!file_write_utf8(g_app.settings_path, json, &err)) {
        app_log(L"settings save failed: %s", err.c_str());
        return;
    }
    g_app.settings_dirty = false;
    (void)immediate;
}

bool app_load_history() {
    g_app.store.clear();
    std::string text;
    std::wstring err;
    if (!file_read_utf8(g_app.history_path, &text, &err)) {
        app_log(L"history read error: %s", err.c_str());
        return false;
    }
    if (text.empty()) return true;
    std::string parse_err;
    if (!g_app.store.from_json(text, parse_err)) {
        app_log(L"history parse error: %s", std::wstring(parse_err.begin(), parse_err.end()).c_str());
        // Keep a copy of the unreadable file so a user can recover it manually.
        std::wstring backup = g_app.history_path + L".broken";
        CopyFileW(g_app.history_path.c_str(), backup.c_str(), FALSE);
        return false;
    }
    app_log(L"history loaded: %zu entries", g_app.store.size());
    return true;
}

void app_save_history(bool immediate) {
    if (!g_app.data_writable) {
        g_app.store_dirty = false;
        return;
    }
    const std::string json = g_app.store.to_json();
    std::wstring err;
    if (!file_write_utf8(g_app.history_path, json, &err)) {
        app_log(L"history save failed: %s", err.c_str());
        g_app.store_dirty = true;
        return;
    }
    g_app.store_dirty = false;
    (void)immediate;
}

void app_store_changed(bool added_newest) {
    g_app.store_dirty = true;
    // Debounce disk writes; a burst of copies then costs one write.
    if (g_app.msg) SetTimer(g_app.msg, TIMER_SAVE, 900, nullptr);
    if (g_app.clip && IsWindowVisible(g_app.clip)) clipboard_refresh(g_app.clip);
    if (g_app.dash && IsWindowVisible(g_app.dash)) dashboard_refresh(g_app.dash);
    if (added_newest) tray_update_tooltip();
}

// ---------------------------------------------------------------------------
//  Status line
// ---------------------------------------------------------------------------
void app_set_status(const std::wstring& text, uint32_t color) {
    g_app.status_text = text;
    g_app.status_color = color;
    g_app.status_until = GetTickCount64() + 4500;
    if (g_app.msg) SetTimer(g_app.msg, TIMER_STATUS, 4700, nullptr);
    if (g_app.dash && IsWindowVisible(g_app.dash)) InvalidateRect(g_app.dash, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
//  Windows
// ---------------------------------------------------------------------------
void app_show_dashboard(bool activate) {
    if (!g_app.dash) g_app.dash = dashboard_create(g_app.inst, false);
    if (!g_app.dash) return;
    ShowWindow(g_app.dash, SW_SHOW);
    if (activate) {
        SetForegroundWindow(g_app.dash);
        SetActiveWindow(g_app.dash);
    }
    dashboard_refresh(g_app.dash);
    InvalidateRect(g_app.dash, nullptr, FALSE);
}

void app_hide_dashboard() {
    if (g_app.dash) ShowWindow(g_app.dash, SW_HIDE);
}

void app_toggle_dashboard() {
    if (g_app.dash && IsWindowVisible(g_app.dash)) {
        app_hide_dashboard();
    } else {
        app_show_dashboard(true);
    }
}

void app_show_keyboard(bool activate) {
    // Remember which application the user is working in *before* our window
    // takes focus, so injected keys have a destination.
    inject_track_foreground();
    if (!g_app.kb) {
        g_app.kb = keyboard_create(g_app.inst);
    }
    if (!g_app.kb) {
        app_set_status(core::is_persian_ui(g_app.st) ? L"ساخت پنجره صفحه کلید ناموفق بود"
                                                    : L"Could not create the keyboard window",
                       color::kDanger);
        return;
    }
    keyboard_apply_mode();
    ShowWindow(g_app.kb, SW_SHOWNOACTIVATE);
    if (activate && !g_app.st.keyboard_no_focus) {
        SetForegroundWindow(g_app.kb);
    }
    SetWindowPos(g_app.kb, g_app.st.keyboard_always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0,
                 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    InvalidateRect(g_app.kb, nullptr, FALSE);
    if (g_app.dash) InvalidateRect(g_app.dash, nullptr, FALSE);
    app_log(L"keyboard window shown (topmost=%d, no_focus=%d)",
            g_app.st.keyboard_always_on_top ? 1 : 0, g_app.st.keyboard_no_focus ? 1 : 0);
}

void app_hide_keyboard() {
    if (g_app.kb) ShowWindow(g_app.kb, SW_HIDE);
    if (g_app.dash) InvalidateRect(g_app.dash, nullptr, FALSE);
}

void app_toggle_keyboard() {
    if (g_app.kb && IsWindowVisible(g_app.kb)) {
        app_hide_keyboard();
    } else {
        app_show_keyboard(true);
    }
}

void app_show_clipboard(bool activate) {
    inject_track_foreground();
    if (!g_app.clip) g_app.clip = clipboard_create(g_app.inst);
    if (!g_app.clip) return;
    clipboard_refresh(g_app.clip);
    ShowWindow(g_app.clip, SW_SHOW);
    SetWindowPos(g_app.clip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (activate) {
        SetForegroundWindow(g_app.clip);
        clipboard_focus_search(g_app.clip);
    }
    InvalidateRect(g_app.clip, nullptr, FALSE);
    app_log(L"clipboard window shown");
}

void app_hide_clipboard() {
    if (g_app.clip) ShowWindow(g_app.clip, SW_HIDE);
}

void app_toggle_clipboard() {
    if (g_app.clip && IsWindowVisible(g_app.clip)) {
        app_hide_clipboard();
    } else {
        app_show_clipboard(true);
    }
}

void app_focus_previous_target() {
    if (!g_app.target || !IsWindow(g_app.target)) return;
    if (!IsWindowVisible(g_app.target)) return;
    focus_restore_target();
}

void app_copy_entry(size_t index, bool hide_after) {
    if (index >= g_app.store.size()) return;
    const std::wstring text = wstr(g_app.store.at(index).text);
    if (!clipboard_set_text(g_app.clip ? g_app.clip : g_app.msg, text)) {
        app_set_status(core::is_persian_ui(g_app.st) ? L"دسترسی به کلیپ‌بورد ممکن نشد"
                                                    : L"Could not access the clipboard",
                       color::kDanger);
        return;
    }
    app_set_status(core::is_persian_ui(g_app.st) ? L"در کلیپ‌بورد کپی شد؛ با Ctrl+V بچسبانید"
                                                : L"Copied. Paste it with Ctrl+V",
                   color::kSuccess);
    if (hide_after && g_app.st.hide_clipboard_after_copy) {
        app_hide_clipboard();
        if (g_app.st.restore_focus_after_copy) app_focus_previous_target();
    }
    if (g_app.clip && IsWindowVisible(g_app.clip)) clipboard_refresh(g_app.clip);
}

void app_apply_keyboard_mode() {
    keyboard_apply_mode();
    if (g_app.kb) InvalidateRect(g_app.kb, nullptr, FALSE);
}

void app_apply_startup(bool enable) {
    std::wstring err;
    if (!startup_set_enabled(enable, &err)) {
        const std::wstring msg =
            (core::is_persian_ui(g_app.st) ? L"تنظیم اجرای خودکار ممکن نشد:\n" : L"Startup could not be changed:\n") +
            err;
        message_box(g_app.dash, msg, T.error_title, MB_ICONERROR | MB_OK);
        app_set_status(msg, color::kDanger);
        return;
    }
    g_app.st.start_with_windows = enable;
    app_save_settings(true);
    app_set_status(enable ? (core::is_persian_ui(g_app.st) ? L"اجرای خودکار با ویندوز فعال شد"
                                                          : L"Start with Windows enabled")
                          : (core::is_persian_ui(g_app.st) ? L"اجرای خودکار با ویندوز غیرفعال شد"
                                                          : L"Start with Windows disabled"),
                   color::kSuccess);
    app_refresh_ui();
}

void app_refresh_ui() {
    if (g_app.dash && IsWindowVisible(g_app.dash)) InvalidateRect(g_app.dash, nullptr, FALSE);
    if (g_app.clip && IsWindowVisible(g_app.clip)) InvalidateRect(g_app.clip, nullptr, FALSE);
    if (g_app.kb && IsWindowVisible(g_app.kb)) InvalidateRect(g_app.kb, nullptr, FALSE);
    tray_update_tooltip();
}

void app_exit() {
    if (g_app.exiting) return;
    g_app.exiting = true;
    app_log(L"exit requested");
    if (g_app.store_dirty) app_save_history(true);
    app_save_settings(true);
    app_unregister_hotkeys();
    clipboard_monitor_stop();
    tray_remove();
    if (g_app.msg) DestroyWindow(g_app.msg);
    PostQuitMessage(0);
}

// ---------------------------------------------------------------------------
//  Clipboard capture
// ---------------------------------------------------------------------------
namespace {

void handle_clipboard_update() {
    if (!g_app.st.clipboard_monitoring) return;
    if (g_app.clipboard_suppress_until && GetTickCount64() < g_app.clipboard_suppress_until) {
        // This update was caused by the app itself (copy button / Ctrl+C on an
        // entry); do not re-order the history for it.
        g_app.clipboard_suppress_until = 0;
        return;
    }
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) && !IsClipboardFormatAvailable(CF_TEXT)) {
        // Images, files, etc. are intentionally not captured (text only).
        return;
    }
    const std::wstring text = clipboard_get_text();
    if (text.empty()) return;
    if (text.size() > core::ClipboardStore::kMaxChars) {
        app_set_status(core::is_persian_ui(g_app.st)
                           ? L"متن بسیار بزرگ بود و ذخیره نشد (بیش از ۱۰۰٬۰۰۰ کاراکتر)"
                           : L"Text too large to store (>100,000 characters)",
                       color::kWarn);
        return;
    }
    const core::u16 converted = u16of(text);
    const core::ClipboardStore::AddResult r = g_app.store.add(converted, now_seconds());
    if (r == core::ClipboardStore::AddResult::kRejectedEmpty) return;
    if (r == core::ClipboardStore::AddResult::kRejectedTooBig) return;
    g_app.clip_sel = 0;
    g_app.clip_scroll = 0;
    app_store_changed(true);
    app_log(L"clipboard captured (%zu chars, %s)", text.size(),
            (r == core::ClipboardStore::AddResult::kMovedToTop) ? L"moved to top" : L"new");
}

}  // namespace

// ---------------------------------------------------------------------------
//  Hidden message window
// ---------------------------------------------------------------------------
static LRESULT CALLBACK message_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (g_taskbar_created_msg && msg == g_taskbar_created_msg) {
        tray_restore_icon();
        return 0;
    }
    switch (msg) {
        case WM_CREATE:
            return 0;
        case WM_TIMER:
            switch (wparam) {
                case TIMER_TRACK:
                    inject_track_foreground();
                    return 0;
                case TIMER_SAVE:
                    KillTimer(hwnd, TIMER_SAVE);
                    if (g_app.store_dirty) app_save_history();
                    return 0;
                case TIMER_STATUS:
                    KillTimer(hwnd, TIMER_STATUS);
                    if (g_app.dash) InvalidateRect(g_app.dash, nullptr, FALSE);
                    return 0;
                default:
                    return 0;
            }
        case WM_HOTKEY:
            if (wparam == kHotkeyClipboard) {
                app_toggle_clipboard();
            } else if (wparam == kHotkeyKeyboard) {
                app_toggle_keyboard();
            }
            return 0;
        case WM_CLIPBOARDUPDATE:
            handle_clipboard_update();
            return 0;
        case WM_AMIN_SHOW_DASHBOARD:
            app_show_dashboard(true);
            return 0;
        case WM_AMIN_SHOW_CLIPBOARD:
            app_show_clipboard(true);
            return 0;
        case WM_AMIN_SHOW_KEYBOARD:
            app_show_keyboard(true);
            return 0;
        case WM_AMIN_TOGGLE_CLIPBOARD:
            app_toggle_clipboard();
            return 0;
        case WM_AMIN_TRAY:
            switch (LOWORD(lparam)) {
                case WM_LBUTTONUP:
                    app_toggle_dashboard();
                    return 0;
                case WM_LBUTTONDBLCLK:
                    app_show_dashboard(true);
                    return 0;
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                    tray_show_menu(hwnd);
                    return 0;
                default:
                    return 0;
            }
        case WM_QUERYENDSESSION:
            app_save_history(true);
            app_save_settings(true);
            return TRUE;
        case WM_ENDSESSION:
            if (wparam) {
                app_save_history(true);
                app_save_settings(true);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

bool message_window_register(HINSTANCE inst) {
    WNDCLASSEXW wc;
    std::memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = message_window_proc;
    wc.hInstance = inst;
    wc.lpszClassName = kClassMessage;
    if (!RegisterClassExW(&wc)) return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    g_taskbar_created_msg = RegisterWindowMessageW(L"TaskbarCreated");
    return true;
}

HWND message_window_create(HINSTANCE inst) {
    // A normal (never shown) top level window is used instead of a
    // HWND_MESSAGE window: message-only windows do not receive broadcasts such
    // as "TaskbarCreated", which the tray icon needs after an Explorer restart.
    return CreateWindowExW(WS_EX_TOOLWINDOW, kClassMessage, L"AMIN KBC", WS_POPUP, 0, 0, 0, 0,
                           nullptr, nullptr, inst, nullptr);
}

}  // namespace amin
