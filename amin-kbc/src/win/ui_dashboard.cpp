// =============================================================================
//  AMIN Keyboard & Clipboard - src/win/ui_dashboard.cpp
//  Main window: header, Keyboard / Clipboard tabs, quick actions, status card
//  and the first-run setup wizard.
// =============================================================================
#include "amin.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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

enum ControlId {
    C_NONE = 0,
    C_MIN,
    C_CLOSE,
    C_TAB_KEYBOARD,
    C_TAB_CLIPBOARD,
    C_SHOW_KB,
    C_HIDE_KB,
    C_TOGGLE_FLOATING,
    C_TOGGLE_NOFOCUS,
    C_OPEN_CLIP,
    C_STARTUP,
    C_SHORTCUT,
    C_DATA_FOLDER,
    C_WIZARD,
    C_SELFTEST,
    C_EXIT,
    C_PAUSE,
    C_CLEAR,
    C_COPY_DASH,
    C_OPEN_CLIP_WINDOW,
    C_ROW,
    C_WIZ_BACK,
    C_WIZ_NEXT,
    C_WIZ_SKIP,
    C_HOTKEY_CYCLE,
    C_WIZ_STARTUP,
    C_WIZ_SHORTCUT,
};

struct Hit {
    RectI rect;
    int id = C_NONE;
    int index = -1;
};

struct DashWindow {
    Surface surf;
    int width = 0;
    int height = 0;
    std::map<int, Anim> hover;
    std::map<int, Anim> press;
    std::vector<Hit> hits;
    Anim close_hover, close_press, min_hover, min_press;
    Anim tab_offset;
    Anim wizard_in;
    int hover_id = C_NONE;
    int press_id = C_NONE;
    bool tracking_leave = false;
    bool timer_on = false;
    HBRUSH edit_brush = nullptr;
    int list_rows_visible = 5;
    ULONGLONG expired_rendered = 0;
};

DashWindow g_d;

int scale_px(int v) { return static_cast<int>(std::lround(v * g_app.scale)); }
bool persian() { return core::is_persian_ui(g_app.st); }

std::wstring hotkey_status_text() {
    if (!g_app.st.clipboard_hotkey_enabled || g_app.st.clipboard_hotkey == "none") {
        return persian() ? L"غیرفعال" : L"disabled";
    }
    if (g_app.hotkey_clip_ok) return g_app.hotkey_clip_active;
    return persian() ? L"ثبت نشد" : L"not registered";
}

// ---------------------------------------------------------------------------
//  Small painting helpers
// ---------------------------------------------------------------------------
void draw_card(Surface& s, const RectI& rc, uint32_t bg = color::kCard, float radius = 14.0f) {
    s.fill_round(rc.x, rc.y, rc.w, rc.h, radius, bg);
    s.stroke_round(rc.x, rc.y, rc.w, rc.h, radius, 1.0f, color::kBorderSoft);
}

void draw_dot(Surface& s, int x, int y, int r, uint32_t c) {
    s.circle(static_cast<float>(x), static_cast<float>(y), static_cast<float>(r), c);
    s.circle(static_cast<float>(x), static_cast<float>(y), static_cast<float>(r + 3),
             with_alpha(c, 45));
}

void draw_toggle(Surface& s, const RectI& rc, bool on, float hover) {
    const int track_w = scale_px(44);
    const int track_h = scale_px(24);
    const RectI track = {rc.x, rc.y + (rc.h - track_h) / 2, track_w, track_h};
    const uint32_t off = lerp_color(color::kCard, color::kBorder, 1.0f);
    const uint32_t on_c = lerp_color(color::kAccent, color::kAccent2, 0.35f);
    const uint32_t track_color =
        on ? lerp_color(on_c, color::kWhite, hover * 0.08f)
           : lerp_color(lerp_color(color::kBgDeep, color::kBorder, 0.75f), color::kBorder, hover);
    s.fill_round(track.x, track.y, track.w, track.h, track.h * 0.5f, track_color);
    const int knob = track_h - scale_px(6);
    const int knob_x = on ? (track.x + track.w - knob - scale_px(3)) : (track.x + scale_px(3));
    s.circle(knob_x + knob * 0.5f, track.y + track.h * 0.5f, knob * 0.5f,
             on ? color::kWhite : lerp_color(color::kTextFaint, color::kText, hover));
    (void)off;
}

void draw_switch_row(Surface& s, const RectI& rc, bool on, float hover, const std::wstring& label,
                     const std::wstring& sub) {
    draw_toggle(s, rc, on, hover);
    const int text_x = rc.x + scale_px(56);
    RECT lr = {text_x, rc.y + (sub.empty() ? 0 : -scale_px(7)), rc.x + rc.w, rc.y + rc.h};
    draw_text(s.dc(), label, lr, font(FontRole::UiBoldSmall), color::kText, AlignLeft,
              sub.empty() ? VCenter : VTop, persian());
    if (!sub.empty()) {
        RECT sr = {text_x, rc.y + scale_px(13), rc.x + rc.w, rc.y + rc.h + scale_px(6)};
        draw_text(s.dc(), sub, sr, font(FontRole::UiTiny), color::kTextFaint, AlignLeft, VTop,
                  persian());
    }
}

void draw_button(Surface& s, const RectI& rc, int id, const std::wstring& label, Icon icon,
                 bool primary, bool danger, DashWindow& d) {
    const float hover = d.hover[id].value;
    const float press = d.press[id].value;
    const float radius = scale_px(11);
    const int oy = press > 0.4f ? 1 : 0;
    uint32_t bg = primary ? color::kAccent : (danger ? lerp_color(color::kCard, color::kDanger, 0.16f)
                                                    : color::kCard);
    uint32_t fg = primary ? color::kWhite : (danger ? color::kDanger : color::kText);
    if (primary) {
        s.fill_round_grad(rc.x, rc.y + oy, rc.w, rc.h, radius,
                          lerp_color(color::kAccent, color::kWhite, 0.05f + hover * 0.06f),
                          lerp_color(color::kAccent2, color::kBlack, 0.08f), false);
    } else {
        bg = lerp_color(bg, lerp_color(bg, color::kWhite, 0.10f), hover);
        s.fill_round(rc.x, rc.y + oy, rc.w, rc.h, radius, bg);
        s.stroke_round(rc.x, rc.y + oy, rc.w, rc.h, radius, 1.0f,
                       lerp_color(color::kBorder, color::kTextFaint, hover * 0.5f));
    }
    const int icon_size = scale_px(17);
    int text_x = rc.x + scale_px(14);
    if (icon != Icon::None) {
        draw_icon(s, icon, static_cast<float>(rc.x + scale_px(13)),
                  static_cast<float>(rc.y + oy + rc.h * 0.5f - icon_size * 0.5f),
                  static_cast<float>(icon_size), fg);
        text_x = rc.x + scale_px(13) + icon_size + scale_px(8);
    }
    RECT tr = {text_x, rc.y + oy, rc.right() - scale_px(10), rc.y + oy + rc.h};
    draw_text(s.dc(), label, tr, font(FontRole::UiBoldSmall), fg, AlignLeft, VCenter, persian());
}

void draw_segmented(Surface& s, const RectI& rc, int id, const std::wstring& label, Icon icon,
                    bool active, DashWindow& d) {
    const float hover = d.hover[id].value;
    if (active) {
        s.fill_round_grad(rc.x, rc.y, rc.w, rc.h, rc.h * 0.5f,
                          lerp_color(color::kAccent, color::kWhite, 0.04f),
                          lerp_color(color::kAccent2, color::kBlack, 0.10f), false);
    } else if (hover > 0.02f) {
        s.fill_round(rc.x, rc.y, rc.w, rc.h, rc.h * 0.5f,
                     with_alpha(color::kCardHover, static_cast<int>(255 * hover)));
    }
    const uint32_t fg = active ? color::kWhite : lerp_color(color::kTextDim, color::kText, hover);
    const int icon_size = scale_px(16);
    const int text_w = measure_text(s.dc(), label, font(FontRole::UiBoldSmall, 0), persian());
    const int total = icon_size + scale_px(8) + text_w;
    int x = rc.x + (rc.w - total) / 2;
    draw_icon(s, icon, static_cast<float>(x), static_cast<float>(rc.y + rc.h * 0.5f - icon_size * 0.5f),
              static_cast<float>(icon_size), fg);
    x += icon_size + scale_px(8);
    RECT tr = {x, rc.y, rc.right() - scale_px(6), rc.y + rc.h};
    draw_text(s.dc(), label, tr, font(FontRole::UiBoldSmall), fg, AlignLeft, VCenter, persian());
}

// ---------------------------------------------------------------------------
//  Layout
// ---------------------------------------------------------------------------
struct Layout {
    RectI min_btn, close_btn;
    RectI tab_kb, tab_clip;
    RectI primary, hide_btn, floating_row, nofocus_row, open_clip_btn;
    RectI preview_card;
    RectI status_card, settings_card;
    RectI startup_row, shortcut_btn, folder_btn, wizard_btn, selftest_btn, exit_btn;
    RectI clip_search, clip_list, clip_copy, clip_delete, clip_clear, clip_pause, clip_open;
    RectI wiz_card, wiz_back, wiz_next, wiz_skip, wiz_hotkey, wiz_startup_row, wiz_shortcut;
    int header_h = 76;
    int tabs_y = 88;
    int content_y = 140;
    int status_h = 40;
};

Layout compute_layout(int w, int h) {
    Layout L;
    const int pad = scale_px(22);
    L.header_h = scale_px(76);
    L.status_h = scale_px(40);
    L.min_btn = {w - pad - scale_px(72), scale_px(20), scale_px(32), scale_px(32)};
    L.close_btn = {w - pad - scale_px(32), scale_px(20), scale_px(32), scale_px(32)};

    L.tabs_y = scale_px(88);
    L.tab_kb = {pad, L.tabs_y, scale_px(158), scale_px(40)};
    L.tab_clip = {pad + scale_px(166), L.tabs_y, scale_px(158), scale_px(40)};

    L.content_y = L.tabs_y + scale_px(40) + scale_px(18);
    const int content_h = h - L.content_y - L.status_h - scale_px(12);
    const int right_w = scale_px(300);
    const int left_x = pad;
    const int left_w = w - pad * 2 - right_w - scale_px(16);
    const int right_x = left_x + left_w + scale_px(16);

    // Left column
    L.primary = {left_x, L.content_y, left_w, scale_px(88)};
    const int half = (left_w - scale_px(12)) / 2;
    L.hide_btn = {left_x, L.primary.y + L.primary.h + scale_px(14), half, scale_px(46)};
    L.open_clip_btn = {left_x + half + scale_px(12), L.hide_btn.y, half, scale_px(46)};
    L.floating_row = {left_x, L.hide_btn.y + L.hide_btn.h + scale_px(18), left_w, scale_px(34)};
    L.nofocus_row = {left_x, L.floating_row.y + scale_px(46), left_w, scale_px(34)};
    L.preview_card = {left_x, L.nofocus_row.y + scale_px(58), left_w,
                      std::max(scale_px(96), L.content_y + content_h - (L.nofocus_row.y + scale_px(58)))};

    // Right column
    L.status_card = {right_x, L.content_y, right_w, scale_px(184)};
    L.settings_card = {right_x, L.status_card.y + L.status_card.h + scale_px(14), right_w,
                       L.content_y + content_h - (L.status_card.y + L.status_card.h + scale_px(14))};

    const int inner_x = L.settings_card.x + scale_px(14);
    const int inner_w = L.settings_card.w - scale_px(28);
    int y = L.settings_card.y + scale_px(40);
    L.startup_row = {inner_x, y, inner_w, scale_px(30)};
    y += scale_px(42);
    const int btn_h = scale_px(34);
    const int btn_gap = scale_px(8);
    const int two_w = (inner_w - btn_gap) / 2;
    L.shortcut_btn = {inner_x, y, two_w, btn_h};
    L.folder_btn = {inner_x + two_w + btn_gap, y, two_w, btn_h};
    y += btn_h + btn_gap;
    L.wizard_btn = {inner_x, y, two_w, btn_h};
    L.selftest_btn = {inner_x + two_w + btn_gap, y, two_w, btn_h};
    y += btn_h + btn_gap;
    L.exit_btn = {inner_x, y, inner_w, btn_h};

    // Clipboard tab (replaces left column content visually)
    const int search_h = scale_px(38);
    L.clip_search = {left_x, L.content_y, left_w, search_h};
    const int clip_buttons_y = L.content_y + content_h - scale_px(46);
    // Leave room for the privacy note below the list (it sits between the list
    // and the button row).
    L.clip_list = {left_x, L.clip_search.y + search_h + scale_px(12), left_w,
                   clip_buttons_y - scale_px(32) - (L.clip_search.y + search_h + scale_px(12))};
    const int cw = scale_px(96);
    L.clip_copy = {left_x, clip_buttons_y, cw, scale_px(46)};
    L.clip_clear = {L.clip_copy.x + cw + scale_px(8), clip_buttons_y, scale_px(112), scale_px(46)};
    L.clip_open = {left_x + left_w - scale_px(180), clip_buttons_y, scale_px(180), scale_px(46)};
    L.clip_pause = {L.clip_clear.x + scale_px(112) + scale_px(8), clip_buttons_y,
                    std::max(scale_px(96), L.clip_open.x - scale_px(8) - (L.clip_clear.x + scale_px(112) + scale_px(8))),
                    scale_px(46)};

    // Wizard card (centered over the content area)
    const int wiz_w = std::min(scale_px(600), w - scale_px(80));
    const int wiz_h = std::min(scale_px(420), h - scale_px(120));
    L.wiz_card = {(w - wiz_w) / 2, L.content_y + scale_px(6), wiz_w, wiz_h};
    const int wp = scale_px(26);
    const int wb = scale_px(38);
    L.wiz_skip = {L.wiz_card.x + wp, L.wiz_card.y + wiz_h - wp - wb, scale_px(90), wb};
    L.wiz_next = {L.wiz_card.x + wiz_w - wp - scale_px(110), L.wiz_card.y + wiz_h - wp - wb,
                  scale_px(110), wb};
    L.wiz_back = {L.wiz_next.x - scale_px(10) - scale_px(90), L.wiz_next.y, scale_px(90), wb};
    L.wiz_hotkey = {L.wiz_card.x + wp, L.wiz_card.y + scale_px(206), L.wiz_card.w - wp * 2,
                    scale_px(52)};
    L.wiz_startup_row = {L.wiz_card.x + wp, L.wiz_card.y + scale_px(196), L.wiz_card.w - wp * 2,
                         scale_px(32)};
    L.wiz_shortcut = {L.wiz_card.x + wp, L.wiz_card.y + scale_px(250), scale_px(210),
                      scale_px(36)};
    return L;
}

// ---------------------------------------------------------------------------
//  Paint sections
// ---------------------------------------------------------------------------
void paint_header(Surface& s, const Layout& L, int w) {
    s.fill_rect_grad(0, 0, w, L.header_h, 0xFF171D2C, color::kBg);
    const int logo = scale_px(40);
    const int lx = scale_px(22), ly = scale_px(18);
    s.fill_round_grad(lx, ly, logo, logo, logo * 0.3f, color::kAccent, color::kAccent2, true);
    draw_icon(s, Icon::Keyboard, static_cast<float>(lx + logo * 0.22f),
              static_cast<float>(ly + logo * 0.22f), logo * 0.56f, color::kWhite);

    RECT title_rc = {lx + logo + scale_px(14), ly - scale_px(4), w - scale_px(120),
                     ly + scale_px(26)};
    draw_text(s.dc(), kAppNameFa, title_rc, font(FontRole::PersianTitle), color::kText, AlignLeft,
              VCenter, true);
    RECT sub_rc = {lx + logo + scale_px(14), ly + scale_px(22), w - scale_px(120),
                   ly + scale_px(42)};
    draw_text(s.dc(), L"AMIN Keyboard & Clipboard", sub_rc, font(FontRole::UiSmall),
              color::kTextFaint, AlignLeft, VCenter, false);

    auto window_button = [&](const RectI& rc, const Anim& hover, Icon icon, bool danger) {
        if (hover.value > 0.02f) {
            s.fill_round(rc.x, rc.y, rc.w, rc.h, rc.h * 0.28f,
                         with_alpha(danger ? color::kDanger : color::kCardHover,
                                    static_cast<int>(190 * hover.value)));
        }
        const float size = rc.h * 0.42f;
        draw_icon(s, icon, rc.x + rc.w * 0.5f - size * 0.5f, rc.y + rc.h * 0.5f - size * 0.5f,
                  size, lerp_color(color::kTextDim, danger ? color::kDanger : color::kText,
                                   hover.value));
    };
    window_button(L.min_btn, g_d.min_hover, Icon::Minimize, false);
    window_button(L.close_btn, g_d.close_hover, Icon::Close, true);
    s.fill_rect(0, L.header_h - 1, w, 1, color::kBorderSoft);
}

void paint_status_card(Surface& s, const Layout& L) {
    draw_card(s, L.status_card, color::kCard, scale_px(14));
    const int pad = scale_px(16);
    RECT title = {L.status_card.x + pad, L.status_card.y + scale_px(12),
                  L.status_card.right() - pad, L.status_card.y + scale_px(34)};
    draw_text(s.dc(), T.status, title, font(FontRole::UiBoldSmall), color::kTextDim, AlignLeft,
              VCenter, persian());

    int y = L.status_card.y + scale_px(44);
    const int row_h = scale_px(32);
    const int dot_x = L.status_card.x + pad + scale_px(4);
    const int label_x = L.status_card.x + pad + scale_px(16);
    const int value_right = L.status_card.right() - pad;

    // Clipboard capture
    draw_dot(s, dot_x, y + row_h / 2, scale_px(3),
             g_app.st.clipboard_monitoring ? color::kSuccess : color::kWarn);
    RECT r1 = {label_x, y, value_right, y + row_h};
    draw_text(s.dc(), persian() ? L"ثبت کلیپ‌بورد" : L"Clipboard capture", r1,
              font(FontRole::UiSmall), color::kText, AlignLeft, VCenter, persian());
    draw_text(s.dc(), g_app.st.clipboard_monitoring ? (persian() ? L"فعال" : L"on")
                                                    : (persian() ? L"متوقف" : L"paused"),
              r1, font(FontRole::UiSmall), color::kTextDim, AlignRight, VCenter, persian());
    y += row_h;

    // Hotkey
    draw_dot(s, dot_x, y + row_h / 2, scale_px(3),
             g_app.hotkey_clip_ok ? color::kSuccess : color::kDanger);
    RECT r2 = {label_x, y, value_right, y + row_h};
    draw_text(s.dc(), T.hotkey_label, r2, font(FontRole::UiSmall), color::kText, AlignLeft,
              VCenter, persian());
    draw_text(s.dc(), hotkey_status_text(), r2, font(FontRole::UiSmall),
              g_app.hotkey_clip_ok ? color::kTextDim : color::kDanger, AlignRight, VCenter,
              persian());
    y += row_h;

    // Startup
    const bool startup = startup_is_enabled();
    draw_dot(s, dot_x, y + row_h / 2, scale_px(3), startup ? color::kSuccess : color::kTextFaint);
    RECT r3 = {label_x, y, value_right, y + row_h};
    draw_text(s.dc(), T.start_with_windows, r3, font(FontRole::UiSmall), color::kText, AlignLeft,
              VCenter, persian());
    draw_text(s.dc(), startup ? (persian() ? L"روشن" : L"on") : (persian() ? L"خاموش" : L"off"),
              r3, font(FontRole::UiSmall), color::kTextDim, AlignRight, VCenter, persian());
    y += row_h;

    // Target window
    RECT r4 = {L.status_card.x + pad, y + scale_px(2), value_right,
               L.status_card.y + L.status_card.h - scale_px(6)};
    const std::wstring target_text =
        g_app.target_title.empty()
            ? T.none
            : wstr(core::make_preview(u16of(g_app.target_title), 26));
    draw_text(s.dc(), T.sent_to, r4, font(FontRole::UiTiny), color::kTextFaint, AlignLeft, VTop,
              persian());
    RECT r5 = {L.status_card.x + pad, y + scale_px(17), value_right,
               L.status_card.y + L.status_card.h - scale_px(4)};
    draw_text(s.dc(), target_text, r5, font(FontRole::PersianSmall), color::kTextDim, AlignLeft,
              VTop, persian());

    // Hotkey problem explanation (if any)
    if (!g_app.hotkey_clip_ok && !g_app.hotkey_clip_requested.empty()) {
        RECT warn = {L.status_card.x + pad, L.status_card.y + L.status_card.h - scale_px(28),
                     value_right, L.status_card.y + L.status_card.h - scale_px(6)};
        draw_text(s.dc(), T.hotkey_win_v_note, warn, font(FontRole::UiTiny), color::kWarn,
                  AlignLeft, VTop, persian());
    }
}

void paint_settings_card(Surface& s, const Layout& L) {
    draw_card(s, L.settings_card, color::kCard, scale_px(14));
    RECT title = {L.settings_card.x + scale_px(16), L.settings_card.y + scale_px(12),
                  L.settings_card.right() - scale_px(16), L.settings_card.y + scale_px(34)};
    draw_text(s.dc(), T.settings, title, font(FontRole::UiBoldSmall), color::kTextDim, AlignLeft,
              VCenter, persian());

    draw_switch_row(s, L.startup_row, startup_is_enabled(), g_d.hover[C_STARTUP].value,
                    T.start_with_windows, persian() ? L"بدون نیاز به دسترسی مدیر" : L"no admin rights");
    draw_button(s, L.shortcut_btn, C_SHORTCUT, T.create_shortcut, Icon::Spark, false, false, g_d);
    draw_button(s, L.folder_btn, C_DATA_FOLDER, T.data_folder, Icon::Clipboard, false, false, g_d);
    draw_button(s, L.wizard_btn, C_WIZARD, T.run_setup, Icon::Gear, false, false, g_d);
    draw_button(s, L.selftest_btn, C_SELFTEST, T.diagnostics, Icon::Check, false, false, g_d);
    draw_button(s, L.exit_btn, C_EXIT, T.exit, Icon::Power, false, true, g_d);
}

void paint_keyboard_tab(Surface& s, const Layout& L) {
    // Primary action
    {
        const RectI& rc = L.primary;
        const float hover = g_d.hover[C_SHOW_KB].value;
        s.fill_round_grad(rc.x, rc.y, rc.w, rc.h, scale_px(16),
                          lerp_color(0xFF7C5CFF, color::kWhite, hover * 0.10f),
                          lerp_color(0xFF4D8DFF, color::kBlack, 0.05f), false);
        const int icon_size = scale_px(30);
        draw_icon(s, Icon::Keyboard, static_cast<float>(rc.x + scale_px(22)),
                  static_cast<float>(rc.y + rc.h * 0.5f - icon_size * 0.5f),
                  static_cast<float>(icon_size), color::kWhite);
        RECT t1 = {rc.x + scale_px(22) + icon_size + scale_px(14), rc.y + scale_px(18),
                   rc.right() - scale_px(16), rc.y + scale_px(48)};
        draw_text(s.dc(), T.show_keyboard, t1, font(FontRole::PersianTitle), color::kWhite,
                  AlignLeft, VCenter, true);
        RECT t2 = {t1.left, rc.y + scale_px(48), rc.right() - scale_px(16), rc.y + scale_px(70)};
        const std::wstring sub = persian() ? L"Show keyboard" : T.show_keyboard;
        draw_text(s.dc(), sub, t2, font(FontRole::UiSmall), with_alpha(color::kWhite, 210),
                  AlignLeft, VCenter, false);
        // Chevron hint
        draw_icon(s, Icon::ChevronRight, static_cast<float>(rc.right() - scale_px(30)),
                  static_cast<float>(rc.y + rc.h * 0.5f - scale_px(9)), static_cast<float>(scale_px(18)),
                  with_alpha(color::kWhite, 200));
    }
    draw_button(s, L.hide_btn, C_HIDE_KB, T.hide_keyboard, Icon::Close, false, false, g_d);
    draw_button(s, L.open_clip_btn, C_OPEN_CLIP, T.open_clipboard, Icon::Clipboard, false, false,
                g_d);

    draw_switch_row(s, L.floating_row, g_app.st.keyboard_always_on_top,
                    g_d.hover[C_TOGGLE_FLOATING].value, T.floating,
                    persian() ? L"On top of other windows" : L"همیشه روی پنجره‌ها");
    draw_switch_row(s, L.nofocus_row, g_app.st.keyboard_no_focus,
                    g_d.hover[C_TOGGLE_NOFOCUS].value, T.no_focus,
                    persian() ? L"Never takes keyboard focus" : L"هرگز فوکوس را نمی‌دزدد");

    // Preview card
    draw_card(s, L.preview_card, color::kCard, scale_px(14));
    RECT pt = {L.preview_card.x + scale_px(16), L.preview_card.y + scale_px(10),
               L.preview_card.right() - scale_px(16), L.preview_card.y + scale_px(32)};
    draw_text(s.dc(), T.preview_title, pt, font(FontRole::UiBoldSmall), color::kTextDim, AlignLeft,
              VCenter, persian());
    const RectI box = {L.preview_card.x + scale_px(14), L.preview_card.y + scale_px(38),
                       L.preview_card.w - scale_px(28), L.preview_card.h - scale_px(74)};
    s.fill_round(box.x, box.y, box.w, box.h, scale_px(10), color::kBgDeep);
    s.stroke_round(box.x, box.y, box.w, box.h, scale_px(10), 1.0f, color::kBorderSoft);
    if (g_app.preview_text.empty()) {
        RECT hint = {box.x + scale_px(12), box.y, box.right() - scale_px(12), box.bottom()};
        draw_text(s.dc(), T.preview_hint, hint, font(FontRole::UiSmall), color::kTextFaint,
                  AlignLeft, VCenter, persian());
    } else {
        const std::wstring shown = wstr(core::make_preview(u16of(g_app.preview_text), 400));
        RECT vr = {box.x + scale_px(12), box.y, box.right() - scale_px(12), box.bottom()};
        draw_text(s.dc(), shown, vr,
                  font_for_text(shown, text_is_rtl(shown) ? FontRole::PersianLabel : FontRole::Ui),
                  color::kText, text_is_rtl(shown) ? AlignRight : AlignLeft, VCenter,
                  text_is_rtl(shown));
    }
    RECT note = {L.preview_card.x + scale_px(4), L.preview_card.bottom() - scale_px(30),
                 L.preview_card.right(), L.preview_card.bottom() - scale_px(10)};
    draw_text(s.dc(), T.preview_hint, note, font(FontRole::UiTiny), color::kTextFaint, AlignLeft,
              VCenter, persian());
}

void paint_clipboard_tab(Surface& s, const Layout& L) {
    // Search field background (the EDIT child paints itself)
    s.fill_round(L.clip_search.x, L.clip_search.y, L.clip_search.w, L.clip_search.h, scale_px(10),
                 color::kPanel);
    s.stroke_round(L.clip_search.x, L.clip_search.y, L.clip_search.w, L.clip_search.h,
                   scale_px(10), 1.0f, color::kBorder);
    draw_icon(s, Icon::Search, static_cast<float>(L.clip_search.x + scale_px(12)),
              static_cast<float>(L.clip_search.y + L.clip_search.h * 0.5f - scale_px(8)),
              static_cast<float>(scale_px(16)), color::kTextFaint);

    draw_card(s, L.clip_list, color::kPanel, scale_px(12));

    // Rows (newest first, no filtering here - the full window has search)
    const auto& entries = g_app.store.entries();
    const int row_h = scale_px(56);
    g_d.list_rows_visible = std::max(1, L.clip_list.h / row_h);
    const int count = std::min<int>(static_cast<int>(entries.size()), g_d.list_rows_visible);
    if (count == 0) {
        RECT er = {L.clip_list.x + scale_px(16), L.clip_list.y, L.clip_list.right() - scale_px(16),
                   L.clip_list.bottom()};
        draw_text(s.dc(), T.empty_history, er, font(FontRole::Ui), color::kTextFaint, AlignCenter,
                  VCenter, persian());
    }
    for (int i = 0; i < count; ++i) {
        const RectI row = {L.clip_list.x + scale_px(6), L.clip_list.y + scale_px(6) + i * row_h,
                           L.clip_list.w - scale_px(12), row_h - scale_px(4)};
        if (i == g_app.clip_sel) {
            s.fill_round(row.x, row.y, row.w, row.h, scale_px(9), color::kAccentSoft);
        }
        const core::ClipEntry& e = entries[i];
        const bool rtl = core::has_rtl(e.text);
        const std::wstring preview = wstr(core::make_preview(e.text, 72));
        RECT lr = {row.x + scale_px(12), row.y + scale_px(6), row.right() - scale_px(12),
                   row.y + scale_px(30)};
        draw_text(s.dc(), preview, lr,
                  font_for_text(preview, rtl ? FontRole::PersianLabel : FontRole::Ui), color::kText,
                  rtl ? AlignRight : AlignLeft, VCenter, rtl);
        RECT mr = {row.x + scale_px(12), row.y + scale_px(28), row.right() - scale_px(12),
                   row.y + row.h - scale_px(4)};
        const std::wstring meta = wstr(core::format_number(static_cast<int64_t>(e.text.size()),
                                                          persian())) +
                                  L" " + T.chars;
        draw_text(s.dc(), meta, mr, font(FontRole::UiTiny), color::kTextFaint,
                  rtl ? AlignRight : AlignLeft, VCenter, persian());
        Hit hit;
        hit.rect = row;
        hit.id = C_ROW;
        hit.index = i;
        g_d.hits.push_back(hit);
    }

    draw_button(s, L.clip_copy, C_COPY_DASH, T.copy, Icon::Copy, true, false, g_d);
    draw_button(s, L.clip_clear, C_CLEAR, T.clear_all, Icon::Trash, false, false, g_d);
    draw_button(s, L.clip_pause, C_PAUSE,
                g_app.st.clipboard_monitoring ? T.pause : T.resume,
                g_app.st.clipboard_monitoring ? Icon::Pause : Icon::Play, false, false, g_d);
    draw_button(s, L.clip_open, C_OPEN_CLIP_WINDOW, T.clipboard_title, Icon::ChevronRight, false,
                false, g_d);

    RECT note = {L.clip_list.x, L.clip_list.bottom() + scale_px(4), L.clip_list.right(),
                 L.clip_list.bottom() + scale_px(22)};
    // (clip_list ends 32 px above the buttons, so this never overlaps them)
    draw_text(s.dc(), T.privacy, note, font(FontRole::UiTiny), color::kTextFaint, AlignLeft,
              VCenter, persian());
}

void paint_wizard(Surface& s, const Layout& L, int w, int h) {
    if (!g_app.wizard_open) return;
    // Scrim (one alpha fill; the per-pixel loop was needlessly slow)
    s.fill_rect(0, 0, w, h, with_alpha(color::kBlack, 150));
    const RectI& card = L.wiz_card;
    s.shadow_round(card.x, card.y, card.w, card.h, scale_px(18), scale_px(24), color::kBlack, 0.5f);
    s.fill_round(card.x, card.y, card.w, card.h, scale_px(18), color::kCard);
    s.stroke_round(card.x, card.y, card.w, card.h, scale_px(18), 1.0f, color::kBorder);

    const int pad = scale_px(26);
    const int step = g_app.wizard_step;
    const int icon_size = scale_px(34);
    s.fill_round_grad(card.x + pad, card.y + pad, icon_size, icon_size, icon_size * 0.3f,
                      color::kAccent, color::kAccent2, true);
    draw_icon(s, step == 0 ? Icon::Spark : (step == 1 ? Icon::Globe : Icon::Check),
              static_cast<float>(card.x + pad + icon_size * 0.22f),
              static_cast<float>(card.y + pad + icon_size * 0.22f), icon_size * 0.56f,
              color::kWhite);

    // Step dots
    for (int i = 0; i < 3; ++i) {
        const int dot_r = scale_px(4);
        const int dx = card.right() - pad - (2 - i) * scale_px(16);
        s.circle(static_cast<float>(dx), static_cast<float>(card.y + pad + scale_px(12)),
                 static_cast<float>(dot_r), i == step ? color::kAccent : color::kBorder);
    }

    const int text_x = card.x + pad + icon_size + scale_px(16);
    RECT title = {text_x, card.y + pad - scale_px(2), card.right() - pad - scale_px(60),
                  card.y + pad + scale_px(26)};
    RECT body = {card.x + pad, card.y + pad + icon_size + scale_px(18), card.right() - pad,
                 card.y + card.h - scale_px(90)};

    const std::wstring title_text =
        step == 0 ? (persian() ? L"خوش آمدید" : L"Welcome")
                  : (step == 1 ? (persian() ? L"میان‌بر کلیپ‌بورد" : L"Clipboard shortcut")
                               : (persian() ? L"آماده است" : L"All set"));
    draw_text(s.dc(), title_text, title, font(FontRole::PersianTitle), color::kText, AlignLeft,
              VCenter, true);

    if (step == 0) {
        const std::wstring text =
            persian()
                ? L"AMIN Keyboard & Clipboard یک صفحه کلید فارسی/انگلیسی و یک تاریخچه کلیپ‌بورد "
                  L"سبک برای ویندوز ۱۰ است.\n\n• متن‌ها فقط روی همین رایانه ذخیره می‌شوند و هیچ "
                  L"داده‌ای ارسال نمی‌شود.\n• نیازی به دسترسی مدیر نیست.\n• هر کلیدی که در صفحه "
                  L"کلید می‌زنید به پنجره‌ای می‌رود که قبلاً فعال بوده است."
                : L"AMIN Keyboard & Clipboard bundles a Persian/English on-screen keyboard and a "
                  L"lightweight clipboard history for Windows 10.\n\n• Everything is stored on "
                  L"this PC only; nothing is uploaded.\n• No administrator rights are required.\n"
                  L"• Keys are sent to the window that was active before the keyboard opened.";
        draw_text_wrapped(s.dc(), text, body, font(FontRole::PersianLabel), color::kTextDim, 0,
                          persian());
    } else if (step == 1) {
        const std::wstring text =
            persian()
                ? L"برای باز/بستن کلیپ‌بورد یک کلید میان‌بر سراسری لازم است. ویندوز ۱۰ نسخه ۱۸۰۹ و "
                  L"بالاتر کلید Win+V را برای خودش نگه می‌دارد، بنابراین برنامه ابتدا Win+V و در صورت "
                  L"ناموفق بودن Ctrl+Alt+V را امتحان می‌کند.\n\nوضعیت واقعی ثبت‌شده:"
                : L"A global shortcut opens/closes the clipboard. Windows 10 1809+ reserves "
                  L"Win+V, so the app tries Win+V first and falls back to Ctrl+Alt+V.\n\n"
                  L"Actually registered:";
        draw_text_wrapped(s.dc(), text, body, font(FontRole::PersianLabel), color::kTextDim, 0,
                          persian());
        draw_card(s, L.wiz_hotkey, color::kBgDeep, scale_px(10));
        RECT hr = {L.wiz_hotkey.x + scale_px(14), L.wiz_hotkey.y, L.wiz_hotkey.right() - scale_px(14),
                   L.wiz_hotkey.y + scale_px(30)};
        const std::wstring hk = g_app.hotkey_clip_ok ? g_app.hotkey_clip_active : hotkey_status_text();
        draw_text(s.dc(), hk.empty() ? L"-" : hk, hr, font(FontRole::UiLarge),
                  g_app.hotkey_clip_ok ? color::kSuccess : color::kDanger, AlignLeft, VCenter,
                  false);
        RECT hr2 = {L.wiz_hotkey.x + scale_px(14), L.wiz_hotkey.y + scale_px(28),
                    L.wiz_hotkey.right() - scale_px(14), L.wiz_hotkey.y + scale_px(48)};
        draw_text(s.dc(),
                  g_app.hotkey_clip_ok ? (persian() ? L"این میان‌بر واقعاً فعال است"
                                                   : L"this shortcut is really registered")
                                       : (persian() ? L"کلید میان‌بر ثبت نشد؛ از دکمه زیر استفاده کنید"
                                                   : L"not registered - use the button below"),
                  hr2, font(FontRole::UiTiny),
                  g_app.hotkey_clip_ok ? color::kTextFaint : color::kWarn, AlignLeft, VCenter,
                  persian());
        // Small pill inside the hotkey row
        const RectI pill = {L.wiz_hotkey.right() - scale_px(150), L.wiz_hotkey.y + scale_px(9),
                            scale_px(136), scale_px(34)};
        draw_button(s, pill, C_HOTKEY_CYCLE, persian() ? L"تغییر" : L"Change", Icon::Spark, false,
                    false, g_d);
    } else {
        const std::wstring text =
            persian()
                ? L"می‌توانید برنامه را با ویندوز اجرا کنید و یک میان‌بر روی دسکتاپ بسازید. هر دو "
                  L"مورد بدون دسترسی مدیر انجام می‌شود و هر زمان قابل برگشت است."
                : L"Optionally start the app with Windows and add a desktop shortcut. Both are "
                  L"per-user, need no administrator rights and can be undone at any time.";
        draw_text_wrapped(s.dc(), text, body, font(FontRole::PersianLabel), color::kTextDim, 0,
                          persian());
        draw_switch_row(s, L.wiz_startup_row, startup_is_enabled(), g_d.hover[C_WIZ_STARTUP].value,
                        T.start_with_windows, L"");
        draw_button(s, L.wiz_shortcut, C_WIZ_SHORTCUT, T.create_shortcut, Icon::Spark, false, false,
                    g_d);
        RECT note = {L.wiz_shortcut.right() + scale_px(10), L.wiz_shortcut.y,
                     L.wiz_card.right() - pad, L.wiz_shortcut.bottom()};
        draw_text(s.dc(), T.startup_note, note, font(FontRole::UiTiny), color::kTextFaint,
                  AlignLeft, VCenter, persian());
    }

    draw_button(s, L.wiz_skip, C_WIZ_SKIP, T.skip, Icon::None, false, false, g_d);
    if (step > 0) draw_button(s, L.wiz_back, C_WIZ_BACK, T.back, Icon::ChevronLeft, false, false, g_d);
    draw_button(s, L.wiz_next, C_WIZ_NEXT, step == 2 ? T.finish : T.next, Icon::ChevronRight, true,
                false, g_d);
}

void paint(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    const int w = client.right, h = client.bottom;
    if (w <= 0 || h <= 0) return;
    const Layout L = compute_layout(w, h);

    HDC screen = GetDC(hwnd);
    if (!g_d.surf.resize(screen, w, h)) {
        ReleaseDC(hwnd, screen);
        return;
    }
    Surface& s = g_d.surf;
    s.clear(color::kBg);
    g_d.hits.clear();

    paint_header(s, L, w);

    // Tabs segmented background
    const RectI tabs_bg = {L.tab_kb.x, L.tab_kb.y, L.tab_kb.w + scale_px(8) + L.tab_clip.w,
                           L.tab_kb.h};
    s.fill_round(tabs_bg.x, tabs_bg.y, tabs_bg.w, tabs_bg.h, tabs_bg.h * 0.5f, color::kPanel);
    draw_segmented(s, L.tab_kb, C_TAB_KEYBOARD, T.tab_keyboard, Icon::Keyboard,
                   g_app.dash_tab == 0, g_d);
    draw_segmented(s, L.tab_clip, C_TAB_CLIPBOARD, T.tab_clipboard, Icon::Clipboard,
                   g_app.dash_tab == 1, g_d);

    if (g_app.dash_tab == 0) {
        paint_keyboard_tab(s, L);
        paint_status_card(s, L);
        paint_settings_card(s, L);
    } else {
        paint_clipboard_tab(s, L);
        paint_status_card(s, L);
        paint_settings_card(s, L);
    }

    // Status bar
    s.fill_rect(0, h - L.status_h, w, 1, color::kBorderSoft);
    const bool has_status = !g_app.status_text.empty() && g_app.status_until != 0 &&
                            GetTickCount64() < g_app.status_until;
    const std::wstring status_text = has_status ? g_app.status_text : T.ready;
    const uint32_t status_color = has_status ? g_app.status_color : color::kTextFaint;
    s.circle(static_cast<float>(scale_px(28)), static_cast<float>(h - L.status_h / 2),
             static_cast<float>(scale_px(3)), status_color);
    RECT status_text_rc = {scale_px(38), h - L.status_h, w - scale_px(300), h};
    draw_text(s.dc(), status_text, status_text_rc, font(FontRole::UiSmall), status_color,
              AlignLeft, VCenter, persian());
    RECT version_rc = {w - scale_px(300), h - L.status_h, w - scale_px(24), h};
    const std::wstring version = std::wstring(L"v") + kAppVersion + L"   \u00b7   " +
                                 (persian() ? L"ذخیره محلی" : L"local only");
    draw_text(s.dc(), version, version_rc, font(FontRole::UiTiny), color::kTextFaint, AlignRight,
              VCenter, persian());

    // Hits for tab bar buttons and window buttons are registered here
    {
        Hit h1;
        h1.rect = L.min_btn;
        h1.id = C_MIN;
        g_d.hits.push_back(h1);
        h1.rect = L.close_btn;
        h1.id = C_CLOSE;
        g_d.hits.push_back(h1);
        h1.rect = L.tab_kb;
        h1.id = C_TAB_KEYBOARD;
        g_d.hits.push_back(h1);
        h1.rect = L.tab_clip;
        h1.id = C_TAB_CLIPBOARD;
        g_d.hits.push_back(h1);
        if (g_app.dash_tab == 0) {
            h1.rect = L.primary;
            h1.id = C_SHOW_KB;
            g_d.hits.push_back(h1);
            h1.rect = L.hide_btn;
            h1.id = C_HIDE_KB;
            g_d.hits.push_back(h1);
            h1.rect = L.open_clip_btn;
            h1.id = C_OPEN_CLIP;
            g_d.hits.push_back(h1);
            h1.rect = L.floating_row;
            h1.id = C_TOGGLE_FLOATING;
            g_d.hits.push_back(h1);
            h1.rect = L.nofocus_row;
            h1.id = C_TOGGLE_NOFOCUS;
            g_d.hits.push_back(h1);
        } else {
            // Clipboard tab hits are pushed while drawing rows; add buttons here.
            h1.rect = L.clip_copy;
            h1.id = C_COPY_DASH;
            g_d.hits.push_back(h1);
            h1.rect = L.clip_clear;
            h1.id = C_CLEAR;
            g_d.hits.push_back(h1);
            h1.rect = L.clip_pause;
            h1.id = C_PAUSE;
            g_d.hits.push_back(h1);
            h1.rect = L.clip_open;
            h1.id = C_OPEN_CLIP_WINDOW;
            g_d.hits.push_back(h1);
        }
        h1.rect = L.startup_row;
        h1.id = C_STARTUP;
        g_d.hits.push_back(h1);
        h1.rect = L.shortcut_btn;
        h1.id = C_SHORTCUT;
        g_d.hits.push_back(h1);
        h1.rect = L.folder_btn;
        h1.id = C_DATA_FOLDER;
        g_d.hits.push_back(h1);
        h1.rect = L.wizard_btn;
        h1.id = C_WIZARD;
        g_d.hits.push_back(h1);
        h1.rect = L.selftest_btn;
        h1.id = C_SELFTEST;
        g_d.hits.push_back(h1);
        h1.rect = L.exit_btn;
        h1.id = C_EXIT;
        g_d.hits.push_back(h1);
    }

    paint_wizard(s, L, w, h);
    if (g_app.wizard_open) {
        Hit h;
        h.rect = L.wiz_next;
        h.id = C_WIZ_NEXT;
        g_d.hits.push_back(h);
        h.rect = L.wiz_back;
        h.id = C_WIZ_BACK;
        g_d.hits.push_back(h);
        h.rect = L.wiz_skip;
        h.id = C_WIZ_SKIP;
        g_d.hits.push_back(h);
        if (g_app.wizard_step == 1) {
            h.rect = L.wiz_hotkey;
            h.id = C_HOTKEY_CYCLE;
            g_d.hits.push_back(h);
        }
        if (g_app.wizard_step == 2) {
            h.rect = L.wiz_startup_row;
            h.id = C_WIZ_STARTUP;
            g_d.hits.push_back(h);
            h.rect = L.wiz_shortcut;
            h.id = C_WIZ_SHORTCUT;
            g_d.hits.push_back(h);
        }
    }

    s.blit(screen, 0, 0);
    ReleaseDC(hwnd, screen);
}

// ---------------------------------------------------------------------------
//  Actions
// ---------------------------------------------------------------------------
void cycle_hotkey() {
    const std::vector<std::string> options = core::selectable_clipboard_hotkeys();
    size_t index = 0;
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == g_app.st.clipboard_hotkey) {
            index = i;
            break;
        }
    }
    index = (index + 1) % options.size();
    g_app.st.clipboard_hotkey = options[index];
    g_app.st.clipboard_hotkey_enabled = (options[index] != "none");
    app_register_hotkeys();
    app_save_settings(true);
    app_set_status(g_app.hotkey_clip_ok
                       ? (persian() ? L"میان‌بر فعال شد: " + g_app.hotkey_clip_active
                                    : L"Shortcut active: " + g_app.hotkey_clip_active)
                       : (persian() ? L"این ترکیب قابل ثبت نبود" : L"That combination could not be registered"),
                   g_app.hotkey_clip_ok ? color::kSuccess : color::kDanger);
    if (g_app.dash) InvalidateRect(g_app.dash, nullptr, FALSE);
}

void toggle_setting(bool* value) {
    *value = !*value;
    app_save_settings(true);
    app_refresh_ui();
}

void handle_control(HWND hwnd, int id, int index) {
    switch (id) {
        case C_MIN:
            ShowWindow(hwnd, SW_MINIMIZE);
            break;
        case C_CLOSE:
            if (g_app.st.minimize_to_tray) {
                app_hide_dashboard();
            } else {
                app_exit();
            }
            break;
        case C_TAB_KEYBOARD:
        case C_TAB_CLIPBOARD: {
            const int new_tab = (id == C_TAB_KEYBOARD) ? 0 : 1;
            if (new_tab != g_app.dash_tab) {
                g_d.tab_offset.value = (new_tab > g_app.dash_tab) ? 14.0f : -14.0f;
                g_d.tab_offset.target = 0.0f;
                g_app.dash_tab = new_tab;
                if (new_tab == 1) {
                    g_app.clip_sel = 0;
                }
                if (!g_d.timer_on) {
                    SetTimer(hwnd, TIMER_ANIM, 16, nullptr);
                    g_d.timer_on = true;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            break;
        }
        case C_SHOW_KB:
            app_show_keyboard(true);
            if (!g_app.target) {
                app_set_status(core::is_persian_ui(g_app.st)
                                   ? L"پنجره مقصد مشخص نیست؛ ابتدا روی برنامه دلخواه کلیک کنید یا "
                                     L"حالت «بدون فوکوس» را روشن کنید"
                                   : L"No target window yet: click the app you want to type into "
                                     L"first, or enable no-focus mode",
                               color::kWarn);
            }
            break;
        case C_HIDE_KB: app_hide_keyboard(); break;
        case C_OPEN_CLIP: app_show_clipboard(true); break;
        case C_TOGGLE_FLOATING:
            toggle_setting(&g_app.st.keyboard_always_on_top);
            app_apply_keyboard_mode();
            app_set_status(g_app.st.keyboard_always_on_top
                               ? (persian() ? L"صفحه کلید همیشه روی پنجره‌ها نمایش داده می‌شود"
                                            : L"Keyboard stays on top")
                               : (persian() ? L"حالت شناور خاموش شد" : L"Always-on-top off"),
                           color::kTextDim);
            break;
        case C_TOGGLE_NOFOCUS:
            toggle_setting(&g_app.st.keyboard_no_focus);
            app_apply_keyboard_mode();
            app_set_status(g_app.st.keyboard_no_focus
                               ? (persian() ? L"حالت بدون فوکوس فعال شد؛ صفحه کلید فوکوس را نمی‌دزدد"
                                            : L"No-focus mode on: the keyboard never takes focus")
                               : (persian() ? L"حالت عادی: صفحه کلید فوکوس می‌گیرد"
                                            : L"Normal mode: the keyboard takes focus"),
                           color::kTextDim);
            break;
        case C_STARTUP:
        case C_WIZ_STARTUP:
            app_apply_startup(!startup_is_enabled());
            break;
        case C_SHORTCUT:
        case C_WIZ_SHORTCUT: {
            std::wstring err;
            if (shell_create_desktop_shortcut(&err)) {
                app_set_status(persian() ? L"میان‌بر روی دسکتاپ ساخته شد"
                                         : L"Desktop shortcut created",
                               color::kSuccess);
            } else {
                message_box(hwnd, (persian() ? L"ساخت میان‌بر ممکن نشد:\n" : L"Could not create the shortcut:\n") + err,
                            T.error_title, MB_ICONERROR | MB_OK);
                app_set_status(persian() ? L"ساخت میان‌بر ناموفق بود" : L"Shortcut creation failed",
                               color::kDanger);
            }
            break;
        }
        case C_DATA_FOLDER: shell_open_data_folder(); break;
        case C_WIZARD:
            g_app.wizard_open = true;
            g_app.wizard_step = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case C_SELFTEST: {
            std::wstring report;
            const int failures = selftest_run(false, &report);
            const std::wstring path = path_join(g_app.data_dir, L"selftest.log");
            std::wstring text = persian()
                                    ? (failures == 0 ? L"همه بررسی‌ها موفق بود.\n" : L"برخی بررسی‌ها ناموفق بود.\n")
                                    : (failures == 0 ? L"All checks passed.\n" : L"Some checks failed.\n");
            text += (persian() ? L"گزارش کامل: " : L"Full report: ") + path;
            message_box(hwnd, text, T.diagnostics, MB_ICONINFORMATION | MB_OK);
            break;
        }
        case C_EXIT:
            app_exit();
            break;
        case C_ROW:
            if (g_app.clip_sel != index) {
                g_app.clip_sel = index;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            app_copy_entry(static_cast<size_t>(index), false);
            break;
        case C_COPY_DASH:
            app_copy_entry(static_cast<size_t>(std::max(0, g_app.clip_sel)), false);
            break;
        case C_CLEAR:
            if (!g_app.store.empty()) {
                if (MessageBoxW(hwnd, T.confirm_clear.c_str(), T.clear_all.c_str(),
                                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES) {
                    g_app.store.clear();
                    app_store_changed(false);
                    app_save_history(true);
                    app_set_status(T.cleared, color::kWarn);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            break;
        case C_PAUSE:
            g_app.st.clipboard_monitoring = !g_app.st.clipboard_monitoring;
            app_save_settings(true);
            tray_update_tooltip();
            app_set_status(g_app.st.clipboard_monitoring ? T.resumed_msg : T.paused_msg,
                           g_app.st.clipboard_monitoring ? color::kSuccess : color::kWarn);
            app_refresh_ui();
            break;
        case C_OPEN_CLIP_WINDOW: app_show_clipboard(true); break;
        case C_WIZ_BACK:
            if (g_app.wizard_step > 0) --g_app.wizard_step;
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case C_WIZ_NEXT:
            if (g_app.wizard_step < 2) {
                ++g_app.wizard_step;
            } else {
                g_app.wizard_open = false;
                g_app.st.wizard_done = true;
                app_save_settings(true);
                app_set_status(persian() ? L"راه‌اندازی کامل شد" : L"Setup complete",
                               color::kSuccess);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case C_WIZ_SKIP:
            g_app.wizard_open = false;
            g_app.st.wizard_done = true;
            app_save_settings(true);
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case C_HOTKEY_CYCLE:
            cycle_hotkey();
            break;
        default:
            break;
    }
}

int hit_test(int x, int y) {
    // Wizard is modal: only wizard hit areas are active while it is open.
    if (g_app.wizard_open) {
        for (const Hit& h : g_d.hits) {
            if ((h.id == C_WIZ_NEXT || h.id == C_WIZ_BACK || h.id == C_WIZ_SKIP ||
                 h.id == C_HOTKEY_CYCLE || h.id == C_WIZ_STARTUP || h.id == C_WIZ_SHORTCUT) &&
                h.rect.contains(x, y)) {
                return h.id;
            }
        }
        return C_NONE;
    }
    for (const Hit& h : g_d.hits) {
        if (h.rect.contains(x, y)) {
            if (h.id == C_ROW) return C_ROW + 1000;  // encode
            return h.id;
        }
    }
    return C_NONE;
}

int hit_test_row(int x, int y) {
    for (const Hit& h : g_d.hits) {
        if (h.id == C_ROW && h.rect.contains(x, y)) return h.index;
    }
    return -1;
}

// ---------------------------------------------------------------------------
//  Window procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK dashboard_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE: {
            g_d.edit_brush = CreateSolidBrush(to_colorref(color::kPanel));
            SetTimer(hwnd, TIMER_STATUS, 2500, nullptr);
            return 0;
        }
        case WM_SIZE: {
            RECT client;
            GetClientRect(hwnd, &client);
            g_d.width = client.right;
            g_d.height = client.bottom;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lparam);
            mmi->ptMinTrackSize.x = scale_px(860);
            mmi->ptMinTrackSize.y = scale_px(600);
            return 0;
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
        case WM_NCHITTEST: {
            POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &pt);
            const Layout L = compute_layout(g_d.width, g_d.height);
            if (pt.y >= 0 && pt.y < L.header_h && pt.x < L.min_btn.x - scale_px(10)) {
                return HTCAPTION;
            }
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetTextColor(dc, to_colorref(color::kText));
            SetBkColor(dc, to_colorref(color::kPanel));
            return reinterpret_cast<LRESULT>(g_d.edit_brush ? g_d.edit_brush
                                                            : GetStockObject(BLACK_BRUSH));
        }
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
            const int id = hit_test(x, y);
            const int plain_id = (id >= C_ROW + 1000) ? C_ROW : id;
            bool changed = false;
            if (plain_id != g_d.hover_id) {
                g_d.hover_id = plain_id;
                changed = true;
            }
            const bool over_min = true;  // handled by window buttons below
            (void)over_min;
            const Layout L = compute_layout(g_d.width, g_d.height);
            auto set_hover = [&](const RectI& rc, Anim& a) {
                const float target = rc.contains(x, y) ? 1.0f : 0.0f;
                if (std::fabs(a.target - target) > 0.01f) {
                    a.target = target;
                    changed = true;
                }
            };
            set_hover(L.min_btn, g_d.min_hover);
            set_hover(L.close_btn, g_d.close_hover);
            set_hover(L.primary, g_d.hover[C_SHOW_KB]);
            set_hover(L.hide_btn, g_d.hover[C_HIDE_KB]);
            set_hover(L.open_clip_btn, g_d.hover[C_OPEN_CLIP]);
            set_hover(L.floating_row, g_d.hover[C_TOGGLE_FLOATING]);
            set_hover(L.nofocus_row, g_d.hover[C_TOGGLE_NOFOCUS]);
            set_hover(L.startup_row, g_d.hover[C_STARTUP]);
            set_hover(L.shortcut_btn, g_d.hover[C_SHORTCUT]);
            set_hover(L.folder_btn, g_d.hover[C_DATA_FOLDER]);
            set_hover(L.wizard_btn, g_d.hover[C_WIZARD]);
            set_hover(L.selftest_btn, g_d.hover[C_SELFTEST]);
            set_hover(L.exit_btn, g_d.hover[C_EXIT]);
            set_hover(L.tab_kb, g_d.hover[C_TAB_KEYBOARD]);
            set_hover(L.tab_clip, g_d.hover[C_TAB_CLIPBOARD]);
            set_hover(L.clip_copy, g_d.hover[C_COPY_DASH]);
            set_hover(L.clip_clear, g_d.hover[C_CLEAR]);
            set_hover(L.clip_pause, g_d.hover[C_PAUSE]);
            set_hover(L.clip_open, g_d.hover[C_OPEN_CLIP_WINDOW]);
            if (g_app.wizard_open) {
                set_hover(L.wiz_next, g_d.hover[C_WIZ_NEXT]);
                set_hover(L.wiz_back, g_d.hover[C_WIZ_BACK]);
                set_hover(L.wiz_skip, g_d.hover[C_WIZ_SKIP]);
                set_hover(L.wiz_hotkey, g_d.hover[C_HOTKEY_CYCLE]);
                set_hover(L.wiz_startup_row, g_d.hover[C_WIZ_STARTUP]);
                set_hover(L.wiz_shortcut, g_d.hover[C_WIZ_SHORTCUT]);
            }
            if (!g_d.tracking_leave) {
                TRACKMOUSEEVENT tme;
                std::memset(&tme, 0, sizeof(tme));
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                g_d.tracking_leave = true;
            }
            SetCursor(LoadCursorW(nullptr, (plain_id != C_NONE) ? IDC_HAND : IDC_ARROW));
            if (changed) {
                if (!g_d.timer_on) {
                    SetTimer(hwnd, TIMER_ANIM, 16, nullptr);
                    g_d.timer_on = true;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            g_d.tracking_leave = false;
            g_d.hover_id = C_NONE;
            for (auto& kv : g_d.hover) kv.second.target = 0.0f;
            g_d.min_hover.target = 0;
            g_d.close_hover.target = 0;
            if (!g_d.timer_on) {
                SetTimer(hwnd, TIMER_ANIM, 16, nullptr);
                g_d.timer_on = true;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            const int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
            int id = hit_test(x, y);
            if (id >= C_ROW + 1000) {
                id = C_ROW;
            }
            g_d.press_id = id;
            if (id != C_NONE) {
                g_d.press[id].target = 1.0f;
                if (!g_d.timer_on) {
                    SetTimer(hwnd, TIMER_ANIM, 16, nullptr);
                    g_d.timer_on = true;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            const int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
            int id = hit_test(x, y);
            int row = -1;
            if (id >= C_ROW + 1000) {
                row = hit_test_row(x, y);
                id = C_ROW;
            }
            if (g_d.press_id != C_NONE) g_d.press[g_d.press_id].target = 0.0f;
            if (id != C_NONE && id == g_d.press_id) {
                handle_control(hwnd, id, row);
            }
            g_d.press_id = C_NONE;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_TIMER:
            if (wparam == TIMER_ANIM) {
                bool moving = false;
                for (auto& kv : g_d.hover) moving |= kv.second.step(0.016f);
                for (auto& kv : g_d.press) moving |= kv.second.step(0.016f);
                moving |= g_d.min_hover.step(0.016f);
                moving |= g_d.close_hover.step(0.016f);
                moving |= g_d.tab_offset.step(0.016f);
                moving |= g_d.wizard_in.step(0.016f);
                InvalidateRect(hwnd, nullptr, FALSE);
                if (!moving) {
                    KillTimer(hwnd, TIMER_ANIM);
                    g_d.timer_on = false;
                }
                return 0;
            }
            if (wparam == TIMER_STATUS) {
                // Repaint only when the status line really changed.
                if (IsWindowVisible(hwnd) && g_app.status_until != g_d.expired_rendered) {
                    if (GetTickCount64() > g_app.status_until) {
                        g_d.expired_rendered = g_app.status_until;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                return 0;
            }
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                if (g_app.wizard_open) {
                    g_app.wizard_open = false;
                    g_app.st.wizard_done = true;
                    app_save_settings(true);
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (g_app.st.minimize_to_tray) {
                    app_hide_dashboard();
                }
                return 0;
            }
            return 0;
        case WM_CLOSE:
            if (g_app.st.minimize_to_tray) {
                app_hide_dashboard();
            } else {
                app_exit();
            }
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, TIMER_ANIM);
            KillTimer(hwnd, TIMER_STATUS);
            g_d.timer_on = false;
            if (g_d.edit_brush) {
                DeleteObject(g_d.edit_brush);
                g_d.edit_brush = nullptr;
            }
            g_d.surf.release();
            g_app.dash_search = nullptr;
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

}  // namespace

bool dashboard_register(HINSTANCE inst) {
    WNDCLASSEXW wc;
    std::memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = dashboard_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassDashboard;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    return true;
}

HWND dashboard_create(HINSTANCE inst, bool visible) {
    const int width = scale_px(980);
    const int height = scale_px(640);
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int screen_w = work.right - work.left, screen_h = work.bottom - work.top;
    const int x = work.left + std::max(0, (screen_w - width) / 2);
    const int y = work.top + std::max(0, (screen_h - height) / 2);

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, kClassDashboard, kAppName,
                                WS_POPUP | WS_CLIPCHILDREN | WS_SYSMENU, x, y,
                                std::min(width, screen_w - scale_px(20)),
                                std::min(height, screen_h - scale_px(20)), nullptr, nullptr, inst,
                                nullptr);
    if (!hwnd) {
        app_log(L"dashboard creation failed (%lu)", GetLastError());
        return nullptr;
    }
    g_d.width = width;
    g_d.height = height;
    set_window_rounded_region(hwnd, scale_px(16));
    if (visible) {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    }
    return hwnd;
}

void dashboard_paint(HWND hwnd) { paint(hwnd); }

void dashboard_refresh(HWND hwnd) {
    if (hwnd) InvalidateRect(hwnd, nullptr, FALSE);
}

void dashboard_resize_children(HWND hwnd) {
    if (!hwnd) return;
    RECT client;
    GetClientRect(hwnd, &client);
    g_d.width = client.right;
    g_d.height = client.bottom;
}

}  // namespace amin
