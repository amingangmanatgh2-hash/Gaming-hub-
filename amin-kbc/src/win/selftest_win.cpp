// =============================================================================
//  AMIN Keyboard & Clipboard - src/win/selftest_win.cpp
//  Runs on the user's machine (--selftest). It checks the platform independent
//  core suite plus the Windows features this app depends on, and writes an
//  honest report: PASS / FAIL / SKIP for every item.
// =============================================================================
#include "amin.h"

#include <cstdio>
#include <cstring>

namespace amin {
namespace {

struct Report {
    std::wstring text;
    int passed = 0;
    int failed = 0;
    int skipped = 0;

    void pass(const std::wstring& what) {
        ++passed;
        text += L"[PASS] " + what + L"\n";
    }
    void fail(const std::wstring& what, const std::wstring& detail = std::wstring()) {
        ++failed;
        text += L"[FAIL] " + what;
        if (!detail.empty()) text += L"  (" + detail + L")";
        text += L"\n";
    }
    void skip(const std::wstring& what, const std::wstring& why) {
        ++skipped;
        text += L"[SKIP] " + what + L"  (" + why + L")\n";
    }
    void info(const std::wstring& what) { text += L"       " + what + L"\n"; }
    void section(const std::wstring& what) { text += L"\n== " + what + L" ==\n"; }
};

std::wstring os_description(DWORD* build_out, DWORD* major_out, DWORD* minor_out) {
    typedef LONG(WINAPI * RtlGetVersionPtr)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    OSVERSIONINFOW osv;
    std::memset(&osv, 0, sizeof(osv));
    osv.dwOSVersionInfoSize = sizeof(osv);
    bool ok = false;
    if (ntdll) {
        const auto fn = reinterpret_cast<RtlGetVersionPtr>(
            reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
        if (fn && fn(&osv) == 0) ok = true;
    }
    if (!ok) {
        osv.dwMajorVersion = 0;
        osv.dwMinorVersion = 0;
        osv.dwBuildNumber = 0;
    }
    if (build_out) *build_out = osv.dwBuildNumber;
    if (major_out) *major_out = osv.dwMajorVersion;
    if (minor_out) *minor_out = osv.dwMinorVersion;
    wchar_t buf[160];
    swprintf(buf, 160, L"Windows %lu.%lu build %lu", static_cast<unsigned long>(osv.dwMajorVersion),
             static_cast<unsigned long>(osv.dwMinorVersion),
             static_cast<unsigned long>(osv.dwBuildNumber));
    return std::wstring(buf) + (osv.szCSDVersion[0] ? L" " + std::wstring(osv.szCSDVersion)
                                                   : std::wstring());
}

bool font_available(const wchar_t* face, int px) {
    HFONT f = CreateFontW(-px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                          DEFAULT_PITCH | FF_DONTCARE, face);
    if (!f) return false;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(dc, f);
    wchar_t actual[64] = {0};
    GetTextFaceW(dc, 64, actual);
    SelectObject(dc, old);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    DeleteObject(f);
    return _wcsicmp(actual, face) == 0;
}

}  // namespace

int selftest_run(bool console, std::wstring* report_out) {
    Report r;
    r.text = std::wstring(L"AMIN Keyboard & Clipboard ") + kAppVersion + L" - self test\n";
    r.text += L"Generated: \n";

    DWORD build = 0, major = 0, minor = 0;
    const std::wstring os = os_description(&build, &major, &minor);
    r.section(L"System");
    r.info(os);
    r.info(std::wstring(L"Executable: ") + exe_path());
    r.info(std::wstring(L"Data folder: ") + g_app.data_dir);
    wchar_t buf[256];
    swprintf(buf, 256, L"DPI: %d (scale %.2f)", g_app.dpi, static_cast<double>(g_app.scale));
    r.info(buf);
    if (major >= 10 && build >= 17134) {
        r.pass(L"Operating system is Windows 10 1803 or newer (the tested target)");
    } else if (major >= 10) {
        r.pass(L"Windows 10 detected (older than 1803; core APIs are still Win7-compatible)");
    } else if (major >= 6) {
        r.pass(L"Windows Vista/7/8.x detected (supported API level)");
    } else {
        r.skip(L"Very old Windows detected", L"not a supported target");
    }

    // ---- Core suite (same code that is unit tested on the build host) --------
    r.section(L"Core logic (text, JSON, store, settings, hotkeys, layouts)");
    const core::TestSummary core_result = core::run_core_selftests();
    // Fold the core report in line by line so failures are visible here too.
    size_t pos = 0;
    while (pos < core_result.log.size()) {
        const size_t nl = core_result.log.find('\n', pos);
        const std::string line =
            core_result.log.substr(pos, (nl == std::string::npos) ? std::string::npos : nl - pos);
        if (line.size() >= 6 && line.compare(0, 6, "[FAIL]") == 0) {
            r.text += L"       " + std::wstring(line.begin(), line.end()) + L"\n";
        }
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    if (core_result.failed == 0) {
        swprintf(buf, 256, L"%d core checks passed", core_result.passed);
        r.pass(buf);
    } else {
        swprintf(buf, 256, L"%d core checks failed", core_result.failed);
        r.fail(buf);
    }

    // ---- Loadable modules ---------------------------------------------------
    r.section(L"Modules and fonts");
    HMODULE usp10 = LoadLibraryW(L"usp10.dll");
    const bool has_uniscribe =
        usp10 && GetProcAddress(usp10, "ScriptStringAnalyse") && GetProcAddress(usp10, "ScriptStringOut");
    if (has_uniscribe) {
        r.pass(L"Uniscribe (usp10.dll) available - Persian/Arabic shaping works");
    } else {
        r.fail(L"Uniscribe (usp10.dll) missing", L"Persian text would not render shaped");
    }
    if (usp10) FreeLibrary(usp10);
    if (font_available(L"Tahoma", 16)) {
        r.pass(L"Font Tahoma installed (Persian glyphs)");
    } else {
        r.fail(L"Font Tahoma missing");
    }
    if (font_available(L"Segoe UI", 16)) {
        r.pass(L"Font Segoe UI installed (Latin UI)");
    } else {
        r.skip(L"Font Segoe UI missing", L"Arial fallback is used");
    }

    // ---- File persistence ---------------------------------------------------
    r.section(L"Local storage (nothing is uploaded)");
    if (g_app.data_writable) {
        const std::wstring probe = path_join(g_app.data_dir, L"selftest-probe.json");
        const std::string payload = "{\"app\":\"test\",\"text\":\"\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85\"}";
        std::wstring err;
        bool ok = file_write_utf8(probe, payload, &err);
        std::string readback;
        if (ok) ok = file_read_utf8(probe, &readback, &err) && readback == payload;
        DeleteFileW(probe.c_str());
        if (ok) {
            r.pass(L"UTF-8 write/read round trip in the data folder");
        } else {
            r.fail(L"Data folder round trip", err);
        }
    } else {
        r.fail(L"Data folder is not writable", g_app.data_error);
    }

    // ---- Windows / message plumbing ----------------------------------------
    r.section(L"Windows integration");
    const bool msg_class = message_window_register(g_app.inst);
    HWND msgw = msg_class ? message_window_create(g_app.inst) : nullptr;
    if (msgw) {
        r.pass(L"Hidden message window created");
        g_app.msg = msgw;
    } else {
        r.fail(L"Hidden message window could not be created");
    }

    if (msgw) {
        const bool listener = AddClipboardFormatListener(msgw) != FALSE;
        if (listener) {
            r.pass(L"AddClipboardFormatListener works (live clipboard capture possible)");
            RemoveClipboardFormatListener(msgw);
        } else {
            r.fail(L"AddClipboardFormatListener failed", win32_error_text(GetLastError()));
        }
    }

    // Clipboard text round trip.
    {
        const std::wstring old_text = clipboard_get_text();
        const std::wstring test_text =
            L"AMIN clipboard test \u0633\u0644\u0627\u0645 12345";
        std::wstring err;
        bool ok = clipboard_set_text(msgw, test_text);
        std::wstring readback = ok ? clipboard_get_text() : std::wstring();
        ok = ok && readback == test_text;
        if (!old_text.empty()) clipboard_set_text(msgw, old_text);
        if (ok) {
            r.pass(L"Clipboard text write/read round trip (original content restored)");
        } else {
            r.fail(L"Clipboard round trip");
        }
        // Restore an empty clipboard if there was nothing before.
        if (old_text.empty() && OpenClipboard(msgw)) {
            EmptyClipboard();
            CloseClipboard();
        }
    }

    // SendInput injection into a real edit control.
    if (msgw) {
        HWND probe = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"", WS_POPUP, -3000, -3000, 260,
                                     90, nullptr, nullptr, g_app.inst, nullptr);
        HWND edit = nullptr;
        if (probe) {
            edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 4, 4,
                                   240, 26, probe, nullptr, g_app.inst, nullptr);
        }
        if (probe && edit) {
            const HWND previous = GetForegroundWindow();
            ShowWindow(probe, SW_SHOWNOACTIVATE);
            SetForegroundWindow(probe);
            Sleep(120);
            if (GetForegroundWindow() == probe) {
                SetFocus(edit);
                const wchar_t test_char = 0x0633;  // س
                inject_char(test_char);
                Sleep(150);
                wchar_t got[8] = {0};
                GetWindowTextW(edit, got, 7);
                if (got[0] == test_char) {
                    r.pass(L"SendInput Unicode injection reaches another window (Persian letter)");
                } else {
                    r.fail(L"SendInput injection did not deliver the character");
                }
            } else {
                r.skip(L"SendInput injection test",
                       L"Windows refused to give this process the foreground");
            }
            if (previous) SetForegroundWindow(previous);
            DestroyWindow(probe);
        } else {
            r.fail(L"Could not create the injection test window");
            if (probe) DestroyWindow(probe);
        }
    }

    // Global shortcuts: report exactly which combination can be registered.
    {
        const std::vector<std::string> candidates = core::auto_clipboard_candidates();
        bool any = false;
        for (const std::string& spec : candidates) {
            core::HotkeySpec parsed;
            std::string err;
            if (!core::parse_hotkey(spec, parsed, err)) continue;
            UINT mod = MOD_NOREPEAT;
            if (parsed.ctrl) mod |= MOD_CONTROL;
            if (parsed.alt) mod |= MOD_ALT;
            if (parsed.shift) mod |= MOD_SHIFT;
            if (parsed.win) mod |= MOD_WIN;
            const UINT vk = static_cast<UINT>(static_cast<unsigned char>(parsed.key));
            const bool ok = RegisterHotKey(msgw, 99, mod, vk) != FALSE;
            const DWORD reg_err = GetLastError();
            if (ok) {
                UnregisterHotKey(msgw, 99);
                any = true;
                r.pass(L"Global shortcut available: " +
                       std::wstring(spec.begin(), spec.end()));
            } else if (reg_err == ERROR_HOTKEY_ALREADY_REGISTERED) {
                r.info(L"Shortcut already in use by another app: " +
                       std::wstring(spec.begin(), spec.end()));
            } else {
                r.info(L"Shortcut refused: " + std::wstring(spec.begin(), spec.end()) + L" (" +
                       win32_error_text(reg_err) + L")");
            }
        }
        if (!any) {
            r.fail(L"No global shortcut could be registered",
                   L"another application holds all candidates");
        }
    }

    // Startup entry (written, verified, then restored to the previous state).
    {
        const bool before = startup_is_enabled();
        std::wstring err;
        const bool wrote = startup_set_enabled(true, &err);
        const bool readback = wrote && startup_is_enabled();
        std::wstring restore_err;
        const bool restored = startup_set_enabled(before, &restore_err);
        if (wrote && readback) {
            r.pass(L"Start with Windows entry can be written and read (HKCU, no admin)");
        } else {
            r.fail(L"Start with Windows entry failed", err);
        }
        if (!restored) r.info(L"Could not restore the previous startup state");
    }

    // Tray icon (added and removed again).
    if (msgw) {
        if (tray_add(msgw)) {
            r.pass(L"Tray icon can be added");
            tray_remove();
        } else {
            r.skip(L"Tray icon", g_app.tray_error.empty() ? L"not available" : g_app.tray_error);
        }
    }

    // COM / IShellLink for the desktop shortcut.
    {
        static const GUID kClsidShellLink = {
            0x00021401, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
        static const GUID kIidShellLinkW = {
            0x000214F9, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
        IShellLinkW* link = nullptr;
        const HRESULT hr = CoCreateInstance(kClsidShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                            kIidShellLinkW, reinterpret_cast<void**>(&link));
        if (SUCCEEDED(hr) && link) {
            link->Release();
            r.pass(L"IShellLink available (desktop shortcut can be created)");
        } else {
            r.fail(L"IShellLink unavailable (COM not initialised?)");
        }
    }

    r.section(L"Result");
    swprintf(buf, 256, L"%d passed, %d failed, %d skipped", r.passed, r.failed, r.skipped);
    r.text += std::wstring(buf) + L"\n";
    r.text += L"Note: this test writes a probe file and briefly touches the clipboard and the "
              L"HKCU startup value; everything is restored afterwards.\n";

    // ---- Output -------------------------------------------------------------
    const std::wstring path = path_join(g_app.data_dir, L"selftest.log");
    std::wstring err;
    if (!file_write_utf8(path, utf8(r.text), &err)) {
        r.text += L"\n(could not write " + path + L": " + err + L")\n";
    }
    if (report_out) *report_out = r.text;

    if (console) {
        // Attach to the launching console if there is one; never allocate a new
        // console window just for this.
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* out = nullptr;
            if (freopen_s(&out, "CONOUT$", "w", stdout) == 0 && out) {
                std::fputws(L"\n", stdout);
                std::fputws(r.text.c_str(), stdout);
                std::fputws(L"\n", stdout);
                std::fflush(stdout);
            }
        }
    }
    app_log(L"selftest finished: %d passed, %d failed, %d skipped", r.passed, r.failed, r.skipped);
    return r.failed;
}

}  // namespace amin
