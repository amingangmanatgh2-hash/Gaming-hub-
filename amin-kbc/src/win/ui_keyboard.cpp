// =============================================================================
//  AMIN Keyboard & Clipboard - src/win/ui_keyboard.cpp
//  The floating on-screen keyboard.
//
//  Key presses are injected with SendInput(KEYEVENTF_UNICODE) after handing the
//  foreground back to the window that was active before the keyboard opened.
//  In "no-focus" mode the window uses WS_EX_NOACTIVATE and never takes focus at
//  all, so characters land directly in the application the user was typing in.
// =============================================================================
#include "amin.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace amin {
namespace {

struct RectI {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

const int kMaxPreviewChars = 200;

struct KeyHit {
    RectI rect;
    int row = 0;
    int col = 0;
};

struct KbWindow {
    Surface surf;
    int width = 0;
    int height = 0;
    int title_h = 0;
    int key_gap = 0;
    int key_h = 0;

    std::vector<KeyHit> hits;
    std::vector<Anim> hover;
    std::vector<Anim> press;
    Anim pin_hover, close_hover;
    Anim pin_press, close_press;
    RectI pin_rect, close_rect;

    int hover_key = -1;
    int press_key = -1;
    bool tracking_leave = false;
    bool timer_on = false;
    HWND last_target = nullptr;
    std::wstring last_target_title;
    int geom_page = -1;
    int geom_lang = -1;
};

KbWindow g_kb;

// ---------------------------------------------------------------------------
//  Layout helpers
// ---------------------------------------------------------------------------
const core::PageDef& current_page() {
    const core::LayoutDef& layout = core::layout_by_index(g_app.kb_lang);
    int page = g_app.kb_page;
    if (page < 0 || page >= layout.page_count) page = 0;
    return layout.pages[page];
}

bool shift_active() { return g_app.kb_shift != g_app.kb_caps; }

int scale_px(int v) { return static_cast<int>(std::lround(v * g_app.scale)); }

void compute_geometry(int width, int height) {
    g_kb.width = width;
    g_kb.height = height;
    g_kb.geom_page = g_app.kb_page;
    g_kb.geom_lang = g_app.kb_lang;
    g_kb.title_h = scale_px(42);
    const int pad = scale_px(12);
    const core::PageDef& page = current_page();
    const int rows = std::max(1, page.row_count);
    g_kb.key_gap = std::max(2, scale_px(6));

    const int available_h = height - g_kb.title_h - pad * 2;
    int key_h = (available_h - (rows - 1) * g_kb.key_gap) / rows;
    key_h = std::max(scale_px(26), key_h);
    g_kb.key_h = key_h;

    // Key rectangles
    g_kb.hits.clear();
    int y = g_kb.title_h + pad;
    const int inner_w = width - pad * 2;
    for (int r = 0; r < page.row_count; ++r) {
        const core::RowDef& row = page.rows[r];
        std::vector<core::KeyPlacement> place;
        core::compute_row(row, inner_w, g_kb.key_gap, y, key_h, &place);
        for (size_t c = 0; c < place.size(); ++c) {
            KeyHit hit;
            hit.rect.x = pad + place[c].x;
            hit.rect.y = place[c].y;
            hit.rect.w = place[c].w;
            hit.rect.h = place[c].h;
            hit.row = r;
            hit.col = static_cast<int>(c);
            g_kb.hits.push_back(hit);
        }
        y += key_h + g_kb.key_gap;
    }
    if (g_kb.hover.size() != g_kb.hits.size()) {
        g_kb.hover.assign(g_kb.hits.size(), Anim());
        g_kb.press.assign(g_kb.hits.size(), Anim());
    }

    // Title bar buttons (right aligned)
    const int btn = scale_px(30);
    const int btn_y = (g_kb.title_h - btn) / 2;
    g_kb.close_rect = {width - scale_px(10) - btn, btn_y, btn, btn};
    g_kb.pin_rect = {g_kb.close_rect.x - scale_px(6) - btn, btn_y, btn, btn};
}

void ensure_timer(HWND hwnd) {
    if (!g_kb.timer_on) {
        SetTimer(hwnd, TIMER_ANIM, 16, nullptr);
        g_kb.timer_on = true;
    }
}

// ---------------------------------------------------------------------------
//  Painting
// ---------------------------------------------------------------------------
uint32_t key_background(const core::KeyDef& key, bool hovered, bool pressed, bool shift_on) {
    switch (key.action) {
        case core::KeyAction::Shift:
        case core::KeyAction::CapsLock:
            if (shift_on) return color::kAccent;
            break;
        case core::KeyAction::Enter:
            return pressed ? color::kAccent2 : (hovered ? lerp_color(color::kAccent2, color::kWhite, 0.12f)
                                                        : color::kAccent2);
        case core::KeyAction::Language:
        case core::KeyAction::Symbols:
            return pressed ? color::kCardPress : (hovered ? color::kCardHover : color::kKeySpecial);
        default:
            break;
    }
    if (pressed) return color::kCardPress;
    if (hovered) return color::kKeyHover;
    return key.action == core::KeyAction::Character ? color::kKey : color::kKeySpecial;
}

void draw_kb_icon(Surface& s, core::KeyAction action, const RectI& rc, uint32_t color) {
    const float size = static_cast<float>(std::min(rc.w, rc.h)) * 0.44f;
    const float x = rc.x + rc.w * 0.5f - size * 0.5f;
    const float y = rc.y + rc.h * 0.5f - size * 0.5f;
    switch (action) {
        case core::KeyAction::Backspace: draw_icon(s, Icon::Backspace, x, y, size, color); break;
        case core::KeyAction::Enter: draw_icon(s, Icon::Enter, x, y, size, color); break;
        case core::KeyAction::Tab: draw_icon(s, Icon::Tab, x, y, size, color); break;
        case core::KeyAction::Shift: draw_icon(s, Icon::Shift, x, y, size, color); break;
        case core::KeyAction::HideKeyboard: draw_icon(s, Icon::Close, x, y, size, color); break;
        case core::KeyAction::Language:
            draw_icon(s, Icon::Globe, rc.x + scale_px(8),
                      rc.y + rc.h * 0.5f - size * 0.5f, size, color);
            break;
        default: break;
    }
}

void paint(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    const int w = client.right, h = client.bottom;
    if (w <= 0 || h <= 0) return;
    // Re-layout not only on resize but also when the page or the language
    // changed, because the key tables (and therefore the hit map) differ.
    if (g_kb.width != w || g_kb.height != h || g_kb.geom_page != g_app.kb_page ||
        g_kb.geom_lang != g_app.kb_lang) {
        compute_geometry(w, h);
    }

    HDC screen = GetDC(hwnd);
    if (!g_kb.surf.resize(screen, w, h)) {
        ReleaseDC(hwnd, screen);
        return;
    }
    Surface& s = g_kb.surf;
    s.clear(color::kBg);

    // Subtle vertical gradient for depth.
    s.fill_rect_grad(0, 0, w, g_kb.title_h, 0xFF1A2032, 0xFF121724);

    // Title bar
    const int pad = scale_px(12);
    const int icon_size = scale_px(18);
    draw_icon(s, Icon::Keyboard, static_cast<float>(pad), g_kb.title_h * 0.5f - icon_size * 0.5f,
              static_cast<float>(icon_size), color::kAccent);
    const std::wstring title = core::is_persian_ui(g_app.st) ? L"\u0635\u0641\u062d\u0647 \u06a9\u0644\u06cc\u062f"
                                                             : L"Keyboard";
    RECT title_rc = {pad + icon_size + scale_px(8), 0, pad + icon_size + scale_px(220),
                     g_kb.title_h};
    draw_text(s.dc(), title, title_rc, font(FontRole::UiBold), color::kText, AlignLeft, VCenter,
              core::is_persian_ui(g_app.st));

    // Language badge
    const core::LayoutDef& layout = core::layout_by_index(g_app.kb_lang);
    const std::wstring lang_label = wstr(layout.name);
    const int badge_w = scale_px(64);
    const int badge_h = scale_px(22);
    RECT badge = {title_rc.right + scale_px(6), (g_kb.title_h - badge_h) / 2,
                  title_rc.right + scale_px(6) + badge_w, (g_kb.title_h + badge_h) / 2};
    s.fill_round(badge.left, badge.top, badge_w, badge_h, badge_h * 0.5f, color::kAccentSoft);
    draw_text(s.dc(), lang_label, badge, font(FontRole::PersianSmall), color::kAccent, AlignCenter,
              VCenter, true);

    // Target window hint
    if (g_app.target && !g_app.target_title.empty()) {
        const std::wstring target =
            wstr(core::make_preview(u16of(g_app.target_title), 34));
        RECT tr = {badge.right + scale_px(14), 0, g_kb.pin_rect.x - scale_px(10), g_kb.title_h};
        if (tr.right > tr.left) {
            const std::wstring prefix = (core::is_persian_ui(g_app.st) ? L"\u2190 " : L"-> ");
            const std::wstring label = prefix + target;
            draw_text(s.dc(), label, tr, font(FontRole::UiSmall), color::kTextFaint, AlignRight,
                      VCenter, core::is_persian_ui(g_app.st));
        }
    }

    // Pin / close
    auto paint_title_button = [&](const RectI& rc, const Anim& hover, const Anim& press,
                                  Icon icon) {
        const uint32_t bg = lerp_color(0, color::kCardHover, hover.value);
        if (hover.value > 0.01f) {
            s.fill_round(rc.x, rc.y, rc.w, rc.h, rc.h * 0.3f,
                         with_alpha(bg, static_cast<int>(255 * hover.value)));
        }
        const uint32_t fg = lerp_color(color::kTextDim, color::kText, hover.value);
        const float size = static_cast<float>(rc.h) * 0.44f;
        draw_icon(s, icon, rc.x + rc.w * 0.5f - size * 0.5f, rc.y + rc.h * 0.5f - size * 0.5f,
                  size, fg);
    };
    paint_title_button(g_kb.pin_rect, g_kb.pin_hover, g_kb.pin_press, Icon::Pin);
    paint_title_button(g_kb.close_rect, g_kb.close_hover, g_kb.close_press, Icon::Close);
    if (g_app.st.keyboard_always_on_top) {
        // Highlight the pin when active.
        s.ring(g_kb.pin_rect.x + g_kb.pin_rect.w * 0.5f,
               g_kb.pin_rect.y + g_kb.pin_rect.h * 0.5f, scale_px(13), 1.0f,
               with_alpha(color::kAccent, 160));
    }

    // Keys
    const core::PageDef& page = current_page();
    const bool shift_on = shift_active();
    for (size_t i = 0; i < g_kb.hits.size(); ++i) {
        const KeyHit& hit = g_kb.hits[i];
        if (hit.row >= page.row_count) continue;
        const core::RowDef& row = page.rows[hit.row];
        if (hit.col >= row.count) continue;
        const core::KeyDef& key = row.keys[hit.col];

        const float hover = i < g_kb.hover.size() ? g_kb.hover[i].value : 0.0f;
        const float press = i < g_kb.press.size() ? g_kb.press[i].value : 0.0f;
        const bool hovered = hover > 0.02f;
        const bool pressed = press > 0.4f;

        const RectI& rc = hit.rect;
        // Press "pushes" the key down by a pixel.
        const int oy = pressed ? 1 : 0;
        const float radius = std::min(static_cast<float>(scale_px(9)),
                                      static_cast<float>(std::min(rc.w, rc.h)) * 0.28f);

        if (hovered && !pressed) {
            s.fill_round(rc.x, rc.y + oy, rc.w, rc.h, radius,
                         with_alpha(color::kAccent, static_cast<int>(18 * hover)));
        }
        uint32_t bg = key_background(key, hovered, pressed, shift_on);
        if (key.action == core::KeyAction::Character || key.action == core::KeyAction::Space) {
            bg = lerp_color(color::kKey, color::kKeyHover, hover * 0.9f);
            if (pressed) bg = lerp_color(color::kAccent, color::kKey, 0.35f);
        }
        s.fill_round(rc.x, rc.y + oy, rc.w, rc.h, radius, bg);
        s.stroke_round(rc.x, rc.y + oy, rc.w, rc.h, radius, 1.0f,
                       lerp_color(color::kBorderSoft, color::kBorder, hover));

        // Label / icon
        const bool is_modifier = key.action == core::KeyAction::Shift ||
                                 key.action == core::KeyAction::CapsLock;
        uint32_t fg = color::kKeyText;
        if (key.action == core::KeyAction::Language || key.action == core::KeyAction::Symbols ||
            key.action == core::KeyAction::Escape)
            fg = color::kTextDim;
        if (is_modifier && shift_on) fg = color::kWhite;
        if (key.action == core::KeyAction::Enter) fg = color::kWhite;
        if (key.action == core::KeyAction::Space) fg = color::kTextFaint;

        const char16_t* label16 = core::key_label(key, shift_on);
        const std::wstring label = wstr(label16);
        const bool need_icon =
            key.action == core::KeyAction::Backspace || key.action == core::KeyAction::Enter ||
            key.action == core::KeyAction::Tab || key.action == core::KeyAction::Shift ||
            key.action == core::KeyAction::HideKeyboard;
        if (need_icon) {
            draw_kb_icon(s, key.action, rc, fg);
        } else if (key.action == core::KeyAction::Space) {
            // three small dashes
            const float cx = rc.x + rc.w * 0.5f, cy = rc.y + rc.h * 0.5f;
            for (int d = -1; d <= 1; ++d) {
                s.line(cx + d * scale_px(9) - scale_px(3), cy, cx + d * scale_px(9) + scale_px(3),
                       cy, 1.6f, with_alpha(fg, 120));
            }
        } else if (!label.empty()) {
            const bool rtl = core::layout_by_index(g_app.kb_lang).rtl;
            FontRole role = rtl ? FontRole::PersianKey : FontRole::UiLarge;
            // Shrink long labels until they fit.
            int delta = 0;
            RECT lr = {rc.x + scale_px(3), rc.y + oy,
                       rc.x + rc.w - scale_px(3), rc.y + oy + rc.h};
            HFONT f = font_for_text(label, role, delta);
            int text_w = measure_text(s.dc(), label, f, rtl);
            while (text_w > lr.right - lr.left && delta > -7) {
                --delta;
                f = font_for_text(label, role, delta);
                text_w = measure_text(s.dc(), label, f, rtl);
            }
            if (key.action == core::KeyAction::Language) {
                RECT text_rc = {rc.x + scale_px(22), rc.y + oy, lr.right, rc.y + oy + rc.h};
                draw_text(s.dc(), label, text_rc, f, fg, AlignLeft, VCenter, rtl);
            } else {
                draw_text(s.dc(), label, lr, f, fg, AlignCenter, VCenter, rtl);
            }
        }
    }

    s.blit(screen, 0, 0);
    ReleaseDC(hwnd, screen);
}

// ---------------------------------------------------------------------------
//  Actions
// ---------------------------------------------------------------------------
void append_preview(wchar_t ch) {
    g_app.preview_text.push_back(ch);
    if (g_app.preview_text.size() > static_cast<size_t>(kMaxPreviewChars)) {
        g_app.preview_text.erase(0, g_app.preview_text.size() - kMaxPreviewChars);
    }
}

// Shows a clear message when a key could not be delivered (no target window or
// Windows refused the foreground switch) instead of pretending it worked.
void report_injection_failure() {
    app_set_status(core::is_persian_ui(g_app.st)
                       ? L"ارسال کلید ممکن نشد: پنجره مقصد مشخص نیست. اول روی برنامه دلخواه "
                         L"کلیک کنید یا حالت «بدون فوکوس» را روشن کنید."
                       : L"Could not send the key: no target window. Click the destination app "
                         L"first, or enable no-focus mode.",
                   color::kDanger);
}

void activate_key(int index) {
    if (index < 0 || index >= static_cast<int>(g_kb.hits.size())) return;
    const KeyHit& hit = g_kb.hits[index];
    const core::PageDef& page = current_page();
    if (hit.row >= page.row_count) return;
    const core::RowDef& row = page.rows[hit.row];
    if (hit.col >= row.count) return;
    const core::KeyDef& key = row.keys[hit.col];
    const bool shift_on = shift_active();

    switch (key.action) {
        case core::KeyAction::Character: {
            const std::wstring text = wstr(core::key_text(key, shift_on));
            if (text.empty()) break;
            if (!inject_text(text)) {
                report_injection_failure();
                break;
            }
            for (wchar_t c : text) append_preview(c);
            if (g_app.kb_shift && !g_app.kb_caps) g_app.kb_shift = false;
            break;
        }
        case core::KeyAction::Space:
            if (!inject_char(L' ')) {
                report_injection_failure();
                break;
            }
            append_preview(L' ');
            if (g_app.kb_shift && !g_app.kb_caps) g_app.kb_shift = false;
            break;
        case core::KeyAction::Backspace:
            if (!inject_backspace()) {
                report_injection_failure();
                break;
            }
            if (!g_app.preview_text.empty()) g_app.preview_text.pop_back();
            break;
        case core::KeyAction::Enter:
            if (!inject_vk(VK_RETURN)) {
                report_injection_failure();
                break;
            }
            append_preview(L'\n');
            break;
        case core::KeyAction::Tab:
            if (!inject_vk(VK_TAB)) {
                report_injection_failure();
                break;
            }
            append_preview(L'\t');
            break;
        case core::KeyAction::Escape:
            inject_vk(VK_ESCAPE);
            break;
        case core::KeyAction::Shift:
            g_app.kb_shift = !g_app.kb_shift;
            if (g_app.kb_caps) g_app.kb_caps = false;  // shift cancels caps
            break;
        case core::KeyAction::CapsLock:
            g_app.kb_caps = !g_app.kb_caps;
            g_app.kb_shift = false;
            break;
        case core::KeyAction::Language:
            g_app.kb_lang = (g_app.kb_lang == 0) ? 1 : 0;
            g_app.kb_page = 0;
            g_app.st.keyboard_language = g_app.kb_lang;
            app_save_settings();
            if (g_app.dash) InvalidateRect(g_app.dash, nullptr, FALSE);
            break;
        case core::KeyAction::Symbols:
            g_app.kb_page = (g_app.kb_page == 0) ? 1 : 0;
            break;
        case core::KeyAction::HideKeyboard:
            app_hide_keyboard();
            return;
        case core::KeyAction::VirtualKey: {
            const std::wstring text = wstr(core::key_text(key, shift_on));
            if (!text.empty()) inject_text(text);
            if (key.vk) inject_vk(key.vk);
            break;
        }
    }
    InvalidateRect(g_app.kb, nullptr, FALSE);
    if (g_app.dash && IsWindowVisible(g_app.dash)) {
        // Repaint the preview card only if it is showing.
        InvalidateRect(g_app.dash, nullptr, FALSE);
    }
}

int hit_test(int x, int y) {
    for (size_t i = 0; i < g_kb.hits.size(); ++i) {
        if (g_kb.hits[i].rect.contains(x, y)) return static_cast<int>(i);
    }
    return -1;
}

void start_drag(HWND hwnd) {
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

// ---------------------------------------------------------------------------
//  Window procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK keyboard_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE: {
            SetTimer(hwnd, TIMER_TRACK, 200, nullptr);  // keep the target fresh
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            paint(hwnd);
            EndPaint(hwnd, &ps);
            (void)dc;
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE: {
            compute_geometry(LOWORD(lparam), HIWORD(lparam));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
            const int key = hit_test(x, y);
            bool changed = false;
            if (key != g_kb.hover_key) {
                g_kb.hover_key = key;
                for (size_t i = 0; i < g_kb.hover.size(); ++i) {
                    g_kb.hover[i].target = (static_cast<int>(i) == key) ? 1.0f : 0.0f;
                }
                changed = true;
            }
            const bool over_pin = g_kb.pin_rect.contains(x, y);
            const bool over_close = g_kb.close_rect.contains(x, y);
            if (over_pin != (g_kb.pin_hover.target > 0.5f)) {
                g_kb.pin_hover.target = over_pin ? 1.0f : 0.0f;
                changed = true;
            }
            if (over_close != (g_kb.close_hover.target > 0.5f)) {
                g_kb.close_hover.target = over_close ? 1.0f : 0.0f;
                changed = true;
            }
            if (!g_kb.tracking_leave) {
                TRACKMOUSEEVENT tme;
                std::memset(&tme, 0, sizeof(tme));
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                g_kb.tracking_leave = true;
            }
            // Change the cursor over title buttons and keys.
            SetCursor(LoadCursorW(nullptr, (key >= 0 || over_pin || over_close) ? IDC_HAND
                                                                              : IDC_ARROW));
            if (changed) {
                ensure_timer(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE: {
            g_kb.tracking_leave = false;
            g_kb.hover_key = -1;
            for (Anim& a : g_kb.hover) a.target = 0.0f;
            g_kb.pin_hover.target = 0.0f;
            g_kb.close_hover.target = 0.0f;
            ensure_timer(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            const int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
            if (g_kb.close_rect.contains(x, y)) {
                g_kb.close_press.target = 1.0f;
                ensure_timer(hwnd);
                return 0;
            }
            if (g_kb.pin_rect.contains(x, y)) {
                g_kb.pin_press.target = 1.0f;
                ensure_timer(hwnd);
                return 0;
            }
            if (y < g_kb.title_h) {
                start_drag(hwnd);
                return 0;
            }
            const int key = hit_test(x, y);
            g_kb.press_key = key;
            if (key >= 0) {
                g_kb.press[key].target = 1.0f;
                ensure_timer(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            SetCapture(hwnd);
            return 0;
        }
        case WM_LBUTTONUP: {
            ReleaseCapture();
            const int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
            if (g_kb.close_press.target > 0.5f) {
                g_kb.close_press.target = 0.0f;
                if (g_kb.close_rect.contains(x, y)) app_hide_keyboard();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_kb.pin_press.target > 0.5f) {
                g_kb.pin_press.target = 0.0f;
                if (g_kb.pin_rect.contains(x, y)) {
                    g_app.st.keyboard_always_on_top = !g_app.st.keyboard_always_on_top;
                    app_save_settings();
                    keyboard_apply_mode();
                    app_set_status(g_app.st.keyboard_always_on_top
                                       ? (core::is_persian_ui(g_app.st) ? L"صفحه کلید همیشه روی پنجره‌ها"
                                                                       : L"Keyboard stays on top")
                                       : (core::is_persian_ui(g_app.st) ? L"حالت شناور خاموش شد"
                                                                       : L"Always-on-top off"),
                                   color::kTextDim);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            const int key = hit_test(x, y);
            if (key >= 0 && key < static_cast<int>(g_kb.press.size())) {
                g_kb.press[key].target = 0.0f;
                ensure_timer(hwnd);
            }
            if (key >= 0 && key == g_kb.press_key) {
                activate_key(key);
            }
            g_kb.press_key = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_TIMER:
            if (wparam == TIMER_ANIM) {
                float dt = 0.016f;
                bool moving = false;
                for (Anim& a : g_kb.hover) moving |= a.step(dt);
                for (Anim& a : g_kb.press) moving |= a.step(dt);
                moving |= g_kb.pin_hover.step(dt);
                moving |= g_kb.close_hover.step(dt);
                moving |= g_kb.pin_press.step(dt);
                moving |= g_kb.close_press.step(dt);
                InvalidateRect(hwnd, nullptr, FALSE);
                if (!moving) {
                    KillTimer(hwnd, TIMER_ANIM);
                    g_kb.timer_on = false;
                }
                return 0;
            }
            if (wparam == TIMER_TRACK) {
                inject_track_foreground();
                // Repaint only when the target window (shown in the title bar)
                // actually changed; this keeps the keyboard idle at ~0% CPU.
                if (g_app.target != g_kb.last_target ||
                    g_app.target_title != g_kb.last_target_title) {
                    g_kb.last_target = g_app.target;
                    g_kb.last_target_title = g_app.target_title;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            return 0;
        case WM_RBUTTONUP: {
            // Right click on the title bar closes the keyboard.
            const int y = GET_Y_LPARAM(lparam);
            if (y < g_kb.title_h) app_hide_keyboard();
            return 0;
        }
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                app_hide_keyboard();
                return 0;
            }
            if (wparam == VK_SHIFT) {
                g_app.kb_shift = !g_app.kb_shift;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_TRACK);
            KillTimer(hwnd, TIMER_ANIM);
            g_kb.timer_on = false;
            g_kb.surf.release();
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

}  // namespace

bool keyboard_register(HINSTANCE inst) {
    WNDCLASSEXW wc;
    std::memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = keyboard_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassKeyboard;
    wc.style = CS_DBLCLKS;
    if (!RegisterClassExW(&wc)) return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    return true;
}

HWND keyboard_create(HINSTANCE inst) {
    // Size the keyboard relative to the work area so it fits small laptops.
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int screen_w = work.right - work.left;
    int width = std::min(scale_px(1060), static_cast<int>(screen_w * 0.72f));
    width = std::max(width, scale_px(620));
    width = std::min(width, screen_w - scale_px(16));
    const int pad = scale_px(12);
    const int gap = scale_px(6);
    const int unit = (width - pad * 2 - gap * 14) / 15;
    const int key_h = std::max(scale_px(30), static_cast<int>(unit * 0.95f));
    const int height = scale_px(42) + pad * 2 + key_h * 5 + gap * 4;
    const int x = work.left + (screen_w - width) / 2;
    const int y = work.bottom - height - scale_px(40);

    std::wstring title = L"AMIN Keyboard";
    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClassKeyboard, title.c_str(),
                                WS_POPUP | WS_CLIPCHILDREN, x, y, width, height, nullptr, nullptr,
                                inst, nullptr);
    if (!hwnd) {
        app_log(L"keyboard window creation failed (%lu)", GetLastError());
        return nullptr;
    }
    keyboard_apply_mode();
    set_window_rounded_region(hwnd, scale_px(16));
    compute_geometry(width, height);
    return hwnd;
}

void keyboard_apply_mode() {
    if (!g_app.kb) return;
    LONG_PTR ex = GetWindowLongPtrW(g_app.kb, GWL_EXSTYLE);
    ex |= WS_EX_TOOLWINDOW;
    if (g_app.st.keyboard_no_focus) {
        ex |= WS_EX_NOACTIVATE;
    } else {
        ex &= ~static_cast<LONG_PTR>(WS_EX_NOACTIVATE);
    }
    SetWindowLongPtrW(g_app.kb, GWL_EXSTYLE, ex);
    SetWindowPos(g_app.kb,
                 g_app.st.keyboard_always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void keyboard_paint(HWND hwnd) { paint(hwnd); }

void keyboard_set_language(int lang_index) {
    g_app.kb_lang = (lang_index == 0) ? 0 : 1;
    g_app.kb_page = 0;
    if (g_app.kb) InvalidateRect(g_app.kb, nullptr, FALSE);
}

}  // namespace amin
