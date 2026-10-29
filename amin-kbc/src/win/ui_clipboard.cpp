// =============================================================================
//  AMIN Keyboard & Clipboard - src/win/ui_clipboard.cpp
//  Clipboard history window: search, newest-first list, copy / delete / clear.
//
//  Text only. Images and files are intentionally NOT captured, and nothing is
//  ever uploaded: the list lives in one JSON file next to settings.json.
// =============================================================================
#include "amin.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>

namespace amin {
namespace {

struct RectI {
    int x = 0, y = 0, w = 0, h = 0;
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

enum class HitId {
    None = 0,
    Close,
    Copy,
    Delete,
    Clear,
    Pause,
    Row,
    ScrollThumb,
    ScrollTrack,
};

struct Hit {
    RectI rect;
    HitId id = HitId::None;
    int index = -1;
};

struct ClipWindow {
    Surface surf;
    int width = 0;
    int height = 0;
    int title_h = 0;
    int row_h = 0;
    RectI close_rect;
    RectI search_rect;
    RectI list_rect;
    RectI copy_rect, delete_rect, clear_rect, pause_rect;
    RectI scroll_track, scroll_thumb;

    std::vector<Hit> hits;
    std::vector<Anim> row_hover;
    Anim close_hover, close_press;
    Anim copy_hover, delete_hover, clear_hover, pause_hover;
    int hover_index = -1;
    int hover_button = -1;   // index into small button hover anims
    bool tracking_leave = false;
    bool timer_on = false;
    bool dragging_thumb = false;
    int drag_offset = 0;
    HBRUSH edit_brush = nullptr;
    int scroll_rows = 0;     // rows that fit in the list
    int visible_rows = 0;
};

ClipWindow g_cw;

int scale_px(int v) { return static_cast<int>(std::lround(v * g_app.scale)); }

int64_t now_seconds() { return static_cast<int64_t>(std::time(nullptr)); }

bool persian() { return core::is_persian_ui(g_app.st); }

// ---------------------------------------------------------------------------
//  Data helpers
// ---------------------------------------------------------------------------
std::vector<size_t>& filtered() { return g_app.clip_filtered; }

void rebuild_filter() {
    g_app.clip_filtered = g_app.store.search(u16of(g_app.clip_filter));
    const int n = static_cast<int>(g_app.clip_filtered.size());
    if (g_app.clip_sel >= n) g_app.clip_sel = std::max(0, n - 1);
    const int max_scroll = std::max(0, n - g_cw.visible_rows);
    if (g_app.clip_scroll > max_scroll) g_app.clip_scroll = max_scroll;
    if (g_app.clip_scroll < 0) g_app.clip_scroll = 0;
}

void delete_selected(HWND hwnd) {
    const auto& list = filtered();
    if (g_app.clip_sel < 0 || g_app.clip_sel >= static_cast<int>(list.size())) return;
    const size_t store_index = list[g_app.clip_sel];
    const std::wstring preview =
        wstr(core::make_preview(g_app.store.at(store_index).text, 40));
    if (!g_app.store.remove_at(store_index)) return;
    app_store_changed(false);
    app_save_history(true);
    app_set_status(T.deleted + L": " + preview, color::kWarn);
    clipboard_refresh(hwnd);
}

void clear_all(HWND hwnd) {
    if (g_app.store.empty()) return;
    const int answer = MessageBoxW(hwnd, T.confirm_clear.c_str(), T.clear_all.c_str(),
                                   MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (answer != IDYES) return;
    g_app.store.clear();
    app_store_changed(false);
    app_save_history(true);
    app_set_status(T.cleared, color::kWarn);
    clipboard_refresh(hwnd);
}

void toggle_pause(HWND hwnd) {
    g_app.st.clipboard_monitoring = !g_app.st.clipboard_monitoring;
    app_save_settings(true);
    tray_update_tooltip();
    app_set_status(g_app.st.clipboard_monitoring ? T.resumed_msg : T.paused_msg,
                   g_app.st.clipboard_monitoring ? color::kSuccess : color::kWarn);
    clipboard_refresh(hwnd);
    if (g_app.dash) InvalidateRect(g_app.dash, nullptr, FALSE);
}

void copy_selected(HWND hwnd, bool hide_after) {
    const auto& list = filtered();
    if (g_app.clip_sel < 0 || g_app.clip_sel >= static_cast<int>(list.size())) return;
    app_copy_entry(list[g_app.clip_sel], hide_after);
    if (!hide_after) clipboard_refresh(hwnd);
}

// ---------------------------------------------------------------------------
//  Geometry
// ---------------------------------------------------------------------------
void compute_geometry(HWND hwnd, int w, int h) {
    g_cw.width = w;
    g_cw.height = h;
    g_cw.title_h = scale_px(50);
    const int pad = scale_px(14);
    g_cw.close_rect = {w - pad - scale_px(30), (g_cw.title_h - scale_px(30)) / 2, scale_px(30),
                       scale_px(30)};

    const int search_h = scale_px(38);
    g_cw.search_rect = {pad, g_cw.title_h + scale_px(6), w - pad * 2, search_h};

    const int btn_h = scale_px(38);
    const int button_area = btn_h + scale_px(16);
    const int list_top = g_cw.search_rect.y + search_h + scale_px(10);
    const int list_bottom = h - pad - button_area - scale_px(18);
    g_cw.list_rect = {pad, list_top, w - pad * 2, std::max(scale_px(60), list_bottom - list_top)};

    g_cw.row_h = scale_px(58);
    g_cw.visible_rows = std::max(1, g_cw.list_rect.h / g_cw.row_h);

    // Scrollbar geometry (only when needed)
    const int n = static_cast<int>(filtered().size());
    g_cw.scroll_rows = n;
    const int track_w = scale_px(6);
    g_cw.scroll_track = {g_cw.list_rect.x + g_cw.list_rect.w - track_w - scale_px(4),
                         g_cw.list_rect.y + scale_px(6), track_w, g_cw.list_rect.h - scale_px(12)};
    if (n > g_cw.visible_rows && n > 0) {
        const int thumb_h = std::max(scale_px(24), g_cw.scroll_track.h * g_cw.visible_rows / n);
        const int max_scroll = n - g_cw.visible_rows;
        const int pos = max_scroll > 0 ? (g_cw.scroll_track.h - thumb_h) * g_app.clip_scroll / max_scroll : 0;
        g_cw.scroll_thumb = {g_cw.scroll_track.x, g_cw.scroll_track.y + pos, track_w, thumb_h};
    } else {
        g_cw.scroll_thumb = {0, 0, 0, 0};
    }

    // Bottom buttons
    const int y = h - pad - btn_h;
    const int gap = scale_px(8);
    int x = pad;
    const int copy_w = scale_px(96);
    const int del_w = scale_px(86);
    const int clear_w = scale_px(110);
    const int pause_w = scale_px(120);
    g_cw.copy_rect = {x, y, copy_w, btn_h};
    x += copy_w + gap;
    g_cw.delete_rect = {x, y, del_w, btn_h};
    x += del_w + gap;
    g_cw.clear_rect = {x, y, clear_w, btn_h};
    g_cw.pause_rect = {w - pad - pause_w, y, pause_w, btn_h};

    // Child edit control: only move it when the target rectangle changed, so a
    // repaint never causes child window churn.
    if (g_app.clip_search) {
        const int ex = g_cw.search_rect.x + scale_px(36);
        const int ey = g_cw.search_rect.y + scale_px(8);
        const int ew = g_cw.search_rect.w - scale_px(48);
        const int eh = g_cw.search_rect.h - scale_px(16);
        static int last_x = -1, last_y = -1, last_w = -1, last_h = -1;
        if (ex != last_x || ey != last_y || ew != last_w || eh != last_h) {
            last_x = ex;
            last_y = ey;
            last_w = ew;
            last_h = eh;
            MoveWindow(g_app.clip_search, ex, ey, ew, eh, TRUE);
        }
    }
}

// ---------------------------------------------------------------------------
//  Painting
// ---------------------------------------------------------------------------
void draw_button(Surface& s, const RectI& rc, const std::wstring& label, Icon icon,
                 uint32_t bg, uint32_t fg, float hover) {
    const float radius = scale_px(10);
    const uint32_t fill = lerp_color(bg, lerp_color(bg, color::kWhite, 0.10f), hover);
    s.fill_round(rc.x, rc.y, rc.w, rc.h, radius, fill);
    s.stroke_round(rc.x, rc.y, rc.w, rc.h, radius, 1.0f, color::kBorder);
    int text_x = rc.x + scale_px(10);
    if (icon != Icon::None) {
        const float size = static_cast<float>(scale_px(16));
        draw_icon(s, icon, rc.x + scale_px(11), rc.y + rc.h * 0.5f - size * 0.5f, size, fg);
        text_x += scale_px(20);
    }
    RECT tr = {text_x, rc.y, rc.right() - scale_px(6), rc.bottom()};
    draw_text(s.dc(), label, tr, font(FontRole::UiBoldSmall), fg, AlignLeft, VCenter, persian());
}

void paint(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    const int w = client.right, h = client.bottom;
    if (w <= 0 || h <= 0) return;
    compute_geometry(hwnd, w, h);

    HDC screen = GetDC(hwnd);
    if (!g_cw.surf.resize(screen, w, h)) {
        ReleaseDC(hwnd, screen);
        return;
    }
    Surface& s = g_cw.surf;
    s.clear(color::kBg);

    const int pad = scale_px(14);

    // Header
    s.fill_rect_grad(0, 0, w, g_cw.title_h, 0xFF1A2032, 0xFF121724);
    const int icon_size = scale_px(20);
    draw_icon(s, Icon::Clipboard, static_cast<float>(pad),
              g_cw.title_h * 0.5f - icon_size * 0.5f, static_cast<float>(icon_size),
              color::kAccent2);
    RECT tr = {pad + icon_size + scale_px(8), 0, w - scale_px(80), g_cw.title_h};
    draw_text(s.dc(), T.clipboard_title, tr, font(FontRole::UiBold), color::kText, AlignLeft,
              VCenter, persian());

    // Close button
    {
        const uint32_t bg = lerp_color(0, color::kCardHover, g_cw.close_hover.value);
        if (g_cw.close_hover.value > 0.01f) {
            s.fill_round(g_cw.close_rect.x, g_cw.close_rect.y, g_cw.close_rect.w,
                         g_cw.close_rect.h, scale_px(8),
                         with_alpha(bg, static_cast<int>(200 * g_cw.close_hover.value)));
        }
        const float size = static_cast<float>(g_cw.close_rect.h) * 0.4f;
        draw_icon(s, Icon::Close, g_cw.close_rect.x + g_cw.close_rect.w * 0.5f - size * 0.5f,
                  g_cw.close_rect.y + g_cw.close_rect.h * 0.5f - size * 0.5f, size,
                  lerp_color(color::kTextDim, color::kText, g_cw.close_hover.value));
    }

    // Search field background (the EDIT control paints itself on top)
    {
        const float radius = scale_px(10);
        s.fill_round(g_cw.search_rect.x, g_cw.search_rect.y, g_cw.search_rect.w,
                     g_cw.search_rect.h, radius, color::kPanel);
        s.stroke_round(g_cw.search_rect.x, g_cw.search_rect.y, g_cw.search_rect.w,
                       g_cw.search_rect.h, radius, 1.0f, color::kBorder);
        const float size = static_cast<float>(scale_px(16));
        draw_icon(s, Icon::Search, g_cw.search_rect.x + scale_px(12),
                  g_cw.search_rect.y + g_cw.search_rect.h * 0.5f - size * 0.5f, size,
                  color::kTextFaint);
    }

    // List background
    {
        const float radius = scale_px(12);
        s.fill_round(g_cw.list_rect.x, g_cw.list_rect.y, g_cw.list_rect.w, g_cw.list_rect.h,
                     radius, color::kPanel);
        s.stroke_round(g_cw.list_rect.x, g_cw.list_rect.y, g_cw.list_rect.w, g_cw.list_rect.h,
                       radius, 1.0f, color::kBorderSoft);
    }

    const auto& list = filtered();
    const int64_t now = now_seconds();

    if (list.empty()) {
        // Empty state
        const float icon_size = static_cast<float>(scale_px(38));
        draw_icon(s, g_app.store.empty() ? Icon::Clipboard : Icon::Search,
                  g_cw.list_rect.x + g_cw.list_rect.w * 0.5f - icon_size * 0.5f,
                  g_cw.list_rect.y + g_cw.list_rect.h * 0.5f - icon_size - scale_px(8), icon_size,
                  with_alpha(color::kTextFaint, 180));
        RECT er = {g_cw.list_rect.x + scale_px(20),
                   g_cw.list_rect.y + g_cw.list_rect.h / 2 + scale_px(6),
                   g_cw.list_rect.x + g_cw.list_rect.w - scale_px(20),
                   g_cw.list_rect.y + g_cw.list_rect.h / 2 + scale_px(46)};
        const std::wstring msg = g_app.store.empty()
                                     ? T.empty_history
                                     : (persian() ? L"نتیجه‌ای پیدا نشد" : L"No matching entries");
        draw_text(s.dc(), msg, er, font(FontRole::Ui), color::kTextFaint, AlignCenter, VTop,
                  persian());
    }

    // Rows (clipped to the list area)
    g_cw.hits.clear();
    const int first = g_app.clip_scroll;
    const int last = std::min(static_cast<int>(list.size()), first + g_cw.visible_rows);
    if (g_cw.row_hover.size() < list.size()) g_cw.row_hover.resize(list.size());

    for (int i = first; i < last; ++i) {
        const int row_index = i - first;
        const int y = g_cw.list_rect.y + scale_px(6) + row_index * g_cw.row_h;
        if (y + g_cw.row_h > g_cw.list_rect.y + g_cw.list_rect.h) break;
        const RectI row_rc = {g_cw.list_rect.x + scale_px(6), y,
                              g_cw.list_rect.w - scale_px(18), g_cw.row_h - scale_px(4)};
        const bool selected = (i == g_app.clip_sel);
        const float hover = (i < static_cast<int>(g_cw.row_hover.size())) ? g_cw.row_hover[i].value : 0.0f;

        if (selected) {
            s.fill_round(row_rc.x, row_rc.y, row_rc.w, row_rc.h, scale_px(9), color::kAccentSoft);
            s.stroke_round(row_rc.x, row_rc.y, row_rc.w, row_rc.h, scale_px(9), 1.0f,
                           with_alpha(color::kAccent, 150));
        } else if (hover > 0.02f) {
            s.fill_round(row_rc.x, row_rc.y, row_rc.w, row_rc.h, scale_px(9),
                         with_alpha(color::kCardHover, static_cast<int>(255 * hover * 0.85f)));
        }

        const core::ClipEntry& entry = g_app.store.at(list[i]);
        const std::wstring text = wstr(entry.text);
        const bool rtl = core::has_rtl(entry.text);
        const int text_left = row_rc.x + scale_px(12);
        const int text_right = row_rc.right() - scale_px(10);

        // Preview line
        RECT line1 = {text_left, row_rc.y + scale_px(7), text_right, row_rc.y + scale_px(30)};
        const std::wstring preview = wstr(core::make_preview(entry.text, 84));
        const TextAlign align = rtl ? AlignRight : AlignLeft;
        draw_text(s.dc(), preview, line1,
                  font_for_text(preview, rtl ? FontRole::PersianLabel : FontRole::Ui), color::kText,
                  align, VCenter, rtl);

        // Meta line: time • characters
        const std::wstring time_text =
            wstr(core::format_relative_time(now, entry.ts, persian()));
        const std::wstring count_text =
            wstr(core::format_number(static_cast<int64_t>(entry.text.size()), persian()));
        const std::wstring meta = time_text + L"  \u00b7  " + count_text + L" " + T.chars;
        RECT line2 = {text_left, row_rc.y + scale_px(30), text_right, row_rc.y + row_rc.h - scale_px(4)};
        draw_text(s.dc(), meta, line2, font(FontRole::UiTiny), color::kTextFaint, align, VCenter,
                  persian());

        Hit hit;
        hit.rect = row_rc;
        hit.id = HitId::Row;
        hit.index = i;
        g_cw.hits.push_back(hit);
    }

    // Scrollbar
    if (g_cw.scroll_thumb.w > 0) {
        s.fill_round(g_cw.scroll_track.x, g_cw.scroll_track.y, g_cw.scroll_track.w,
                     g_cw.scroll_track.h, g_cw.scroll_track.w * 0.5f,
                     with_alpha(color::kBorder, 120));
        s.fill_round(g_cw.scroll_thumb.x, g_cw.scroll_thumb.y, g_cw.scroll_thumb.w,
                     g_cw.scroll_thumb.h, g_cw.scroll_thumb.w * 0.5f,
                     with_alpha(color::kTextFaint, 190));
    }

    // Bottom buttons
    const bool has_selection =
        g_app.clip_sel >= 0 && g_app.clip_sel < static_cast<int>(list.size());
    s.fill_round(g_cw.copy_rect.x, g_cw.copy_rect.y, g_cw.copy_rect.w, g_cw.copy_rect.h,
                 scale_px(10), has_selection ? color::kAccent : color::kCard);
    if (has_selection) {
        s.fill_round_grad(g_cw.copy_rect.x, g_cw.copy_rect.y, g_cw.copy_rect.w, g_cw.copy_rect.h,
                          scale_px(10), lerp_color(color::kAccent, color::kWhite, 0.06f),
                          lerp_color(color::kAccent2, color::kBlack, 0.10f), false);
    }
    {
        const float size = static_cast<float>(scale_px(16));
        draw_icon(s, Icon::Copy, g_cw.copy_rect.x + scale_px(11),
                  g_cw.copy_rect.y + g_cw.copy_rect.h * 0.5f - size * 0.5f, size, color::kWhite);
        RECT lr = {g_cw.copy_rect.x + scale_px(31), g_cw.copy_rect.y, g_cw.copy_rect.right(),
                   g_cw.copy_rect.bottom()};
        draw_text(s.dc(), T.copy, lr, font(FontRole::UiBoldSmall), color::kWhite, AlignLeft,
                  VCenter, persian());
    }
    draw_button(s, g_cw.delete_rect, T.del, Icon::Trash, color::kCard,
                has_selection ? color::kText : color::kTextFaint, g_cw.delete_hover.value);
    draw_button(s, g_cw.clear_rect, T.clear_all, Icon::Trash, color::kCard, color::kTextDim,
                g_cw.clear_hover.value);
    draw_button(s, g_cw.pause_rect,
                g_app.st.clipboard_monitoring ? T.pause : T.resume,
                g_app.st.clipboard_monitoring ? Icon::Pause : Icon::Play, color::kCard,
                g_app.st.clipboard_monitoring ? color::kWarn : color::kSuccess,
                g_cw.pause_hover.value);

    // Footer: status / count / privacy
    {
        RECT fr = {pad, h - scale_px(26), w - pad, h - scale_px(6)};
        const std::wstring count = wstr(core::format_number(static_cast<int64_t>(g_app.store.size()),
                                                           persian())) +
                                   L" / " + wstr(core::format_number(100, persian())) + L" " +
                                   T.entry_count;
        draw_text(s.dc(), count, fr, font(FontRole::UiTiny), color::kTextFaint, AlignRight,
                  VCenter, persian());
        const std::wstring status = g_app.status_text.empty() ? T.privacy : g_app.status_text;
        const uint32_t status_color = g_app.status_text.empty() ? color::kTextFaint : g_app.status_color;
        RECT sr = {pad, fr.top, w - pad - scale_px(120), fr.bottom};
        draw_text(s.dc(), status, sr, font(FontRole::UiTiny), status_color, AlignLeft, VCenter,
                  persian());
    }

    s.blit(screen, 0, 0);
    ReleaseDC(hwnd, screen);
}

// ---------------------------------------------------------------------------
//  Window procedure
// ---------------------------------------------------------------------------
void set_scroll(HWND hwnd, int new_scroll) {
    const int n = static_cast<int>(filtered().size());
    const int max_scroll = std::max(0, n - g_cw.visible_rows);
    new_scroll = std::max(0, std::min(max_scroll, new_scroll));
    if (new_scroll != g_app.clip_scroll) {
        g_app.clip_scroll = new_scroll;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void move_selection(HWND hwnd, int delta) {
    const int n = static_cast<int>(filtered().size());
    if (n == 0) return;
    g_app.clip_sel = std::max(0, std::min(n - 1, g_app.clip_sel + delta));
    if (g_app.clip_sel < g_app.clip_scroll) g_app.clip_scroll = g_app.clip_sel;
    if (g_app.clip_sel >= g_app.clip_scroll + g_cw.visible_rows) {
        g_app.clip_scroll = g_app.clip_sel - g_cw.visible_rows + 1;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK clipboard_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE: {
            g_cw.edit_brush = CreateSolidBrush(to_colorref(color::kPanel));
            g_app.clip_search = CreateWindowExW(
                0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT, 0, 0, 10, 10,
                hwnd, reinterpret_cast<HMENU>(1001), g_app.inst, nullptr);
            if (g_app.clip_search) {
                SendMessageW(g_app.clip_search, WM_SETFONT,
                             reinterpret_cast<WPARAM>(font(FontRole::Ui)), TRUE);
                SendMessageW(g_app.clip_search, EM_SETCUEBANNER, TRUE,
                             reinterpret_cast<LPARAM>(T.search_hint.c_str()));
                SendMessageW(g_app.clip_search, EM_SETLIMITTEXT, 200, 0);
                SetWindowLongPtrW(g_app.clip_search, GWLP_USERDATA, 1);
            }
            SetTimer(hwnd, TIMER_STATUS, 3000, nullptr);
            return 0;
        }
        case WM_SIZE:
            compute_geometry(hwnd, LOWORD(lparam), HIWORD(lparam));
            set_window_rounded_region(hwnd, scale_px(14));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lparam);
            mmi->ptMinTrackSize.x = scale_px(430);
            mmi->ptMinTrackSize.y = scale_px(420);
            return 0;
        }
        case WM_NCCALCSIZE:
            if (wparam) return 0;  // borderless but still resizable via WM_NCHITTEST
            break;
        case WM_NCHITTEST: {
            const LRESULT base = DefWindowProcW(hwnd, msg, wparam, lparam);
            POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &pt);
            if (pt.y >= 0 && pt.y < g_cw.title_h && pt.x < g_cw.close_rect.x - scale_px(8)) {
                return HTCAPTION;  // drag by the header
            }
            return base;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            paint(hwnd);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetTextColor(dc, to_colorref(color::kText));
            SetBkColor(dc, to_colorref(color::kPanel));
            return reinterpret_cast<LRESULT>(g_cw.edit_brush ? g_cw.edit_brush
                                                             : GetStockObject(BLACK_BRUSH));
        }
        case WM_COMMAND:
            if (HIWORD(wparam) == EN_CHANGE && LOWORD(wparam) == 1001) {
                wchar_t buffer[256] = {0};
                GetWindowTextW(g_app.clip_search, buffer, 255);
                g_app.clip_filter = buffer;
                g_app.clip_sel = 0;
                g_app.clip_scroll = 0;
                rebuild_filter();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            break;
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
            bool changed = false;
            int hovered_row = -1;
            for (const Hit& hit : g_cw.hits) {
                if (hit.id == HitId::Row && hit.rect.contains(x, y)) {
                    hovered_row = hit.index;
                    break;
                }
            }
            if (hovered_row != g_cw.hover_index) {
                g_cw.hover_index = hovered_row;
                g_cw.row_hover.resize(filtered().size());
                for (size_t i = 0; i < g_cw.row_hover.size(); ++i) {
                    g_cw.row_hover[i].target = (static_cast<int>(i) == hovered_row) ? 1.0f : 0.0f;
                }
                changed = true;
            }
            auto hover_button = [&](const RectI& rc, Anim& anim) {
                const bool over = rc.contains(x, y);
                const float target = over ? 1.0f : 0.0f;
                if (std::fabs(anim.target - target) > 0.01f) {
                    anim.target = target;
                    changed = true;
                }
                return over;
            };
            hover_button(g_cw.close_rect, g_cw.close_hover);
            hover_button(g_cw.copy_rect, g_cw.copy_hover);
            hover_button(g_cw.delete_rect, g_cw.delete_hover);
            hover_button(g_cw.clear_rect, g_cw.clear_hover);
            hover_button(g_cw.pause_rect, g_cw.pause_hover);

            if (g_cw.dragging_thumb) {
                const int track_h = g_cw.scroll_track.h;
                const int thumb_h = std::max(scale_px(24), g_cw.scroll_thumb.h);
                const int usable = std::max(1, track_h - thumb_h);
                const int offset = (y - g_cw.scroll_track.y) - g_cw.drag_offset;
                const int n = static_cast<int>(filtered().size());
                const int max_scroll = std::max(1, n - g_cw.visible_rows);
                const int new_scroll = offset * max_scroll / usable;
                set_scroll(hwnd, new_scroll);
            }

            if (!g_cw.tracking_leave) {
                TRACKMOUSEEVENT tme;
                std::memset(&tme, 0, sizeof(tme));
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                g_cw.tracking_leave = true;
            }
            const bool over_clickable = hovered_row >= 0 || g_cw.close_rect.contains(x, y) ||
                                        g_cw.copy_rect.contains(x, y) ||
                                        g_cw.delete_rect.contains(x, y) ||
                                        g_cw.clear_rect.contains(x, y) ||
                                        g_cw.pause_rect.contains(x, y);
            SetCursor(LoadCursorW(nullptr, over_clickable ? IDC_HAND : IDC_ARROW));
            if (changed && !g_cw.timer_on) {
                SetTimer(hwnd, TIMER_ANIM, 16, nullptr);
                g_cw.timer_on = true;
            }
            if (changed) InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSELEAVE:
            g_cw.tracking_leave = false;
            g_cw.hover_index = -1;
            for (Anim& a : g_cw.row_hover) a.target = 0.0f;
            g_cw.close_hover.target = 0;
            g_cw.copy_hover.target = 0;
            g_cw.delete_hover.target = 0;
            g_cw.clear_hover.target = 0;
            g_cw.pause_hover.target = 0;
            if (!g_cw.timer_on) {
                SetTimer(hwnd, TIMER_ANIM, 16, nullptr);
                g_cw.timer_on = true;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            const int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
            if (g_cw.close_rect.contains(x, y)) {
                app_hide_clipboard();
                return 0;
            }
            if (g_cw.copy_rect.contains(x, y)) {
                copy_selected(hwnd, false);
                return 0;
            }
            if (g_cw.delete_rect.contains(x, y)) {
                delete_selected(hwnd);
                return 0;
            }
            if (g_cw.clear_rect.contains(x, y)) {
                clear_all(hwnd);
                return 0;
            }
            if (g_cw.pause_rect.contains(x, y)) {
                toggle_pause(hwnd);
                return 0;
            }
            if (g_cw.scroll_thumb.w > 0 && g_cw.scroll_thumb.contains(x, y)) {
                g_cw.dragging_thumb = true;
                g_cw.drag_offset = y - g_cw.scroll_thumb.y;
                SetCapture(hwnd);
                return 0;
            }
            if (g_cw.scroll_track.w > 0 && g_cw.scroll_track.contains(x, y)) {
                set_scroll(hwnd, g_app.clip_scroll + (y > g_cw.scroll_thumb.y + g_cw.scroll_thumb.h
                                                          ? g_cw.visible_rows
                                                          : -g_cw.visible_rows));
                return 0;
            }
            for (const Hit& hit : g_cw.hits) {
                if (hit.id == HitId::Row && hit.rect.contains(x, y)) {
                    g_app.clip_sel = hit.index;
                    // Single click copies right away (as requested); the window stays
                    // open so the user can pick another entry.
                    copy_selected(hwnd, false);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (g_cw.dragging_thumb) {
                g_cw.dragging_thumb = false;
                ReleaseCapture();
            }
            return 0;
        case WM_LBUTTONDBLCLK: {
            const int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
            for (const Hit& hit : g_cw.hits) {
                if (hit.id == HitId::Row && hit.rect.contains(x, y)) {
                    g_app.clip_sel = hit.index;
                    copy_selected(hwnd, true);  // copy, hide, give focus back
                    return 0;
                }
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
            set_scroll(hwnd, g_app.clip_scroll - (delta > 0 ? 3 : -3));
            return 0;
        }
        case WM_KEYDOWN: {
            switch (wparam) {
                case VK_DOWN: move_selection(hwnd, 1); return 0;
                case VK_UP: move_selection(hwnd, -1); return 0;
                case VK_PRIOR: move_selection(hwnd, -g_cw.visible_rows); return 0;
                case VK_NEXT: move_selection(hwnd, g_cw.visible_rows); return 0;
                case VK_RETURN: copy_selected(hwnd, true); return 0;
                case VK_DELETE: delete_selected(hwnd); return 0;
                case VK_ESCAPE: app_hide_clipboard(); return 0;
                case 'C':
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        copy_selected(hwnd, false);
                        return 0;
                    }
                    break;
                case 'F':
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        clipboard_focus_search(hwnd);
                        return 0;
                    }
                    break;
                default: break;
            }
            break;
        }
        case WM_TIMER:
            if (wparam == TIMER_ANIM) {
                bool moving = false;
                for (Anim& a : g_cw.row_hover) moving |= a.step(0.016f);
                moving |= g_cw.close_hover.step(0.016f);
                moving |= g_cw.copy_hover.step(0.016f);
                moving |= g_cw.delete_hover.step(0.016f);
                moving |= g_cw.clear_hover.step(0.016f);
                moving |= g_cw.pause_hover.step(0.016f);
                InvalidateRect(hwnd, nullptr, FALSE);
                if (!moving) {
                    KillTimer(hwnd, TIMER_ANIM);
                    g_cw.timer_on = false;
                }
                return 0;
            }
            if (wparam == TIMER_STATUS) {
                // Refresh the relative timestamps ("2 minutes ago") and drop an
                // expired status message; skip the repaint when nothing changed.
                if (!IsWindowVisible(hwnd)) return 0;
                bool needs_paint = false;
                if (g_app.status_until && GetTickCount64() > g_app.status_until) {
                    g_app.status_text.clear();
                    needs_paint = true;
                }
                static DWORD last_minute = 0;
                const DWORD minute = static_cast<DWORD>(GetTickCount64() / 60000);
                if (minute != last_minute) {
                    last_minute = minute;
                    needs_paint = true;
                }
                if (needs_paint) InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ANIM);
            KillTimer(hwnd, TIMER_STATUS);
            g_cw.timer_on = false;
            if (g_cw.edit_brush) {
                DeleteObject(g_cw.edit_brush);
                g_cw.edit_brush = nullptr;
            }
            g_cw.surf.release();
            g_app.clip_search = nullptr;
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

bool clipboard_register(HINSTANCE inst) {
    WNDCLASSEXW wc;
    std::memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = clipboard_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassClipboard;
    wc.style = CS_DBLCLKS;
    if (!RegisterClassExW(&wc)) return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    return true;
}

HWND clipboard_create(HINSTANCE inst) {
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int width = scale_px(560);
    int height = scale_px(620);
    const int work_h = work.bottom - work.top;
    if (height > work_h - scale_px(40)) height = work_h - scale_px(40);
    const int x = work.right - width - scale_px(24);
    const int y = work.top + (work_h - height) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClassClipboard, T.clipboard_title.c_str(),
                                WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN, x, y, width, height,
                                nullptr, nullptr, inst, nullptr);
    if (!hwnd) {
        app_log(L"clipboard window creation failed (%lu)", GetLastError());
        return nullptr;
    }
    set_window_rounded_region(hwnd, scale_px(14));
    compute_geometry(hwnd, width, height);
    clipboard_refresh(hwnd);
    return hwnd;
}

void clipboard_refresh(HWND hwnd) {
    if (hwnd && g_app.clip_search) {
        wchar_t buffer[256] = {0};
        GetWindowTextW(g_app.clip_search, buffer, 255);
        g_app.clip_filter = buffer;
    }
    rebuild_filter();
    if (hwnd) InvalidateRect(hwnd, nullptr, FALSE);
}

void clipboard_focus_search(HWND hwnd) {
    if (g_app.clip_search) {
        SetFocus(g_app.clip_search);
        SendMessageW(g_app.clip_search, EM_SETSEL, 0, -1);
    }
    (void)hwnd;
}

void clipboard_paint(HWND hwnd) { paint(hwnd); }

void clipboard_resize_children(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    compute_geometry(hwnd, client.right, client.bottom);
}

}  // namespace amin
