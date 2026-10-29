// =============================================================================
//  AMIN Keyboard & Clipboard - src/win/platform.cpp
//  Focus tracking + Unicode injection, global hotkeys, tray icon,
//  clipboard listener, startup registry value and shell shortcuts.
//
//  Every Win32 call here exists on Windows 7 SP1 and Windows 10 1803.
// =============================================================================
#include "amin.h"

#include <objbase.h>

#include <cstring>
#include <cwchar>

namespace amin {

// ---------------------------------------------------------------------------
//  Focus tracking
// ---------------------------------------------------------------------------
namespace {

bool is_our_process_window(HWND hwnd) {
    if (!hwnd) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid == GetCurrentProcessId();
}

bool is_shell_window(HWND hwnd) {
    wchar_t cls[64] = {0};
    if (!GetClassNameW(hwnd, cls, 63)) return false;
    return wcscmp(cls, L"Shell_TrayWnd") == 0 || wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0 ||
           wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0 ||
           wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0 ||
           wcscmp(cls, L"TopLevelWindowForOverflowXamlIsland") == 0;
}

void remember(HWND hwnd) {
    if (!hwnd || hwnd == g_app.target) return;
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) return;
    g_app.target = hwnd;
    wchar_t title[256] = {0};
    GetWindowTextW(hwnd, title, 255);
    g_app.target_title.assign(title);
    // Trim absurdly long titles.
    if (g_app.target_title.size() > 80) {
        g_app.target_title.resize(77);
        g_app.target_title += L"...";
    }
}

}  // namespace

void inject_track_foreground() {
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    if (is_our_process_window(fg)) return;  // our own window: keep the previous target
    if (is_shell_window(fg)) return;
    remember(fg);
}

void focus_restore_target() {
    if (!g_app.target || !IsWindow(g_app.target)) return;
    HWND fg = GetForegroundWindow();
    if (fg == g_app.target) return;
    if (!is_our_process_window(fg)) return;  // somebody else is active; do not steal focus

    if (IsIconic(g_app.target)) ShowWindow(g_app.target, SW_RESTORE);

    for (int attempt = 0; attempt < 4; ++attempt) {
        SetForegroundWindow(g_app.target);
        if (GetForegroundWindow() == g_app.target) return;

        // Windows can refuse SetForegroundWindow (foreground lock). A temporary
        // AttachThreadInput to the current foreground thread lifts that
        // restriction; it is the documented workaround and safe to use briefly.
        HWND current = GetForegroundWindow();
        const DWORD fg_thread = current ? GetWindowThreadProcessId(current, nullptr) : 0;
        const DWORD my_thread = GetCurrentThreadId();
        bool attached = false;
        if (fg_thread && fg_thread != my_thread) {
            attached = AttachThreadInput(my_thread, fg_thread, TRUE) != FALSE;
        }
        BringWindowToTop(g_app.target);
        SetForegroundWindow(g_app.target);
        SetActiveWindow(g_app.target);
        if (attached) AttachThreadInput(my_thread, fg_thread, FALSE);
        if (GetForegroundWindow() == g_app.target) return;
        Sleep(6);  // give the target thread time to process the activation
    }
}

namespace {

bool send_input_pair(const INPUT& down, const INPUT& up) {
    INPUT in[2] = {down, up};
    const UINT sent = SendInput(2, in, sizeof(INPUT));
    return sent == 2;
}

bool send_unicode_unit(wchar_t ch) {
    INPUT down;
    std::memset(&down, 0, sizeof(down));
    down.type = INPUT_KEYBOARD;
    down.ki.wVk = 0;
    down.ki.wScan = ch;
    down.ki.dwFlags = KEYEVENTF_UNICODE;
    down.ki.time = 0;
    down.ki.dwExtraInfo = GetMessageExtraInfo();
    INPUT up = down;
    up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    return send_input_pair(down, up);
}

}  // namespace

namespace {

// Makes sure the character we are about to send lands in the target
// application instead of in our own window.
bool ensure_target_receives_input() {
    HWND fg = GetForegroundWindow();
    if (fg && !is_our_process_window(fg)) return true;  // already the right app
    if (!g_app.target || !IsWindow(g_app.target)) return false;  // nothing remembered
    for (int i = 0; i < 3; ++i) {
        focus_restore_target();
        if (GetForegroundWindow() == g_app.target) return true;
        Sleep(8);
    }
    return GetForegroundWindow() == g_app.target;
}

}  // namespace

bool inject_char(wchar_t ch) {
    if (!ensure_target_receives_input()) {
        app_log(L"inject: no target window available (target=%p)", static_cast<void*>(g_app.target));
        return false;
    }
    return send_unicode_unit(ch);
}

bool inject_text(const std::wstring& text) {
    bool ok = true;
    for (size_t i = 0; i < text.size(); ++i) {
        if (!inject_char(text[i])) ok = false;
    }
    return ok;
}

bool inject_vk(UINT vk) {
    ensure_target_receives_input();
    INPUT down;
    std::memset(&down, 0, sizeof(down));
    down.type = INPUT_KEYBOARD;
    down.ki.wVk = static_cast<WORD>(vk);
    down.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    down.ki.dwFlags = 0;
    down.ki.dwExtraInfo = GetMessageExtraInfo();
    INPUT up = down;
    up.ki.dwFlags = KEYEVENTF_KEYUP;
    return send_input_pair(down, up);
}

bool inject_backspace() { return inject_vk(VK_BACK); }

// ---------------------------------------------------------------------------
//  Clipboard listener
// ---------------------------------------------------------------------------
bool clipboard_monitor_start(HWND msg_window) {
    if (!msg_window) return false;
    const BOOL ok = AddClipboardFormatListener(msg_window);
    if (!ok) app_log(L"AddClipboardFormatListener failed (%lu)", GetLastError());
    return ok != FALSE;
}

void clipboard_monitor_stop() {
    if (g_app.msg) RemoveClipboardFormatListener(g_app.msg);
}

// ---------------------------------------------------------------------------
//  Global hotkeys
// ---------------------------------------------------------------------------
namespace {

struct RegResult {
    bool ok = false;
    UINT err = 0;
    std::wstring spec;
    UINT mod = 0;
    UINT vk = 0;
};

UINT vk_from_char(char c) {
    // Virtual key codes for A-Z and 0-9 match their ASCII values.
    return static_cast<UINT>(static_cast<unsigned char>(c));
}

bool try_register(int id, const std::string& spec, RegResult* out, std::wstring* reason) {
    core::HotkeySpec parsed;
    std::string err;
    if (!core::parse_hotkey(spec, parsed, err)) {
        if (reason) *reason = L"unsupported";
        out->spec = std::wstring(spec.begin(), spec.end());
        return false;
    }
    UINT mod = MOD_NOREPEAT;
    if (parsed.ctrl) mod |= MOD_CONTROL;
    if (parsed.alt) mod |= MOD_ALT;
    if (parsed.shift) mod |= MOD_SHIFT;
    if (parsed.win) mod |= MOD_WIN;
    const UINT vk = vk_from_char(parsed.key);
    if (RegisterHotKey(g_app.msg, id, mod, vk)) {
        out->ok = true;
        out->spec = std::wstring(core::format_hotkey(parsed).begin(),
                                 core::format_hotkey(parsed).end());
        out->mod = mod;
        out->vk = vk;
        return true;
    }
    out->err = GetLastError();
    out->spec = std::wstring(core::format_hotkey(parsed).begin(),
                             core::format_hotkey(parsed).end());
    if (reason) {
        if (out->err == ERROR_HOTKEY_ALREADY_REGISTERED) *reason = L"conflict";
        else if (out->err == ERROR_ACCESS_DENIED) *reason = L"denied";
        else *reason = L"unsupported";
    }
    app_log(L"RegisterHotKey(%s) failed with %lu", out->spec.c_str(),
            static_cast<unsigned long>(out->err));
    return false;
}

}  // namespace

void app_unregister_hotkeys() {
    if (g_app.hotkey_clip_ok && g_app.msg) UnregisterHotKey(g_app.msg, kHotkeyClipboard);
    if (g_app.hotkey_kb_ok && g_app.msg) UnregisterHotKey(g_app.msg, kHotkeyKeyboard);
    g_app.hotkey_clip_ok = false;
    g_app.hotkey_kb_ok = false;
    g_app.hotkey_clip_active.clear();
    g_app.hotkey_kb_active.clear();
}

void app_register_hotkeys() {
    app_unregister_hotkeys();
    g_app.hotkey_clip_message.clear();
    g_app.hotkey_clip_reason.clear();

    if (!g_app.msg) return;
    g_app.hotkey_clip_requested = format_hotkey_display(g_app.st.clipboard_hotkey);

    if (g_app.st.clipboard_hotkey_enabled && g_app.st.clipboard_hotkey != "none") {
        std::vector<std::string> candidates;
        if (g_app.st.clipboard_hotkey == "auto") {
            candidates = core::auto_clipboard_candidates();
        } else {
            candidates.push_back(g_app.st.clipboard_hotkey);
            // If the preferred combination is taken (Win+V on Windows 10 1809+),
            // keep a working shortcut by falling back to Ctrl+Alt+V.
            if (g_app.st.clipboard_hotkey == "Win+V") candidates.push_back("Ctrl+Alt+V");
        }
        RegResult result;
        std::wstring first_fail_reason;
        std::wstring first_fail_spec;
        for (const std::string& spec : candidates) {
            RegResult r;
            std::wstring reason;
            if (try_register(kHotkeyClipboard, spec, &r, &reason)) {
                result = r;
                break;
            }
            if (first_fail_reason.empty()) {
                first_fail_reason = reason;
                first_fail_spec = r.spec;
            }
            result.err = r.err;
            result.spec = r.spec;
        }
        if (result.ok) {
            g_app.hotkey_clip_ok = true;
            g_app.hotkey_clip_mod = result.mod;
            g_app.hotkey_clip_vk = result.vk;
            g_app.hotkey_clip_active = result.spec;
            if (!first_fail_reason.empty()) {
                // Something was skipped: explain it rather than hiding it.
                g_app.hotkey_clip_reason = first_fail_reason;
                const bool was_win_v = first_fail_spec == L"Win+V";
                g_app.hotkey_clip_message =
                    was_win_v
                        ? (core::is_persian_ui(g_app.st)
                               ? L"Win+V توسط ویندوز گرفته شده است؛ میان‌بر جایگزین " + result.spec +
                                     L" ثبت شد."
                               : L"Win+V is owned by Windows; registered " + result.spec +
                                     L" instead.")
                        : (core::is_persian_ui(g_app.st)
                               ? first_fail_spec + L" در دسترس نبود؛ " + result.spec + L" ثبت شد."
                               : first_fail_spec + L" was unavailable; registered " + result.spec +
                                     L".");
                app_log(L"hotkey fallback: %s", g_app.hotkey_clip_message.c_str());
            }
        } else {
            g_app.hotkey_clip_ok = false;
            g_app.hotkey_clip_reason = first_fail_reason.empty() ? L"unsupported" : first_fail_reason;
            const std::string reason_ascii = core::ascii_from_u16(u16of(first_fail_reason));
            const std::string text =
                core::hotkey_failure_text(reason_ascii, core::is_persian_ui(g_app.st));
            g_app.hotkey_clip_message = std::wstring(text.begin(), text.end());
        }
    }

    if (g_app.st.keyboard_hotkey_enabled && g_app.st.keyboard_hotkey != "none") {
        RegResult r;
        std::wstring reason;
        if (try_register(kHotkeyKeyboard, g_app.st.keyboard_hotkey, &r, &reason)) {
            g_app.hotkey_kb_ok = true;
            g_app.hotkey_kb_active = r.spec;
        }
    }
    app_log(L"hotkeys: clipboard=%s keyboard=%s", g_app.hotkey_clip_active.c_str(),
            g_app.hotkey_kb_active.c_str());
}

// ---------------------------------------------------------------------------
//  Tray icon
// ---------------------------------------------------------------------------
namespace {
NOTIFYICONDATAW g_nid;
bool g_nid_valid = false;
HICON g_tray_icon = nullptr;
const UINT kTrayId = 1;

HICON load_app_icon(int size) {
    HICON icon = static_cast<HICON>(
        LoadImageW(g_app.inst, MAKEINTRESOURCEW(1), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
    if (!icon) icon = LoadIconW(nullptr, IDI_APPLICATION);
    return icon;
}

void fill_nid(NOTIFYICONDATAW* nid) {
    std::memset(nid, 0, sizeof(*nid));
    nid->cbSize = sizeof(NOTIFYICONDATAW);
    nid->hWnd = g_app.msg;
    nid->uID = kTrayId;
    nid->uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid->uCallbackMessage = WM_AMIN_TRAY;
    nid->hIcon = g_tray_icon ? g_tray_icon : load_app_icon(32);
    lstrcpynW(nid->szTip, kAppName, 127);
}
}  // namespace

bool tray_add(HWND msg_window) {
    if (!msg_window) return false;
    g_app.msg = msg_window;
    if (!g_tray_icon) g_tray_icon = load_app_icon(32);
    fill_nid(&g_nid);
    g_nid_valid = Shell_NotifyIconW(NIM_ADD, &g_nid) != FALSE;
    if (!g_nid_valid) {
        g_app.tray_error = win32_error_text(GetLastError());
        app_log(L"Shell_NotifyIcon(NIM_ADD) failed: %s", g_app.tray_error.c_str());
        return false;
    }
    // Keep the legacy callback semantics (lParam = mouse message).
    tray_update_tooltip();
    return true;
}

void tray_remove() {
    if (!g_nid_valid) return;
    NOTIFYICONDATAW nid;
    std::memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_app.msg;
    nid.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_nid_valid = false;
    if (g_tray_icon) {
        DestroyIcon(g_tray_icon);
        g_tray_icon = nullptr;
    }
}

void tray_restore_icon() {
    // Explorer restarted (TaskbarCreated): put the icon back.
    if (!g_app.st.show_tray_icon) return;
    g_nid_valid = false;
    tray_add(g_app.msg);
}

void tray_update_tooltip() {
    if (!g_nid_valid) return;
    NOTIFYICONDATAW nid;
    std::memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_app.msg;
    nid.uID = kTrayId;
    nid.uFlags = NIF_TIP;
    const std::wstring state =
        g_app.st.clipboard_monitoring ? (core::is_persian_ui(g_app.st) ? L"فعال" : L"on")
                                      : (core::is_persian_ui(g_app.st) ? L"متوقف" : L"paused");
    wchar_t tip[128];
    swprintf(tip, 128, L"%s\n%s: %s", kAppName,
             core::is_persian_ui(g_app.st) ? L"ثبت کلیپ‌بورد" : L"Capture", state.c_str());
    lstrcpynW(nid.szTip, tip, 127);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void tray_show_menu(HWND msg_window) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    const bool fa = core::is_persian_ui(g_app.st);
    AppendMenuW(menu, MF_STRING, 1001, fa ? L"پنل اصلی" : L"Dashboard");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 1002, fa ? L"صفحه کلید" : L"Keyboard");
    if (g_app.kb && IsWindowVisible(g_app.kb)) {
        CheckMenuItem(menu, 1002, MF_BYCOMMAND | MF_CHECKED);
    }
    AppendMenuW(menu, MF_STRING, 1003, fa ? L"کلیپ‌بورد" : L"Clipboard");
    if (g_app.clip && IsWindowVisible(g_app.clip)) {
        CheckMenuItem(menu, 1003, MF_BYCOMMAND | MF_CHECKED);
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 1004,
                fa ? L"توقف/ادامه ثبت کلیپ‌بورد" : L"Pause/resume clipboard capture");
    if (!g_app.st.clipboard_monitoring) CheckMenuItem(menu, 1004, MF_BYCOMMAND | MF_CHECKED);
    AppendMenuW(menu, MF_STRING, 1005, fa ? L"اجرا با ویندوز" : L"Start with Windows");
    if (startup_is_enabled()) CheckMenuItem(menu, 1005, MF_BYCOMMAND | MF_CHECKED);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 1006, fa ? L"خروج" : L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(msg_window);
    const UINT cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x,
                                    pt.y, 0, msg_window, nullptr);
    DestroyMenu(menu);
    switch (cmd) {
        case 1001: app_show_dashboard(true); break;
        case 1002: app_toggle_keyboard(); break;
        case 1003: app_toggle_clipboard(); break;
        case 1004: {
            g_app.st.clipboard_monitoring = !g_app.st.clipboard_monitoring;
            app_save_settings();
            app_set_status(g_app.st.clipboard_monitoring
                               ? (fa ? L"ثبت کلیپ‌بورد فعال شد" : L"Clipboard capture on")
                               : (fa ? L"ثبت کلیپ‌بورد متوقف شد" : L"Clipboard capture paused"),
                           g_app.st.clipboard_monitoring ? color::kSuccess : color::kWarn);
            tray_update_tooltip();
            app_refresh_ui();
            break;
        }
        case 1005: app_apply_startup(!startup_is_enabled()); break;
        case 1006: app_exit(); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
//  Startup entry (HKCU only - no administrator rights, ever)
// ---------------------------------------------------------------------------
namespace {
const wchar_t* const kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

std::wstring startup_command() {
    std::wstring cmd = L"\"";
    cmd += exe_path();
    cmd += L"\" --minimized";
    return cmd;
}
}  // namespace

bool startup_is_enabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    wchar_t buffer[1024];
    DWORD size = sizeof(buffer) - sizeof(wchar_t);
    DWORD type = 0;
    const LONG rc = RegQueryValueExW(key, kRunValueName, nullptr, &type,
                                     reinterpret_cast<BYTE*>(buffer), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return false;
    buffer[size / sizeof(wchar_t)] = 0;
    return buffer[0] != 0;
}

bool startup_set_enabled(bool enable, std::wstring* err) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_READ | KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        if (err) *err = win32_error_text(GetLastError());
        return false;
    }
    bool ok = false;
    if (enable) {
        const std::wstring cmd = startup_command();
        const DWORD bytes = static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t));
        ok = RegSetValueExW(key, kRunValueName, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(cmd.c_str()), bytes) == ERROR_SUCCESS;
        if (!ok && err) *err = win32_error_text(GetLastError());
    } else {
        const LONG rc = RegDeleteValueW(key, kRunValueName);
        ok = rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND;
        if (!ok && err) *err = win32_error_text(GetLastError());
    }
    RegCloseKey(key);
    app_log(L"startup_set_enabled(%d) -> %d", enable ? 1 : 0, ok ? 1 : 0);
    return ok;
}

// ---------------------------------------------------------------------------
//  Desktop shortcut (IShellLink). GUIDs are declared locally so no extra
//  import library is required by any compiler.
// ---------------------------------------------------------------------------
bool shell_create_desktop_shortcut(std::wstring* err) {
    static const GUID kClsidShellLink = {
        0x00021401, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
    static const GUID kIidShellLinkW = {
        0x000214F9, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
    static const GUID kIidPersistFile = {
        0x0000010B, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

    wchar_t desktop[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT,
                                desktop))) {
        if (err) *err = L"desktop folder not found";
        return false;
    }
    const std::wstring link_path = path_join(desktop, L"AMIN Keyboard & Clipboard.lnk");

    IShellLinkW* link = nullptr;
    HRESULT hr = CoCreateInstance(kClsidShellLink, nullptr, CLSCTX_INPROC_SERVER, kIidShellLinkW,
                                  reinterpret_cast<void**>(&link));
    if (FAILED(hr) || !link) {
        if (err) *err = L"CoCreateInstance(IShellLink) failed";
        return false;
    }
    const std::wstring exe = exe_path();
    link->SetPath(exe.c_str());
    link->SetWorkingDirectory(exe_dir().c_str());
    link->SetDescription(L"AMIN Keyboard & Clipboard");
    link->SetIconLocation(exe.c_str(), 0);
    link->SetArguments(L"");

    bool ok = false;
    IPersistFile* file = nullptr;
    if (SUCCEEDED(link->QueryInterface(kIidPersistFile, reinterpret_cast<void**>(&file))) && file) {
        ok = SUCCEEDED(file->Save(link_path.c_str(), TRUE));
        file->Release();
    }
    link->Release();
    if (!ok && err) *err = L"saving the .lnk file failed";
    app_log(L"desktop shortcut '%s' -> %d", link_path.c_str(), ok ? 1 : 0);

    // Verify that the file really exists; report honestly if not.
    if (ok && !file_exists(link_path)) ok = false;
    return ok;
}

}  // namespace amin
