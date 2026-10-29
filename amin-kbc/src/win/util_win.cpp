// =============================================================================
//  AMIN Keyboard & Clipboard - src/win/util_win.cpp
//  Paths, UTF-8 file IO, logging, clipboard access and the UI string table.
//  Only APIs that exist on Windows 7 / Windows 10 1803 are used.
// =============================================================================
#include "amin.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace amin {

L10n T;
L10n TE;

namespace {

std::wstring widen(const char* utf8) {
    if (!utf8) return std::wstring();
    const int len = static_cast<int>(std::strlen(utf8));
    if (len == 0) return std::wstring();
    const int need = MultiByteToWideChar(CP_UTF8, 0, utf8, len, nullptr, 0);
    if (need <= 0) return std::wstring();
    std::wstring out(static_cast<size_t>(need), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, len, &out[0], need);
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
//  Localized strings
// ---------------------------------------------------------------------------
void l10n_init() {
    // Persian (primary)
    T.show_keyboard = widen(u8"نمایش صفحه کلید");
    T.hide_keyboard = widen(u8"بستن صفحه کلید");
    T.hide = widen(u8"پنهان کردن");
    T.exit = widen(u8"خروج از برنامه");
    T.start_with_windows = widen(u8"اجرا با ویندوز");
    T.floating = widen(u8"شناور و همیشه روی پنجره‌ها");
    T.no_focus = widen(u8"حالت بدون فوکوس");
    T.preview_title = widen(u8"پیش‌نمایش متن ارسال‌شده");
    T.preview_hint = widen(u8"هر کلید به پنجره‌ای فرستاده می‌شود که قبل از باز شدن صفحه کلید فعال بوده است.");
    T.tab_keyboard = widen(u8"صفحه کلید");
    T.tab_clipboard = widen(u8"کلیپ‌بورد");
    T.search_hint = widen(u8"جستجو در تاریخچه…");
    T.copy = widen(u8"کپی");
    T.del = widen(u8"حذف");
    T.clear_all = widen(u8"پاک کردن همه");
    T.pause = widen(u8"توقف ثبت");
    T.resume = widen(u8"ادامه ثبت");
    T.monitoring_on = widen(u8"ثبت کلیپ‌بورد فعال است");
    T.monitoring_off = widen(u8"ثبت کلیپ‌بورد متوقف است");
    T.clipboard_title = widen(u8"تاریخچه کلیپ‌بورد");
    T.empty_history = widen(u8"هنوز متنی کپی نشده است");
    T.privacy = widen(u8"همه چیز فقط روی همین رایانه ذخیره می‌شود؛ هیچ داده‌ای به اینترنت فرستاده نمی‌شود.");
    T.status = widen(u8"وضعیت");
    T.shortcuts_hint = widen(u8"میان‌برها");
    T.settings = widen(u8"تنظیمات");
    T.run_setup = widen(u8"راه‌اندازی اولیه");
    T.next = widen(u8"بعدی");
    T.back = widen(u8"قبلی");
    T.finish = widen(u8"پایان");
    T.skip = widen(u8"رد کردن");
    T.startup_note = widen(u8"فقط یک مقدار در HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run ساخته می‌شود؛ نیازی به دسترسی مدیر نیست.");
    T.hotkey_label = widen(u8"میان‌بر باز کردن کلیپ‌بورد");
    T.open_clipboard = widen(u8"باز کردن کلیپ‌بورد");
    T.tick = L"\u2713";
    T.quick_actions = widen(u8"دسترسی سریع");
    T.sent_to = widen(u8"ارسال به");
    T.none = widen(u8"هیچ");
    T.version_label = widen(u8"نسخه");
    T.data_folder = widen(u8"پوشه داده‌ها");
    T.desktop_shortcut = widen(u8"ساخت میانبر روی دسکتاپ");
    T.create_shortcut = widen(u8"ساخت میانبر");
    T.copied = widen(u8"در کلیپ‌بورد کپی شد");
    T.deleted = widen(u8"حذف شد");
    T.cleared = widen(u8"تاریخچه پاک شد");
    T.paused_msg = widen(u8"ثبت کلیپ‌بورد متوقف شد");
    T.resumed_msg = widen(u8"ثبت کلیپ‌بورد ادامه یافت");
    T.confirm_clear = widen(u8"همه تاریخچه پاک شود؟ این کار برگشت‌پذیر نیست.");
    T.yes = widen(u8"بله");
    T.no = widen(u8"خیر");
    T.entry_count = widen(u8"مورد");
    T.chars = widen(u8"کاراکتر");
    T.error_title = widen(u8"خطا");
    T.info_title = widen(u8"اطلاعات");
    T.ready = widen(u8"آماده");
    T.hotkey_ok = widen(u8"فعال");
    T.hotkey_fail = widen(u8"غیرفعال");
    T.hotkey_win_v_note = widen(
        u8"ویندوز ۱۰ نسخه ۱۸۰۹ و جدیدتر کلید Win+V را برای تاریخچه خودش رزرو می‌کند. اگر این کلید را نتوانیم ثبت کنیم، "
        u8"به‌طور خودکار از Ctrl+Alt+V استفاده می‌شود.");
    T.installer_note = widen(u8"نسخه قابل حمل است و به نصب نیاز ندارد. میانبر و اجرا با ویندوز از داخل برنامه ساخته می‌شود.");
    T.diagnostics = widen(u8"خودآزمایی");
    T.selftest_run = widen(u8"اجرای خودآزمایی");

    // English (secondary)
    TE.show_keyboard = L"Show keyboard";
    TE.hide_keyboard = L"Hide keyboard";
    TE.hide = L"Hide";
    TE.exit = L"Exit application";
    TE.start_with_windows = L"Start with Windows";
    TE.floating = L"Floating, always on top";
    TE.no_focus = L"No-focus mode";
    TE.preview_title = L"Preview of the text being sent";
    TE.preview_hint =
        L"Keys are sent to the window that was active before the keyboard opened.";
    TE.tab_keyboard = L"Keyboard";
    TE.tab_clipboard = L"Clipboard";
    TE.search_hint = L"Search history...";
    TE.copy = L"Copy";
    TE.del = L"Delete";
    TE.clear_all = L"Clear all";
    TE.pause = L"Pause capture";
    TE.resume = L"Resume capture";
    TE.monitoring_on = L"Clipboard capture is on";
    TE.monitoring_off = L"Clipboard capture is paused";
    TE.clipboard_title = L"Clipboard history";
    TE.empty_history = L"Nothing copied yet";
    TE.privacy = L"Everything stays on this PC. Nothing is ever uploaded.";
    TE.status = L"Status";
    TE.shortcuts_hint = L"Shortcuts";
    TE.settings = L"Settings";
    TE.run_setup = L"Setup wizard";
    TE.next = L"Next";
    TE.back = L"Back";
    TE.finish = L"Finish";
    TE.skip = L"Skip";
    TE.startup_note =
        L"Only one value is written to HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run. "
        L"No administrator rights are needed.";
    TE.hotkey_label = L"Open clipboard shortcut";
    TE.open_clipboard = L"Open clipboard";
    TE.tick = L"\u2713";
    TE.quick_actions = L"Quick actions";
    TE.sent_to = L"Sending to";
    TE.none = L"none";
    TE.version_label = L"Version";
    TE.data_folder = L"Data folder";
    TE.desktop_shortcut = L"Create a desktop shortcut";
    TE.create_shortcut = L"Create shortcut";
    TE.copied = L"Copied to clipboard";
    TE.deleted = L"Deleted";
    TE.cleared = L"History cleared";
    TE.paused_msg = L"Clipboard capture paused";
    TE.resumed_msg = L"Clipboard capture resumed";
    TE.confirm_clear = L"Clear the whole history? This cannot be undone.";
    TE.yes = L"Yes";
    TE.no = L"No";
    TE.entry_count = L"entries";
    TE.chars = L"characters";
    TE.error_title = L"Error";
    TE.info_title = L"Information";
    TE.ready = L"Ready";
    TE.hotkey_ok = L"active";
    TE.hotkey_fail = L"disabled";
    TE.hotkey_win_v_note =
        L"Windows 10 1809 and newer reserve Win+V for its own clipboard history. If that "
        L"shortcut cannot be registered, Ctrl+Alt+V is used automatically.";
    TE.installer_note =
        L"This build is portable and needs no installation. The shortcut and the startup "
        L"entry are created from inside the app.";
    TE.diagnostics = L"Self test";
    TE.selftest_run = L"Run self test";
}

// ---------------------------------------------------------------------------
//  Error text / logging
// ---------------------------------------------------------------------------
std::wstring win32_error_text(DWORD code) {
    if (code == 0) return std::wstring();
    wchar_t* buffer = nullptr;
    const DWORD n = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring out;
    if (n && buffer) {
        out.assign(buffer, n);
        while (!out.empty() && (out.back() == L'\r' || out.back() == L'\n' || out.back() == L' '))
            out.pop_back();
    }
    LocalFree(buffer);
    if (out.empty()) {
        wchar_t buf[64];
        swprintf(buf, 64, L"error %lu", static_cast<unsigned long>(code));
        out = buf;
    }
    return out;
}

void app_log(const wchar_t* fmt, ...) {
    if (g_app.log_path.empty()) return;
    // Keep the log small: rotate once at 512 KB.
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(g_app.log_path.c_str(), GetFileExInfoStandard, &fad)) {
        const ULONGLONG size = (static_cast<ULONGLONG>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
        if (size > 512 * 1024) {
            std::wstring old = g_app.log_path + L".old";
            MoveFileExW(g_app.log_path.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
    }
    wchar_t line[2048];
    SYSTEMTIME st;
    GetLocalTime(&st);
    int n = swprintf(line, 2048, L"%04d-%02d-%02d %02d:%02d:%02d  ", st.wYear, st.wMonth, st.wDay,
                     st.wHour, st.wMinute, st.wSecond);
    if (n < 0) return;
    va_list args;
    va_start(args, fmt);
    int m = vswprintf(line + n, static_cast<size_t>(2048 - n), fmt, args);
    va_end(args);
    if (m < 0) return;
    n += m;
    line[n++] = L'\r';
    line[n++] = L'\n';
    line[n] = 0;
    const std::string utf8_line = utf8(std::wstring(line, static_cast<size_t>(n)));
    HANDLE h = CreateFileW(g_app.log_path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, utf8_line.data(), static_cast<DWORD>(utf8_line.size()), &written, nullptr);
    CloseHandle(h);
}

// ---------------------------------------------------------------------------
//  Paths
// ---------------------------------------------------------------------------
std::wstring path_join(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    std::wstring out = a;
    if (out.back() != L'\\' && out.back() != L'/') out.push_back(L'\\');
    out += b;
    return out;
}

std::wstring exe_path() {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(),
                                          static_cast<DWORD>(buffer.size()));
        if (n == 0) return std::wstring();
        if (n < buffer.size() - 1) return std::wstring(buffer.data(), n);
        buffer.resize(buffer.size() * 2);
        if (buffer.size() > 32768) return std::wstring();
    }
}

std::wstring exe_dir() {
    std::wstring path = exe_path();
    const size_t pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? std::wstring() : path.substr(0, pos);
}

bool ensure_directory(const std::wstring& path) {
    if (path.empty()) return false;
    const int hr = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    return hr == ERROR_SUCCESS || hr == ERROR_ALREADY_EXISTS || hr == ERROR_FILE_EXISTS;
}

bool file_exists(const std::wstring& path) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool file_write_utf8(const std::wstring& path, const std::string& data, std::wstring* err) {
    const std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        if (err) *err = win32_error_text(GetLastError());
        return false;
    }
    DWORD written = 0;
    bool ok = true;
    if (!data.empty()) {
        ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) != 0 &&
             written == data.size();
    }
    FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok) {
        if (err) *err = win32_error_text(GetLastError());
        DeleteFileW(tmp.c_str());
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        if (err) *err = win32_error_text(GetLastError());
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

bool file_read_utf8(const std::wstring& path, std::string* out, std::wstring* err) {
    if (!out) return false;
    out->clear();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        const DWORD e = GetLastError();
        if (err) *err = win32_error_text(e);
        return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND;  // not an error
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) {
        if (err) *err = win32_error_text(GetLastError());
        CloseHandle(h);
        return false;
    }
    if (size.QuadPart < 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(h);
        if (err) *err = L"file too large";
        return false;
    }
    out->resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    bool ok = true;
    if (size.QuadPart > 0) {
        ok = ReadFile(h, &(*out)[0], static_cast<DWORD>(out->size()), &read, nullptr) != 0;
        out->resize(read);
    }
    CloseHandle(h);
    return ok;
}

void message_box(HWND owner, const std::wstring& text, const std::wstring& caption, UINT flags) {
    // Ensure the box is visible even when our window is topmost.
    if (owner && IsWindowVisible(owner)) {
        SetForegroundWindow(owner);
    }
    MessageBoxW(owner, text.c_str(), caption.c_str(), flags | MB_SETFOREGROUND);
}

// ---------------------------------------------------------------------------
//  Clipboard (text only!)
// ---------------------------------------------------------------------------
bool clipboard_open_with_retry(HWND owner, int attempts) {
    for (int i = 0; i < attempts; ++i) {
        if (OpenClipboard(owner)) return true;
        Sleep(15);
    }
    return false;
}

std::wstring clipboard_get_text() {
    if (!clipboard_open_with_retry(g_app.msg, 10)) {
        app_log(L"clipboard: OpenClipboard failed (%lu)", GetLastError());
        return std::wstring();
    }
    std::wstring result;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data) {
        const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(data));
        if (p) {
            size_t len = 0;
            // Clipboard data is NUL terminated, but be defensive about missing NULs.
            while (len < 32u * 1024u * 1024u && p[len] != 0) ++len;
            result.assign(p, len);
            GlobalUnlock(data);
        }
    } else {
        // Fall back to ANSI text so older applications are still captured.
        HANDLE ansi = GetClipboardData(CF_TEXT);
        if (ansi) {
            const char* p = static_cast<const char*>(GlobalLock(ansi));
            if (p) {
                const int need = MultiByteToWideChar(CP_ACP, 0, p, -1, nullptr, 0);
                if (need > 1) {
                    std::wstring wide(static_cast<size_t>(need - 1), L'\0');
                    MultiByteToWideChar(CP_ACP, 0, p, -1, &wide[0], need);
                    result = wide;
                }
                GlobalUnlock(ansi);
            }
        }
    }
    CloseClipboard();
    return result;
}

bool clipboard_set_text(HWND owner, const std::wstring& text) {
    // Ignore the WM_CLIPBOARDUPDATE that this write will generate.
    g_app.clipboard_suppress_until = GetTickCount64() + 700;
    if (!clipboard_open_with_retry(owner, 10)) {
        app_log(L"clipboard: OpenClipboard for write failed (%lu)", GetLastError());
        return false;
    }
    bool ok = false;
    if (EmptyClipboard()) {
        const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
        if (mem) {
            void* p = GlobalLock(mem);
            if (p) {
                std::memcpy(p, text.c_str(), bytes);
                GlobalUnlock(mem);
                if (SetClipboardData(CF_UNICODETEXT, mem)) {
                    ok = true;
                    mem = nullptr;  // ownership transferred
                }
            }
            if (mem) GlobalFree(mem);
        }
        // Mark entries written by us (used by diagnostics; harmless otherwise).
        const UINT fmt = RegisterClipboardFormatW(L"AMIN.KBC.Text");
        if (fmt) {
            HGLOBAL mark = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
            if (mark) {
                void* p = GlobalLock(mark);
                if (p) {
                    *static_cast<DWORD*>(p) = 1;
                    GlobalUnlock(mark);
                    SetClipboardData(fmt, mark);
                }
            }
        }
    }
    CloseClipboard();
    return ok;
}

std::wstring format_hotkey_display(const std::string& spec) {
    if (spec == "auto") return L"auto";
    if (spec == "none") return L"-";
    core::HotkeySpec parsed;
    std::string err;
    if (core::parse_hotkey(spec, parsed, err)) {
        const std::string formatted = core::format_hotkey(parsed);
        return std::wstring(formatted.begin(), formatted.end());
    }
    return std::wstring(spec.begin(), spec.end());
}

void shell_open_data_folder() {
    if (g_app.data_dir.empty()) return;
    ShellExecuteW(nullptr, L"open", g_app.data_dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void shell_open_url(const wchar_t* url) {
    if (!url || !*url) return;
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

bool run_at_startup_supported() { return true; }

}  // namespace amin
