// =============================================================================
//  AMIN Keyboard & Clipboard - src/win/amin.h
//  Shared declarations for the Windows (Win32) layer.
//
//  Compatibility target: Windows 10 (including 1803 / build 17134) and Windows
//  7 SP1. _WIN32_WINNT is pinned to 0x0601 on purpose: the compiler then
//  rejects any API that does not exist on those systems, so new-Windows-only
//  calls cannot slip in unnoticed.
// =============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <usp10.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "../core/hotkey.h"
#include "../core/layout.h"
#include "../core/selftest_core.h"
#include "../core/settings.h"
#include "../core/store.h"

namespace amin {

// ---------------------------------------------------------------------------
//  Application identity
// ---------------------------------------------------------------------------
extern const wchar_t* const kAppName;        // "AMIN Keyboard & Clipboard"
extern const wchar_t* const kAppNameFa;      // Persian name
extern const wchar_t* const kAppVersion;     // "1.0.0"
extern const wchar_t* const kMutexName;
extern const wchar_t* const kRunValueName;   // HKCU Run value name
extern const wchar_t* const kDataFolderName; // %APPDATA%\AminKeyboardClipboard

// Window classes
extern const wchar_t* const kClassDashboard;
extern const wchar_t* const kClassKeyboard;
extern const wchar_t* const kClassClipboard;
extern const wchar_t* const kClassMessage;

// Custom window messages
enum : UINT {
    WM_AMIN_TRAY = WM_APP + 1,
    WM_AMIN_SHOW_DASHBOARD = WM_APP + 2,
    WM_AMIN_SHOW_CLIPBOARD = WM_APP + 3,
    WM_AMIN_SHOW_KEYBOARD = WM_APP + 4,
    WM_AMIN_HOTKEY = WM_APP + 5,
    WM_AMIN_CLIP_UPDATED = WM_APP + 6,
    WM_AMIN_STORE_CHANGED = WM_APP + 7,
    WM_AMIN_TOGGLE_CLIPBOARD = WM_APP + 8,
    WM_AMIN_REFRESH = WM_APP + 9,
};

enum : int {
    TIMER_ANIM = 1,
    TIMER_TRACK = 2,
    TIMER_SAVE = 3,
    TIMER_STATUS = 4,
};

// Hotkey ids
enum : int { kHotkeyClipboard = 1, kHotkeyKeyboard = 2 };

// ---------------------------------------------------------------------------
//  UI colours (0xAARRGGBB)
// ---------------------------------------------------------------------------
namespace color {
constexpr uint32_t kBg = 0xFF0D0F16;
constexpr uint32_t kBgDeep = 0xFF0A0C12;
constexpr uint32_t kPanel = 0xFF141826;
constexpr uint32_t kCard = 0xFF171C2B;
constexpr uint32_t kCardHover = 0xFF1E2536;
constexpr uint32_t kCardPress = 0xFF232C42;
constexpr uint32_t kBorder = 0xFF262E42;
constexpr uint32_t kBorderSoft = 0xFF1C2333;
constexpr uint32_t kText = 0xFFE9EDF7;
constexpr uint32_t kTextDim = 0xFF9AA5BC;
constexpr uint32_t kTextFaint = 0xFF6B7689;
constexpr uint32_t kAccent = 0xFF7C5CFF;
constexpr uint32_t kAccent2 = 0xFF4D8DFF;
constexpr uint32_t kAccentSoft = 0xFF2A2350;
constexpr uint32_t kSuccess = 0xFF38D39F;
constexpr uint32_t kWarn = 0xFFFFB84D;
constexpr uint32_t kDanger = 0xFFFF5D73;
constexpr uint32_t kKey = 0xFF1B2130;
constexpr uint32_t kKeyHover = 0xFF253049;
constexpr uint32_t kKeySpecial = 0xFF141A27;
constexpr uint32_t kKeyText = 0xFFE9EDF7;
constexpr uint32_t kWhite = 0xFFFFFFFF;
constexpr uint32_t kBlack = 0xFF000000;
}  // namespace color

uint32_t rgba(int r, int g, int b, int a = 255);
uint32_t lerp_color(uint32_t a, uint32_t b, float t);
uint32_t with_alpha(uint32_t c, int alpha);
int color_a(uint32_t c);
int color_r(uint32_t c);
int color_g(uint32_t c);
int color_b(uint32_t c);
COLORREF to_colorref(uint32_t c);

// ---------------------------------------------------------------------------
//  Animation helper (exponential ease-out, frame rate independent)
// ---------------------------------------------------------------------------
struct Anim {
    float value = 0.0f;
    float target = 0.0f;
    // Advances towards target; returns true while still moving.
    bool step(float dt);
    bool running() const { return value != target; }
    void set(float v) { value = target = v; }
};

float ease_out_cubic(float t);
float clampf(float v, float lo, float hi);

// ---------------------------------------------------------------------------
//  Surface: 32-bit top-down DIB used for flicker free, anti-aliased drawing
// ---------------------------------------------------------------------------
class Surface {
public:
    ~Surface();
    // Resizes to the given size; returns false when the DIB cannot be created.
    bool resize(HDC ref, int w, int h);
    void release();
    bool valid() const { return bits_ != nullptr; }
    int width() const { return w_; }
    int height() const { return h_; }
    HDC dc() const { return dc_; }

    void clear(uint32_t color);
    void fill_rect(int x, int y, int w, int h, uint32_t color);
    void fill_rect_grad(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
    void fill_round(int x, int y, int w, int h, float radius, uint32_t color);
    void fill_round_grad(int x, int y, int w, int h, float radius, uint32_t c1, uint32_t c2,
                         bool vertical);
    void stroke_round(int x, int y, int w, int h, float radius, float thickness, uint32_t color);
    void shadow_round(int x, int y, int w, int h, float radius, int spread, uint32_t color,
                      float opacity);
    void line(float x0, float y0, float x1, float y1, float thickness, uint32_t color);
    void polyline(const float* points, int count, float thickness, uint32_t color);
    void circle(float cx, float cy, float r, uint32_t color);
    void ring(float cx, float cy, float r, float thickness, uint32_t color);
    void triangle(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t color);
    // Blends the whole surface onto the destination.
    void blit(HDC dst, int x, int y);

    // Raw pixel access (bounds are checked).
    void blend_px(int x, int y, uint32_t color, float alpha);

private:
    void blend_span(int y, int x0, int x1, uint32_t color, float alpha);

    HBITMAP bmp_ = nullptr;
    HBITMAP old_bmp_ = nullptr;
    HDC dc_ = nullptr;
    void* bits_ = nullptr;
    int w_ = 0;
    int h_ = 0;
};

// ---------------------------------------------------------------------------
//  Fonts
// ---------------------------------------------------------------------------
enum class FontRole {
    Ui = 0,        // Segoe UI
    UiSmall,
    UiTiny,
    UiMedium,
    UiLarge,
    UiTitle,
    UiBold,
    UiBoldSmall,
    PersianKey,
    PersianLabel,
    PersianSmall,
    PersianTitle,
    RoleCount
};

// Creates/recreates all fonts for the current DPI. Returns false if GDI failed.
bool fonts_init(int dpi);
void fonts_free();
HFONT font(FontRole role, int px_delta = 0);
// Picks a font from a fallback chain that actually contains every glyph of the
// string (important for Persian punctuation such as U+FDFC, and for symbols).
HFONT font_for_text(const std::wstring& text, FontRole role, int px_delta = 0);

// ---------------------------------------------------------------------------
//  Text drawing (Uniscribe for Persian/Arabic shaping, GDI for Latin)
// ---------------------------------------------------------------------------
enum TextAlign { AlignLeft = 0, AlignCenter = 1, AlignRight = 2 };
enum TextVAlign { VTop = 0, VCenter = 1, VBottom = 2 };

// Draws text inside rc. Handles RTL/Arabic shaping, ellipsis and clipping.
// rtl_base forces an RTL paragraph direction (Persian labels).
void draw_text(HDC dc, const std::wstring& text, RECT rc, HFONT f, uint32_t color,
               TextAlign align = AlignLeft, TextVAlign valign = VCenter, bool rtl_base = false,
               bool ellipsis = true);
// Word wraps text into rc (handles explicit newlines and RTL paragraphs).
void draw_text_wrapped(HDC dc, const std::wstring& text, RECT rc, HFONT f, uint32_t color,
                       int line_height_px = 0, bool rtl_base = false);
// Measures the width of a single line.
int measure_text(HDC dc, const std::wstring& text, HFONT f, bool rtl_base = false);
// True when the string contains Arabic-script code points.
bool text_is_rtl(const std::wstring& text);

// ---------------------------------------------------------------------------
//  Icons (drawn with anti-aliased primitives so no image assets are needed)
// ---------------------------------------------------------------------------
enum class Icon {
    None = 0,
    Keyboard,
    Clipboard,
    Close,
    Minimize,
    Pin,
    Search,
    Copy,
    Trash,
    Pause,
    Play,
    Power,
    Gear,
    Check,
    Warning,
    ChevronLeft,
    ChevronRight,
    Spark,
    Mouse,
    Shift,
    Backspace,
    Enter,
    Tab,
    Globe,
    Grid,
};
void draw_icon(Surface& s, Icon icon, float x, float y, float size, uint32_t color);

// ---------------------------------------------------------------------------
//  Window specific UI modules
// ---------------------------------------------------------------------------
bool dashboard_register(HINSTANCE inst);
HWND dashboard_create(HINSTANCE inst, bool visible);
void dashboard_paint(HWND hwnd);
void dashboard_refresh(HWND hwnd);
void dashboard_resize_children(HWND hwnd);

bool keyboard_register(HINSTANCE inst);
HWND keyboard_create(HINSTANCE inst);
void keyboard_paint(HWND hwnd);
void keyboard_apply_mode();          // re-applies always-on-top / no-focus flags
void keyboard_rebuild_fonts();       // after DPI/scale changes
void keyboard_set_language(int lang_index);

bool clipboard_register(HINSTANCE inst);
HWND clipboard_create(HINSTANCE inst);
void clipboard_paint(HWND hwnd);
void clipboard_refresh(HWND hwnd);
void clipboard_focus_search(HWND hwnd);
void clipboard_resize_children(HWND hwnd);

// ---------------------------------------------------------------------------
//  Application state
// ---------------------------------------------------------------------------
struct App {
    HINSTANCE inst = nullptr;
    HWND msg = nullptr;       // hidden top level window: tray, hotkeys, clipboard
    HWND dash = nullptr;
    HWND kb = nullptr;
    HWND clip = nullptr;

    HWND dash_search = nullptr;
    HWND clip_search = nullptr;

    HWND target = nullptr;    // last foreground window that is not ours
    std::wstring target_title;

    int dpi = 96;
    float scale = 1.0f;

    core::Settings st;
    core::ClipboardStore store;

    // Paths
    std::wstring data_dir;
    std::wstring history_path;
    std::wstring settings_path;
    std::wstring log_path;
    bool data_writable = false;
    std::wstring data_error;

    // Status
    bool clipboard_listener_ok = false;
    bool tray_ok = false;
    std::wstring tray_error;

    // Hotkey state (what is actually registered right now)
    bool hotkey_clip_ok = false;
    UINT hotkey_clip_mod = 0;
    UINT hotkey_clip_vk = 0;
    std::wstring hotkey_clip_active;    // e.g. "Ctrl+Alt+V" or ""
    std::wstring hotkey_clip_requested; // what the user asked for
    std::wstring hotkey_clip_reason;    // "conflict" / "denied" / ...
    std::wstring hotkey_clip_message;   // user facing explanation
    bool hotkey_kb_ok = false;
    std::wstring hotkey_kb_active;

    // Set right after we write the clipboard ourselves so the listener does not
    // re-order the history for our own writes. Time based to stay correct even
    // when Windows does not send a notification for identical text.
    ULONGLONG clipboard_suppress_until = 0;

    bool store_dirty = false;
    bool settings_dirty = false;
    bool exiting = false;

    // UI state shared between dashboard/keyboard/clipboard
    int dash_tab = 0;              // 0 = keyboard, 1 = clipboard
    bool wizard_open = false;
    int wizard_step = 0;
    std::wstring preview_text;     // last text sent from the virtual keyboard
    std::wstring status_text;
    uint32_t status_color = color::kTextDim;
    ULONGLONG status_until = 0;

    // Clipboard window state
    std::wstring clip_filter;
    int clip_sel = 0;              // index into the *filtered* list
    int clip_scroll = 0;
    int clip_hover = -1;
    std::vector<size_t> clip_filtered;

    // Keyboard window state
    int kb_lang = 0;
    int kb_page = 0;
    bool kb_shift = false;
    bool kb_caps = false;
    bool kb_shift_used = false;    // one-shot shift consumed after one character

    bool StartHidden = false;      // launched with --minimized
};

extern App g_app;

// ---------------------------------------------------------------------------
//  App level operations (app.cpp)
// ---------------------------------------------------------------------------
void app_init_paths();
bool app_load_settings();
void app_save_settings(bool immediate = false);
bool app_load_history();
void app_save_history(bool immediate = false);
void app_log(const wchar_t* fmt, ...);

void app_show_dashboard(bool activate);
void app_hide_dashboard();
void app_toggle_dashboard();

void app_show_keyboard(bool activate = true);
void app_hide_keyboard();
void app_toggle_keyboard();

void app_show_clipboard(bool activate = true);
void app_hide_clipboard();
void app_toggle_clipboard();
void app_focus_previous_target();

void app_exit();

void app_set_status(const std::wstring& text, uint32_t color);
void app_refresh_ui();               // repaint all visible windows
void app_store_changed(bool added_newest);   // persist + refresh lists
void app_apply_keyboard_mode();
void app_apply_startup(bool enable);
void app_copy_entry(size_t index, bool hide_after);

// Registers the global shortcuts as requested by settings. Fills the status
// fields on g_app. Never throws, always reports what really happened.
void app_register_hotkeys();
void app_unregister_hotkeys();

// ---------------------------------------------------------------------------
//  Platform helpers (platform.cpp)
// ---------------------------------------------------------------------------
// Foreground tracking / injection
void inject_track_foreground();
bool inject_char(wchar_t ch);
bool inject_text(const std::wstring& text);
bool inject_vk(UINT vk);
bool inject_backspace();
void focus_restore_target();

// Clipboard
bool clipboard_open_with_retry(HWND owner, int attempts);
std::wstring clipboard_get_text();
bool clipboard_set_text(HWND owner, const std::wstring& text);
bool clipboard_monitor_start(HWND msg_window);
void clipboard_monitor_stop();

// Tray
bool tray_add(HWND msg_window);
void tray_remove();
void tray_update_tooltip();
void tray_show_menu(HWND msg_window);
void tray_restore_icon();

// Startup registry + shell integration
bool startup_is_enabled();
bool startup_set_enabled(bool enable, std::wstring* err);
bool shell_create_desktop_shortcut(std::wstring* err);
void shell_open_data_folder();
void shell_open_url(const wchar_t* url);

// Utilities
std::wstring win32_error_text(DWORD code);
std::wstring path_join(const std::wstring& a, const std::wstring& b);
std::wstring exe_path();
std::wstring exe_dir();
bool file_write_utf8(const std::wstring& path, const std::string& data, std::wstring* err);
bool file_read_utf8(const std::wstring& path, std::string* out, std::wstring* err);
bool ensure_directory(const std::wstring& path);
bool file_exists(const std::wstring& path);
void message_box(HWND owner, const std::wstring& text, const std::wstring& caption, UINT flags);
bool run_at_startup_supported();

// Conversions between core::u16 and the Windows wide string
inline std::wstring wstr(const core::u16& s) {
    static_assert(sizeof(wchar_t) == 2, "Windows wchar_t must be UTF-16");
    return std::wstring(reinterpret_cast<const wchar_t*>(s.data()), s.size());
}
inline std::wstring wstr(const char16_t* s) {
    if (!s) return std::wstring();
    size_t n = 0;
    while (s[n]) ++n;
    return std::wstring(reinterpret_cast<const wchar_t*>(s), n);
}
inline core::u16 u16of(const std::wstring& s) {
    static_assert(sizeof(wchar_t) == 2, "Windows wchar_t must be UTF-16");
    return core::u16(reinterpret_cast<const char16_t*>(s.data()), s.size());
}
inline std::string utf8(const std::wstring& s) { return core::u16_to_utf8(u16of(s)); }

// Localized UI strings (Persian primary, English secondary)
struct L10n {
    std::wstring show_keyboard, hide_keyboard, hide, exit, start_with_windows, floating,
        no_focus, preview_title, preview_hint, tab_keyboard, tab_clipboard, search_hint,
        copy, del, clear_all, pause, resume, monitoring_on, monitoring_off, clipboard_title,
        empty_history, privacy, status, shortcuts_hint, settings, run_setup, next, back,
        finish, skip, startup_note, hotkey_label, open_clipboard, tick, quick_actions,
        sent_to, none, version_label, data_folder, desktop_shortcut, create_shortcut,
        copied, deleted, cleared, paused_msg, resumed_msg, confirm_clear, yes, no,
        entry_count, chars, error_title, info_title, ready, hotkey_ok, hotkey_fail,
        hotkey_win_v_note, installer_note, diagnostics, selftest_run;
};
extern L10n T;      // Persian strings
extern L10n TE;     // English strings
void l10n_init();

// Hidden message window (tray, hotkeys, clipboard notifications)
bool message_window_register(HINSTANCE inst);
HWND message_window_create(HINSTANCE inst);

// Self test
int selftest_run(bool console, std::wstring* report_out = nullptr);

// Window helpers
void paint_window_background(HWND hwnd, HDC dst, Surface& surf);
void init_common_controls();
void set_window_rounded_region(HWND hwnd, int radius);
std::wstring format_hotkey_display(const std::string& spec);

}  // namespace amin
