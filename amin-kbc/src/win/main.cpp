// =============================================================================
//  AMIN Keyboard & Clipboard - src/win/main.cpp
//  Entry point: single instance handling, DPI, fonts, window creation, message
//  loop and clean shutdown.
// =============================================================================
#include "amin.h"

#include <objbase.h>

#include <cstring>

namespace amin {

void init_common_controls() {
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);
}

namespace {

std::wstring lower_copy(std::wstring s) {
    for (wchar_t& c : s) {
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    return s;
}

// Exact (case-insensitive) token match, so a path such as C:\\amin-hub\\x.exe
// can never be mistaken for the "-h" switch.
bool has_arg(const std::wstring& cmdline, const wchar_t* arg) {
    const std::wstring lower = lower_copy(cmdline);
    const std::wstring needle = lower_copy(arg);
    size_t pos = 0;
    while (pos <= lower.size()) {
        size_t end = lower.find(L' ', pos);
        if (end == std::wstring::npos) end = lower.size();
        std::wstring token = lower.substr(pos, end - pos);
        // Strip surrounding quotes.
        while (!token.empty() && token.front() == L'"') token.erase(token.begin());
        while (!token.empty() && token.back() == L'"') token.pop_back();
        if (token == needle) return true;
        if (end == lower.size()) break;
        pos = end + 1;
    }
    return false;
}

void show_help() {
    const std::wstring text =
        L"AMIN Keyboard & Clipboard " + std::wstring(kAppVersion) + L"\n\n"
        L"Usage:\n"
        L"  AminKeyboardClipboard.exe              open the dashboard\n"
        L"  AminKeyboardClipboard.exe --minimized  start hidden in the tray\n"
        L"  AminKeyboardClipboard.exe --selftest   run the built-in self test\n"
        L"  AminKeyboardClipboard.exe --version    show the version\n\n"
        L"\u0645\u0627\u0645\u0648\u0631 \u062e\u0637: \u0628\u0631\u0646\u0627\u0645\u0647 "
        L"\u0628\u062f\u0648\u0646 \u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0646\u06cc\u0632 "
        L"\u0627\u062c\u0631\u0627 \u0645\u06cc\u200c\u0634\u0648\u062f\u061b "
        L"\u062f\u0627\u062f\u0647\u200c\u0647\u0627 \u062f\u0631 "
        L"%APPDATA%\\AminKeyboardClipboard \u0630\u062e\u06cc\u0631\u0647 "
        L"\u0645\u06cc\u200c\u0634\u0648\u0646\u062f.";
    message_box(nullptr, text, kAppName, MB_ICONINFORMATION | MB_OK);
}

void log_system_info() {
    app_log(L"--- AMIN Keyboard & Clipboard %s starting ---", kAppVersion);
    typedef LONG(WINAPI * RtlGetVersionPtr)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        const auto fn = reinterpret_cast<RtlGetVersionPtr>(
            reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
        OSVERSIONINFOW osv;
        std::memset(&osv, 0, sizeof(osv));
        osv.dwOSVersionInfoSize = sizeof(osv);
        if (fn && fn(&osv) == 0) {
            app_log(L"OS %lu.%lu build %lu", static_cast<unsigned long>(osv.dwMajorVersion),
                    static_cast<unsigned long>(osv.dwMinorVersion),
                    static_cast<unsigned long>(osv.dwBuildNumber));
        }
    }
    app_log(L"exe: %s", exe_path().c_str());
    app_log(L"data: %s (writable=%d)", g_app.data_dir.c_str(), g_app.data_writable ? 1 : 0);
    app_log(L"dpi=%d scale=%.2f", g_app.dpi, static_cast<double>(g_app.scale));
}

}  // namespace

int run_main(HINSTANCE inst, PWSTR cmdline, int show_cmd) {
    g_app.inst = inst;
    (void)show_cmd;
    l10n_init();
    init_common_controls();
    app_init_paths();

    const std::wstring cmd = cmdline ? std::wstring(cmdline) : std::wstring();

    if (has_arg(cmd, L"--help") || has_arg(cmd, L"-h")) {
        show_help();
        return 0;
    }
    if (has_arg(cmd, L"--version")) {
        message_box(nullptr, std::wstring(kAppName) + L"\n" + kAppVersion, kAppName,
                    MB_ICONINFORMATION | MB_OK);
        return 0;
    }
    if (has_arg(cmd, L"--selftest")) {
        return selftest_run(true) == 0 ? 0 : 1;
    }

    g_app.StartHidden = has_arg(cmd, L"--minimized");

    // ---- single instance ----------------------------------------------------
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(kClassMessage, nullptr);
        if (existing && !g_app.StartHidden) {
            PostMessageW(existing, WM_AMIN_SHOW_DASHBOARD, 0, 0);
        }
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    // ---- DPI + fonts --------------------------------------------------------
    SetProcessDPIAware();
    HDC screen = GetDC(nullptr);
    g_app.dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
    if (screen) ReleaseDC(nullptr, screen);
    if (g_app.dpi < 96) g_app.dpi = 96;
    g_app.scale = static_cast<float>(g_app.dpi) / 96.0f;
    if (!fonts_init(g_app.dpi)) {
        message_box(nullptr, L"GDI font creation failed.", L"AMIN", MB_ICONERROR | MB_OK);
        return 1;
    }

    // ---- settings / history -------------------------------------------------
    app_load_settings();
    if (!app_load_history()) {
        app_set_status(core::is_persian_ui(g_app.st)
                           ? L"تاریخچه کلیپ‌بورد خوانا نبود و از نو ساخته شد"
                           : L"Clipboard history was unreadable; starting fresh",
                       color::kWarn);
    }
    log_system_info();

    if (!g_app.data_writable && !g_app.StartHidden) {
        message_box(nullptr,
                    (core::is_persian_ui(g_app.st)
                         ? L"پوشه ذخیره‌سازی در دسترس نیست:\n"
                         : L"The data folder is not writable:\n") +
                        g_app.data_dir + L"\n" + g_app.data_error +
                        (core::is_persian_ui(g_app.st)
                             ? L"\n\nبرنامه اجرا می‌شود اما تنظیمات و تاریخچه ذخیره نمی‌شوند."
                             : L"\n\nThe app runs, but settings and history cannot be saved."),
                    T.error_title, MB_ICONWARNING | MB_OK);
    }

    // ---- windows ------------------------------------------------------------
    if (!message_window_register(inst) || !dashboard_register(inst) ||
        !keyboard_register(inst) || !clipboard_register(inst)) {
        message_box(nullptr, L"Window class registration failed.", L"AMIN",
                    MB_ICONERROR | MB_OK);
        return 1;
    }

    g_app.msg = message_window_create(inst);
    if (!g_app.msg) {
        message_box(nullptr, L"Could not create the message window.", L"AMIN",
                    MB_ICONERROR | MB_OK);
        return 1;
    }

    g_app.clipboard_listener_ok = clipboard_monitor_start(g_app.msg);
    if (!g_app.clipboard_listener_ok) {
        app_set_status(core::is_persian_ui(g_app.st)
                           ? L"ثبت خودکار کلیپ‌بورد فعال نشد"
                           : L"Automatic clipboard capture could not be enabled",
                       color::kDanger);
    }

    app_register_hotkeys();

    if (g_app.st.show_tray_icon) {
        g_app.tray_ok = tray_add(g_app.msg);
    }
    if (g_app.StartHidden && !g_app.tray_ok) {
        // Started hidden but there is no tray icon to get back from: show the
        // dashboard instead of leaving the user with an invisible application.
        g_app.StartHidden = false;
        message_box(nullptr,
                    core::is_persian_ui(g_app.st)
                        ? L"آیکون سینی ساخته نشد، بنابراین برنامه به‌جای حالت پنهان، پنجره اصلی را "
                          L"باز می‌کند."
                        : L"The tray icon could not be created, so the main window is shown "
                          L"instead of starting hidden.",
                    T.error_title, MB_ICONWARNING | MB_OK);
    }

    // Foreground tracking keeps the "send keys here" target up to date.
    SetTimer(g_app.msg, TIMER_TRACK, 500, nullptr);

    if (g_app.StartHidden) {
        app_log(L"started minimized");
    } else {
        app_show_dashboard(true);
        if (!g_app.st.wizard_done) {
            g_app.wizard_open = true;
            g_app.wizard_step = 0;
        }
    }
    if (g_app.st.keyboard_language != g_app.kb_lang) g_app.kb_lang = g_app.st.keyboard_language;
    app_refresh_ui();

    // ---- message loop -------------------------------------------------------
    MSG msg;
    std::memset(&msg, 0, sizeof(msg));
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.hwnd && IsDialogMessageW(msg.hwnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // ---- shutdown -----------------------------------------------------------
    app_log(L"message loop ended (wparam=%lld)", static_cast<long long>(msg.wParam));
    if (g_app.store_dirty) app_save_history(true);
    app_save_settings(true);
    fonts_free();
    if (mutex) CloseHandle(mutex);
    return static_cast<int>(msg.wParam);
}

}  // namespace amin

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show_cmd) {
    (void)prev;
    // COM is needed for IShellLink (desktop shortcut creation).
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const int result = amin::run_main(inst, cmdline, show_cmd);
    if (SUCCEEDED(hr)) CoUninitialize();
    return result;
}
