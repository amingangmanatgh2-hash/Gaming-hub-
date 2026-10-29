// =============================================================================
//  AMIN Keyboard & Clipboard - src/win/gfx.cpp
//  Software renderer (32bpp DIB) + text + vector icons.
//
//  Everything is drawn with GDI into an off-screen 32-bit DIB and blitted in one
//  go: no flicker, and shapes are anti-aliased with analytic coverage so the UI
//  stays crisp on any DPI without relying on GDI+ / Direct2D (which would need
//  newer Windows versions on some code paths).
//
//  Persian/Arabic text is shaped with Uniscribe (usp10.dll, present since
//  Windows 2000) because plain GDI does not do contextual Arabic shaping.
// =============================================================================
#include "amin.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace amin {

// ---------------------------------------------------------------------------
//  Colour helpers
// ---------------------------------------------------------------------------
uint32_t rgba(int r, int g, int b, int a) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}
int color_a(uint32_t c) { return static_cast<int>((c >> 24) & 0xFF); }
int color_r(uint32_t c) { return static_cast<int>((c >> 16) & 0xFF); }
int color_g(uint32_t c) { return static_cast<int>((c >> 8) & 0xFF); }
int color_b(uint32_t c) { return static_cast<int>(c & 0xFF); }

uint32_t with_alpha(uint32_t c, int alpha) {
    return (c & 0x00FFFFFFu) | (static_cast<uint32_t>(std::max(0, std::min(255, alpha))) << 24);
}

uint32_t lerp_color(uint32_t a, uint32_t b, float t) {
    t = clampf(t, 0.0f, 1.0f);
    const int aa = color_a(a), ar = color_r(a), ag = color_g(a), ab = color_b(a);
    const int ba = color_a(b), br = color_r(b), bg = color_g(b), bb = color_b(b);
    return rgba(static_cast<int>(ar + (br - ar) * t + 0.5f),
                static_cast<int>(ag + (bg - ag) * t + 0.5f),
                static_cast<int>(ab + (bb - ab) * t + 0.5f),
                static_cast<int>(aa + (ba - aa) * t + 0.5f));
}

COLORREF to_colorref(uint32_t c) { return RGB(color_r(c), color_g(c), color_b(c)); }

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float ease_out_cubic(float t) {
    t = clampf(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

bool Anim::step(float dt) {
    if (value == target) return false;
    // Frame-rate independent exponential approach (~90 ms to settle).
    const float k = 1.0f - std::exp(-dt * 18.0f);
    value += (target - value) * k;
    if (std::fabs(target - value) < 0.0015f) value = target;
    return true;
}

// ---------------------------------------------------------------------------
//  Surface
// ---------------------------------------------------------------------------
Surface::~Surface() { release(); }

void Surface::release() {
    if (dc_) {
        if (old_bmp_) SelectObject(dc_, old_bmp_);
        DeleteDC(dc_);
        dc_ = nullptr;
    }
    old_bmp_ = nullptr;
    if (bmp_) {
        DeleteObject(bmp_);
        bmp_ = nullptr;
    }
    bits_ = nullptr;
    w_ = h_ = 0;
}

bool Surface::resize(HDC ref, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (w == w_ && h == h_ && bits_) return true;
    release();

    BITMAPINFO bi;
    std::memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    dc_ = CreateCompatibleDC(ref);
    if (!dc_) return false;
    bmp_ = CreateDIBSection(dc_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp_ || !bits) {
        release();
        return false;
    }
    old_bmp_ = static_cast<HBITMAP>(SelectObject(dc_, bmp_));
    bits_ = bits;
    w_ = w;
    h_ = h;
    return true;
}

void Surface::clear(uint32_t c) { fill_rect(0, 0, w_, h_, c); }

void Surface::blend_span(int y, int x0, int x1, uint32_t color, float alpha) {
    if (y < 0 || y >= h_ || alpha <= 0.0f) return;
    if (x0 < 0) x0 = 0;
    if (x1 > w_) x1 = w_;
    if (x0 >= x1) return;
    uint32_t* row = static_cast<uint32_t*>(bits_) + static_cast<size_t>(y) * static_cast<size_t>(w_);
    const float a = clampf(alpha, 0.0f, 1.0f);
    const float ia = 1.0f - a;
    const int sr = color_r(color), sg = color_g(color), sb = color_b(color);
    for (int x = x0; x < x1; ++x) {
        const uint32_t d = row[x];
        const int dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
        const int r = static_cast<int>(sr * a + dr * ia + 0.5f);
        const int g = static_cast<int>(sg * a + dg * ia + 0.5f);
        const int b = static_cast<int>(sb * a + db * ia + 0.5f);
        row[x] = 0xFF000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) |
                 static_cast<uint32_t>(b);
    }
}

void Surface::blend_px(int x, int y, uint32_t color, float alpha) {
    blend_span(y, x, x + 1, color, alpha);
}

void Surface::fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    const int x0 = std::max(0, x), x1 = std::min(w_, x + w);
    const int y0 = std::max(0, y), y1 = std::min(h_, y + h);
    const float a = static_cast<float>(color_a(color)) / 255.0f;
    if (a >= 0.999f) {
        // Fast path: opaque fill.
        for (int yy = y0; yy < y1; ++yy) {
            uint32_t* row = static_cast<uint32_t*>(bits_) +
                            static_cast<size_t>(yy) * static_cast<size_t>(w_);
            const uint32_t px = 0xFF000000u | (color & 0x00FFFFFFu);
            for (int xx = x0; xx < x1; ++xx) row[xx] = px;
        }
    } else {
        for (int yy = y0; yy < y1; ++yy) blend_span(yy, x0, x1, color, a);
    }
}

void Surface::fill_rect_grad(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    if (w <= 0 || h <= 0) return;
    const int y0 = std::max(0, y), y1 = std::min(h_, y + h);
    for (int yy = y0; yy < y1; ++yy) {
        const float t = static_cast<float>(yy - y) / static_cast<float>(std::max(1, h - 1));
        const uint32_t c = lerp_color(top, bottom, t);
        blend_span(yy, x, x + w, c, static_cast<float>(color_a(c)) / 255.0f);
    }
}

// Signed distance from a point to a rounded rectangle (negative inside).
static inline float round_rect_sdf(float px, float py, float left, float top, float w, float h,
                                   float r) {
    const float cx = left + w * 0.5f;
    const float cy = top + h * 0.5f;
    const float hw = w * 0.5f - r;
    const float hh = h * 0.5f - r;
    const float dx = std::fabs(px - cx) - std::max(0.0f, hw);
    const float dy = std::fabs(py - cy) - std::max(0.0f, hh);
    const float ax = std::max(dx, 0.0f), ay = std::max(dy, 0.0f);
    return std::sqrt(ax * ax + ay * ay) + std::min(std::max(dx, dy), 0.0f) - r;
}

void Surface::fill_round(int x, int y, int w, int h, float radius, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    const float r = std::min(radius, std::min(w, h) * 0.5f);
    const float base_alpha = static_cast<float>(color_a(color)) / 255.0f;
    const int x0 = std::max(0, x), x1 = std::min(w_, x + w);
    const int y0 = std::max(0, y), y1 = std::min(h_, y + h);
    if (x1 <= x0 || y1 <= y0) return;

    const int corner_h = std::min(h, static_cast<int>(std::ceil(r)));
    const int mid_x0 = x + static_cast<int>(std::floor(r));
    const int mid_x1 = x + w - static_cast<int>(std::floor(r));

    for (int yy = y0; yy < y1; ++yy) {
        const int row = yy - y;
        const bool corner_row = (row < corner_h) || (row >= h - corner_h);
        if (!corner_row) {
            // Middle band: a single opaque/alpha span, no per-pixel distance.
            blend_span(yy, x0, x1, color, base_alpha);
            continue;
        }
        const float py = static_cast<float>(yy) + 0.5f;
        // Left and right rounded corners.
        const int c_end = std::min(x1, std::max(x0, mid_x0));
        for (int xx = x0; xx < c_end; ++xx) {
            const float px = static_cast<float>(xx) + 0.5f;
            const float d = round_rect_sdf(px, py, static_cast<float>(x), static_cast<float>(y),
                                           static_cast<float>(w), static_cast<float>(h), r);
            const float cov = clampf(0.5f - d, 0.0f, 1.0f);
            if (cov > 0.0f) blend_px(xx, yy, color, cov * base_alpha);
        }
        const int c_start = std::min(x1, std::max(x0, mid_x1));
        for (int xx = c_start; xx < x1; ++xx) {
            const float px = static_cast<float>(xx) + 0.5f;
            const float d = round_rect_sdf(px, py, static_cast<float>(x), static_cast<float>(y),
                                           static_cast<float>(w), static_cast<float>(h), r);
            const float cov = clampf(0.5f - d, 0.0f, 1.0f);
            if (cov > 0.0f) blend_px(xx, yy, color, cov * base_alpha);
        }
        // Between the corners the row is fully inside the shape.
        if (mid_x1 > mid_x0) {
            blend_span(yy, std::max(x0, mid_x0), std::min(x1, mid_x1), color, base_alpha);
        }
    }
}

void Surface::fill_round_grad(int x, int y, int w, int h, float radius, uint32_t c1, uint32_t c2,
                              bool vertical) {
    if (w <= 0 || h <= 0) return;
    const float r = std::min(radius, std::min(w, h) * 0.5f);
    const int x0 = std::max(0, x), x1 = std::min(w_, x + w);
    const int y0 = std::max(0, y), y1 = std::min(h_, y + h);
    if (x1 <= x0 || y1 <= y0) return;
    const int corner_h = std::min(h, static_cast<int>(std::ceil(r)));

    for (int yy = y0; yy < y1; ++yy) {
        const int row = yy - y;
        const float py = static_cast<float>(yy) + 0.5f;
        const float tv = clampf((py - y) / static_cast<float>(std::max(1, h)), 0.0f, 1.0f);
        for (int xx = x0; xx < x1; ++xx) {
            const bool corner_row = (row < corner_h) || (row >= h - corner_h);
            float cov = 1.0f;
            if (corner_row) {
                const float px = static_cast<float>(xx) + 0.5f;
                const float d = round_rect_sdf(
                    px, py, static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
                    static_cast<float>(h), r);
                cov = clampf(0.5f - d, 0.0f, 1.0f);
                if (cov <= 0.0f) continue;
            }
            const float t = vertical
                                ? tv
                                : clampf((xx - x) / static_cast<float>(std::max(1, w)), 0.0f, 1.0f);
            const uint32_t c = lerp_color(c1, c2, t);
            blend_px(xx, yy, c, cov * (static_cast<float>(color_a(c)) / 255.0f));
        }
    }
}

void Surface::stroke_round(int x, int y, int w, int h, float radius, float thickness,
                           uint32_t color) {
    if (w <= 0 || h <= 0 || thickness <= 0.0f) return;
    const float r = std::min(radius, std::min(w, h) * 0.5f);
    const int x0 = std::max(0, x - 2), x1 = std::min(w_, x + w + 2);
    const int y0 = std::max(0, y - 2), y1 = std::min(h_, y + h + 2);
    const float half = thickness * 0.5f;
    const float base_alpha = static_cast<float>(color_a(color)) / 255.0f;
    for (int yy = y0; yy < y1; ++yy) {
        const float py = static_cast<float>(yy) + 0.5f;
        for (int xx = x0; xx < x1; ++xx) {
            const float px = static_cast<float>(xx) + 0.5f;
            const float d = round_rect_sdf(px, py, static_cast<float>(x), static_cast<float>(y),
                                           static_cast<float>(w), static_cast<float>(h), r);
            const float cov = clampf(0.5f - (std::fabs(d) - half), 0.0f, 1.0f);
            if (cov > 0.0f) blend_px(xx, yy, color, cov * base_alpha);
        }
    }
}

void Surface::shadow_round(int x, int y, int w, int h, float radius, int spread, uint32_t color,
                           float opacity) {
    if (spread <= 0 || opacity <= 0.0f) return;
    const float r = std::min(radius, std::min(w, h) * 0.5f);
    const int x0 = std::max(0, x - spread), x1 = std::min(w_, x + w + spread);
    const int y0 = std::max(0, y - spread), y1 = std::min(h_, y + h + spread);
    const float base_alpha = static_cast<float>(color_a(color)) / 255.0f * opacity;
    for (int yy = y0; yy < y1; ++yy) {
        const float py = static_cast<float>(yy) + 0.5f;
        for (int xx = x0; xx < x1; ++xx) {
            const float px = static_cast<float>(xx) + 0.5f;
            const float d = round_rect_sdf(px, py, static_cast<float>(x), static_cast<float>(y),
                                           static_cast<float>(w), static_cast<float>(h), r);
            if (d <= 0.0f) continue;
            const float t = 1.0f - clampf(d / static_cast<float>(spread), 0.0f, 1.0f);
            const float a = base_alpha * t * t;
            if (a > 0.002f) blend_px(xx, yy, color, a);
        }
    }
}

void Surface::line(float x0f, float y0f, float x1f, float y1f, float thickness, uint32_t color) {
    const float half = thickness * 0.5f;
    const int bx0 = std::max(0, static_cast<int>(std::floor(std::min(x0f, x1f) - half - 1)));
    const int bx1 = std::min(w_ - 1, static_cast<int>(std::ceil(std::max(x0f, x1f) + half + 1)));
    const int by0 = std::max(0, static_cast<int>(std::floor(std::min(y0f, y1f) - half - 1)));
    const int by1 = std::min(h_ - 1, static_cast<int>(std::ceil(std::max(y0f, y1f) + half + 1)));
    const float base_alpha = static_cast<float>(color_a(color)) / 255.0f;
    const float vx = x1f - x0f, vy = y1f - y0f;
    const float len2 = vx * vx + vy * vy;
    for (int yy = by0; yy <= by1; ++yy) {
        for (int xx = bx0; xx <= bx1; ++xx) {
            const float px = static_cast<float>(xx) + 0.5f, py = static_cast<float>(yy) + 0.5f;
            float t = 0.0f;
            if (len2 > 1e-6f) t = clampf(((px - x0f) * vx + (py - y0f) * vy) / len2, 0.0f, 1.0f);
            const float dx = px - (x0f + vx * t), dy = py - (y0f + vy * t);
            const float d = std::sqrt(dx * dx + dy * dy);
            const float cov = clampf(0.5f - (d - half), 0.0f, 1.0f);
            if (cov > 0.0f) blend_px(xx, yy, color, cov * base_alpha);
        }
    }
}

void Surface::polyline(const float* points, int count, float thickness, uint32_t color) {
    if (count < 2) return;
    for (int i = 0; i + 1 < count; ++i) {
        line(points[i * 2], points[i * 2 + 1], points[i * 2 + 2], points[i * 2 + 3], thickness,
             color);
    }
    // Round the joints so corners look smooth.
    for (int i = 0; i < count; ++i) {
        circle(points[i * 2], points[i * 2 + 1], thickness * 0.5f, color);
    }
}

void Surface::circle(float cx, float cy, float r, uint32_t color) {
    if (r <= 0.0f) return;
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - r - 1)));
    const int x1 = std::min(w_ - 1, static_cast<int>(std::ceil(cx + r + 1)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - r - 1)));
    const int y1 = std::min(h_ - 1, static_cast<int>(std::ceil(cy + r + 1)));
    const float base_alpha = static_cast<float>(color_a(color)) / 255.0f;
    for (int yy = y0; yy <= y1; ++yy) {
        for (int xx = x0; xx <= x1; ++xx) {
            const float dx = static_cast<float>(xx) + 0.5f - cx;
            const float dy = static_cast<float>(yy) + 0.5f - cy;
            const float d = std::sqrt(dx * dx + dy * dy) - r;
            const float cov = clampf(0.5f - d, 0.0f, 1.0f);
            if (cov > 0.0f) blend_px(xx, yy, color, cov * base_alpha);
        }
    }
}

void Surface::ring(float cx, float cy, float r, float thickness, uint32_t color) {
    const float half = thickness * 0.5f;
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - r - half - 1)));
    const int x1 = std::min(w_ - 1, static_cast<int>(std::ceil(cx + r + half + 1)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - r - half - 1)));
    const int y1 = std::min(h_ - 1, static_cast<int>(std::ceil(cy + r + half + 1)));
    const float base_alpha = static_cast<float>(color_a(color)) / 255.0f;
    for (int yy = y0; yy <= y1; ++yy) {
        for (int xx = x0; xx <= x1; ++xx) {
            const float dx = static_cast<float>(xx) + 0.5f - cx;
            const float dy = static_cast<float>(yy) + 0.5f - cy;
            const float d = std::fabs(std::sqrt(dx * dx + dy * dy) - r) - half;
            const float cov = clampf(0.5f - d, 0.0f, 1.0f);
            if (cov > 0.0f) blend_px(xx, yy, color, cov * base_alpha);
        }
    }
}

void Surface::triangle(float ax, float ay, float bx, float by, float cx, float cy, uint32_t color) {
    const int x0 = std::max(0, static_cast<int>(std::floor(std::min({ax, bx, cx}))));
    const int x1 = std::min(w_ - 1, static_cast<int>(std::ceil(std::max({ax, bx, cx}))));
    const int y0 = std::max(0, static_cast<int>(std::floor(std::min({ay, by, cy}))));
    const int y1 = std::min(h_ - 1, static_cast<int>(std::ceil(std::max({ay, by, cy}))));
    const float e = 0.0001f;
    auto area = [](float px, float py, float x1_, float y1_, float x2_, float y2_) {
        return (x1_ - px) * (y2_ - py) - (x2_ - px) * (y1_ - py);
    };
    for (int yy = y0; yy <= y1; ++yy) {
        for (int xx = x0; xx <= x1; ++xx) {
            // 2x2 supersampling for smooth edges.
            int hits = 0;
            for (int sy = 0; sy < 2; ++sy) {
                for (int sx = 0; sx < 2; ++sx) {
                    const float px = static_cast<float>(xx) + 0.25f + 0.5f * sx;
                    const float py = static_cast<float>(yy) + 0.25f + 0.5f * sy;
                    const float w0 = area(px, py, bx, by, cx, cy);
                    const float w1 = area(px, py, cx, cy, ax, ay);
                    const float w2 = area(px, py, ax, ay, bx, by);
                    if ((w0 >= -e && w1 >= -e && w2 >= -e) || (w0 <= e && w1 <= e && w2 <= e)) ++hits;
                }
            }
            if (hits) blend_px(xx, yy, color, static_cast<float>(hits) / 4.0f);
        }
    }
}

void Surface::blit(HDC dst, int x, int y) {
    if (!bits_ || !dc_) return;
    BitBlt(dst, x, y, w_, h_, dc_, 0, 0, SRCCOPY);
}

// ---------------------------------------------------------------------------
//  Fonts
// ---------------------------------------------------------------------------
namespace {

struct FontSlot {
    HFONT handle = nullptr;
    const wchar_t* face = L"Segoe UI";
    int px = 14;
    int weight = FW_NORMAL;
};

const wchar_t* const kFaceUi = L"Segoe UI";
const wchar_t* const kFaceFa = L"Tahoma";

struct RoleSpec {
    const wchar_t* face;
    int px;
    int weight;
};

RoleSpec role_spec(FontRole role) {
    switch (role) {
        case FontRole::Ui: return {kFaceUi, 14, FW_NORMAL};
        case FontRole::UiSmall: return {kFaceUi, 12, FW_NORMAL};
        case FontRole::UiTiny: return {kFaceUi, 11, FW_NORMAL};
        case FontRole::UiMedium: return {kFaceUi, 16, FW_NORMAL};
        case FontRole::UiLarge: return {kFaceUi, 19, FW_SEMIBOLD};
        case FontRole::UiTitle: return {kFaceUi, 25, FW_SEMIBOLD};
        case FontRole::UiBold: return {kFaceUi, 14, FW_SEMIBOLD};
        case FontRole::UiBoldSmall: return {kFaceUi, 12, FW_SEMIBOLD};
        case FontRole::PersianKey: return {kFaceFa, 19, FW_NORMAL};
        case FontRole::PersianLabel: return {kFaceFa, 14, FW_NORMAL};
        case FontRole::PersianSmall: return {kFaceFa, 12, FW_NORMAL};
        case FontRole::PersianTitle: return {kFaceFa, 18, FW_BOLD};
        default: return {kFaceUi, 14, FW_NORMAL};
    }
}

const int kRoleCount = static_cast<int>(FontRole::RoleCount);
std::vector<HFONT> g_extra_fonts;
FontSlot g_fonts[kRoleCount];
int g_font_dpi = 0;

// Fallback fonts used when the primary face lacks a glyph.
const wchar_t* const kFallbackFaces[] = {L"Segoe UI", L"Tahoma", L"Arial", L"Segoe UI Symbol",
                                         L"Times New Roman"};

HDC g_measure_dc = nullptr;

HDC measure_dc() {
    if (!g_measure_dc) {
        HDC screen = GetDC(nullptr);
        g_measure_dc = CreateCompatibleDC(screen);
        ReleaseDC(nullptr, screen);
    }
    return g_measure_dc;
}

bool font_has_all_glyphs(HFONT f, const std::wstring& text) {
    if (text.empty()) return true;
    HDC dc = measure_dc();
    if (!dc) return true;
    HGDIOBJ old = SelectObject(dc, f);
    std::vector<WORD> glyphs(text.size());
    const DWORD flags = GGI_MARK_NONEXISTING_GLYPHS;
    const DWORD n = GetGlyphIndicesW(dc, text.c_str(), static_cast<int>(text.size()), glyphs.data(),
                                     flags);
    SelectObject(dc, old);
    if (n == GDI_ERROR) return true;  // be permissive
    for (size_t i = 0; i < text.size(); ++i) {
        if (glyphs[i] == 0xFFFF) return false;
    }
    return true;
}

struct CacheKey {
    std::wstring text;
    int role;
    int delta;
    bool operator==(const CacheKey& o) const {
        return role == o.role && delta == o.delta && text == o.text;
    }
};
struct CacheHash {
    size_t operator()(const CacheKey& k) const {
        size_t h = static_cast<size_t>(k.role) * 131u + static_cast<size_t>(k.delta);
        for (wchar_t c : k.text) h = h * 31u + static_cast<size_t>(c);
        return h;
    }
};
std::unordered_map<CacheKey, HFONT, CacheHash> g_font_cache;

}  // namespace

bool fonts_init(int dpi) {
    fonts_free();
    g_font_dpi = dpi;
    const float scale = static_cast<float>(dpi) / 96.0f;
    for (int i = 0; i < kRoleCount; ++i) {
        const RoleSpec spec = role_spec(static_cast<FontRole>(i));
        const int px = std::max(8, static_cast<int>(std::lround(spec.px * scale)));
        HFONT f = CreateFontW(-px, 0, 0, 0, spec.weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, spec.face);
        if (!f) return false;
        g_fonts[i].handle = f;
        g_fonts[i].face = spec.face;
        g_fonts[i].px = px;
        g_fonts[i].weight = spec.weight;
    }
    return true;
}

void fonts_free() {
    for (int i = 0; i < kRoleCount; ++i) {
        if (g_fonts[i].handle) {
            DeleteObject(g_fonts[i].handle);
            g_fonts[i].handle = nullptr;
        }
    }
    g_font_cache.clear();
    for (HFONT f : g_extra_fonts) DeleteObject(f);
    g_extra_fonts.clear();
    if (g_measure_dc) {
        DeleteDC(g_measure_dc);
        g_measure_dc = nullptr;
    }
}

HFONT font(FontRole role, int px_delta) {
    const int i = static_cast<int>(role);
    if (i < 0 || i >= kRoleCount) return nullptr;
    if (px_delta == 0) return g_fonts[i].handle;
    const int px = std::max(8, g_fonts[i].px + static_cast<int>(std::lround(px_delta * g_font_dpi / 96.0)));
    const std::wstring face = g_fonts[i].face;
    static std::unordered_map<std::wstring, HFONT> extra;
    // Key by face + px + weight
    wchar_t key[96];
    swprintf(key, 96, L"%s|%d|%d", face.c_str(), px, g_fonts[i].weight);
    key[95] = 0;
    auto it = extra.find(key);
    if (it != extra.end()) return it->second;
    HFONT f = CreateFontW(-px, 0, 0, 0, g_fonts[i].weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH | FF_DONTCARE, face.c_str());
    if (f) {
        extra[key] = f;
        g_extra_fonts.push_back(f);
    }
    return f ? f : g_fonts[i].handle;
}

HFONT font_for_text(const std::wstring& text, FontRole role, int px_delta) {
    const int i = static_cast<int>(role);
    if (i < 0 || i >= kRoleCount) return nullptr;
    if (text.empty()) return font(role, px_delta);
    CacheKey key;
    key.text = text;
    key.role = i;
    key.delta = px_delta;
    auto it = g_font_cache.find(key);
    if (it != g_font_cache.end()) return it->second;

    HFONT chosen = font(role, px_delta);
    if (!font_has_all_glyphs(chosen, text)) {
        const int px = std::max(8, g_fonts[i].px + px_delta);
        for (const wchar_t* face : kFallbackFaces) {
            HFONT f = CreateFontW(-px, 0, 0, 0, g_fonts[i].weight, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
            if (!f) continue;
            if (font_has_all_glyphs(f, text)) {
                chosen = f;
                g_extra_fonts.push_back(f);
                break;
            }
            DeleteObject(f);
        }
    }
    if (g_font_cache.size() > 512) g_font_cache.clear();
    g_font_cache[key] = chosen;
    return chosen;
}

// ---------------------------------------------------------------------------
//  Text
// ---------------------------------------------------------------------------
bool text_is_rtl(const std::wstring& text) {
    return core::has_rtl(core::u16(reinterpret_cast<const char16_t*>(text.data()), text.size()));
}

namespace {

void setup_dc(HDC dc, HFONT f, uint32_t color) {
    SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, to_colorref(color));
}

bool script_analyse(HDC dc, const std::wstring& text, bool rtl, SCRIPT_STRING_ANALYSIS* out) {
    if (text.empty()) return false;
    SCRIPT_CONTROL sc;
    SCRIPT_STATE ss;
    std::memset(&sc, 0, sizeof(sc));
    std::memset(&ss, 0, sizeof(ss));
    ss.uBidiLevel = rtl ? 1 : 0;
    sc.uDefaultLanguage = 0x0409;  // English; affects digit shaping only
    const int glyphs = static_cast<int>(text.size() * 3 + 16);
    const DWORD flags = SSA_GLYPHS | SSA_FALLBACK | SSA_LINK;
    HRESULT hr = ScriptStringAnalyse(dc, text.c_str(), static_cast<int>(text.size()), glyphs, -1,
                                     flags, 0, &sc, &ss, nullptr, nullptr, nullptr, out);
    return SUCCEEDED(hr) && *out != nullptr;
}

int gdi_measure(HDC dc, const std::wstring& text, HFONT f) {
    HGDIOBJ old = SelectObject(dc, f);
    SIZE sz = {0, 0};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &sz);
    SelectObject(dc, old);
    return sz.cx;
}

int measure_impl(HDC dc, const std::wstring& text, HFONT f, bool rtl) {
    if (!f || text.empty()) return 0;
    HGDIOBJ old = SelectObject(dc, f);
    int result = 0;
    if (rtl) {
        // Measure with the real shaping engine so the width matches drawing.
        SCRIPT_STRING_ANALYSIS ssa = nullptr;
        if (script_analyse(dc, text, true, &ssa)) {
            const SIZE* sz = ScriptString_pSize(ssa);
            if (sz) result = sz->cx;
            ScriptStringFree(&ssa);
        }
        if (result <= 0) result = gdi_measure(dc, text, f);
    } else {
        result = gdi_measure(dc, text, f);
    }
    SelectObject(dc, old);
    return result;
}

}  // namespace

int measure_text(HDC dc, const std::wstring& text, HFONT f, bool rtl_base) {
    return measure_impl(dc, text, f, rtl_base || text_is_rtl(text));
}

void draw_text(HDC dc, const std::wstring& text, RECT rc, HFONT f, uint32_t color, TextAlign align,
               TextVAlign valign, bool rtl_base, bool ellipsis) {
    if (!f || text.empty() || rc.right <= rc.left) return;
    const bool rtl = rtl_base || text_is_rtl(text);
    setup_dc(dc, f, color);

    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    const int text_h = tm.tmHeight;
    int y = rc.top;
    if (valign == VCenter) y = rc.top + (rc.bottom - rc.top - text_h) / 2;
    else if (valign == VBottom) y = rc.bottom - text_h;

    if (!rtl) {
        UINT flags = DT_SINGLELINE | DT_NOPREFIX | (ellipsis ? DT_END_ELLIPSIS : 0);
        if (align == AlignCenter) flags |= DT_CENTER;
        else if (align == AlignRight) flags |= DT_RIGHT;
        if (valign == VCenter) flags |= DT_VCENTER;
        RECT r = rc;
        DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &r, flags);
        return;
    }

    // RTL / complex script: shape with Uniscribe.
    std::wstring shown = text;
    int width = measure_impl(dc, shown, f, true);
    const int avail = rc.right - rc.left;
    if (ellipsis && width > avail) {
        // Trim code units until it fits with an ellipsis.
        int guard = 0;
        while (shown.size() > 1 && ++guard < 400) {
            shown.pop_back();
            std::wstring candidate = shown;
            candidate.push_back(0x2026);
            width = measure_impl(dc, candidate, f, true);
            if (width <= avail) {
                shown = candidate;
                break;
            }
        }
        if (width > avail) return;
    }
    int x = rc.left;
    if (align == AlignCenter) x = rc.left + (avail - width) / 2;
    else if (align == AlignRight) x = rc.right - width;

    SCRIPT_STRING_ANALYSIS ssa = nullptr;
    if (script_analyse(dc, shown, true, &ssa)) {
        RECT clip = rc;
        ScriptStringOut(ssa, x, y, ETO_CLIPPED, &clip, 0, 0, FALSE);
        ScriptStringFree(&ssa);
    } else {
        RECT r = rc;
        DrawTextW(dc, shown.c_str(), static_cast<int>(shown.size()), &r,
                  DT_SINGLELINE | DT_NOPREFIX | (align == AlignCenter ? DT_CENTER : 0) |
                      (align == AlignRight ? DT_RIGHT : 0) | DT_RTLREADING);
    }
}

// ---------------------------------------------------------------------------
//  Word wrapped paragraphs (used by the setup wizard / hints)
// ---------------------------------------------------------------------------
void draw_text_wrapped(HDC dc, const std::wstring& text, RECT rc, HFONT f, uint32_t color,
                       int line_height_px, bool rtl_base) {
    if (!f || text.empty() || rc.right <= rc.left) return;
    const bool rtl = rtl_base || text_is_rtl(text);
    HGDIOBJ old = SelectObject(dc, f);
    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    const int lh = line_height_px > 0 ? line_height_px : (static_cast<int>(tm.tmHeight) + std::max(2, static_cast<int>(tm.tmHeight) / 3));

    std::vector<std::wstring> lines;
    std::wstring current;
    size_t i = 0;
    auto flush_line = [&]() {
        lines.push_back(current);
        current.clear();
    };
    // Split into words but keep explicit newlines.
    while (i <= text.size()) {
        if (i == text.size()) {
            if (!current.empty()) flush_line();
            break;
        }
        const wchar_t c = text[i];
        if (c == L'\n') {
            flush_line();
            ++i;
            continue;
        }
        if (c == L' ' || c == L'\t' || c == L'\r') {
            ++i;
            continue;
        }
        // Read one word.
        size_t j = i;
        while (j < text.size() && text[j] != L' ' && text[j] != L'\t' && text[j] != L'\n' &&
               text[j] != L'\r')
            ++j;
        std::wstring word = text.substr(i, j - i);
        i = j;
        const std::wstring candidate = current.empty() ? word : current + L" " + word;
        if (measure_impl(dc, candidate, f, rtl) <= (rc.right - rc.left)) {
            current = candidate;
        } else {
            if (!current.empty()) flush_line();
            current = word;
        }
    }

    int y = rc.top;
    for (const std::wstring& line : lines) {
        if (line.empty()) {
            y += lh / 2;
            continue;
        }
        if (y + lh > rc.bottom) break;
        RECT line_rc = {rc.left, y, rc.right, y + lh};
        draw_text(dc, line, line_rc, f, color, rtl ? AlignRight : AlignLeft, VTop, rtl, false);
        y += lh;
    }
}

// ---------------------------------------------------------------------------
//  Icons
// ---------------------------------------------------------------------------
void draw_icon(Surface& s, Icon icon, float x, float y, float size, uint32_t color) {
    const float t = std::max(1.2f, size * 0.085f);  // stroke thickness
    const float cx = x + size * 0.5f, cy = y + size * 0.5f;
    switch (icon) {
        case Icon::None:
            break;
        case Icon::Keyboard: {
            const float w = size * 0.92f, h = size * 0.62f;
            const float left = x + (size - w) * 0.5f, top = y + (size - h) * 0.5f;
            s.stroke_round(static_cast<int>(left), static_cast<int>(top), static_cast<int>(w),
                           static_cast<int>(h), size * 0.14f, t, color);
            // keys
            const float k = size * 0.075f;
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 4; ++col) {
                    s.fill_round(static_cast<int>(left + w * 0.10f + col * w * 0.20f),
                                 static_cast<int>(top + h * 0.22f + row * h * 0.24f),
                                 static_cast<int>(k), static_cast<int>(k), 1.0f, color);
                }
            }
            s.fill_round(static_cast<int>(left + w * 0.10f), static_cast<int>(top + h * 0.70f),
                         static_cast<int>(w * 0.50f), static_cast<int>(k * 0.9f), 1.0f, color);
            break;
        }
        case Icon::Clipboard: {
            const float w = size * 0.66f, h = size * 0.86f;
            const float left = cx - w * 0.5f, top = y + (size - h) * 0.5f + size * 0.04f;
            s.stroke_round(static_cast<int>(left), static_cast<int>(top), static_cast<int>(w),
                           static_cast<int>(h), size * 0.16f, t, color);
            s.fill_round(static_cast<int>(cx - w * 0.22f), static_cast<int>(top - size * 0.05f),
                         static_cast<int>(w * 0.44f), static_cast<int>(size * 0.16f),
                         size * 0.06f, color);
            for (int i = 0; i < 2; ++i) {
                s.fill_round(static_cast<int>(left + w * 0.20f),
                             static_cast<int>(top + h * 0.38f + i * h * 0.20f),
                             static_cast<int>(w * 0.60f), static_cast<int>(t * 0.85f), 1.0f,
                             color);
            }
            break;
        }
        case Icon::Close: {
            const float d = size * 0.28f;
            s.line(cx - d, cy - d, cx + d, cy + d, t, color);
            s.line(cx + d, cy - d, cx - d, cy + d, t, color);
            break;
        }
        case Icon::Minimize:
            s.line(cx - size * 0.28f, cy, cx + size * 0.28f, cy, t, color);
            break;
        case Icon::Pin: {
            const float r = size * 0.30f;
            s.ring(cx, cy - size * 0.06f, r, t, color);
            s.line(cx - size * 0.18f, cy - size * 0.06f, cx + size * 0.18f, cy - size * 0.06f, t,
                   color);
            s.line(cx, cy + r * 0.2f, cx, cy + size * 0.34f, t, color);
            break;
        }
        case Icon::Search: {
            const float r = size * 0.26f;
            s.ring(cx - size * 0.06f, cy - size * 0.06f, r, t, color);
            s.line(cx + r * 0.65f, cy + r * 0.65f, cx + size * 0.32f, cy + size * 0.32f, t * 1.15f,
                   color);
            break;
        }
        case Icon::Copy: {
            const float w = size * 0.52f, h = size * 0.60f;
            s.stroke_round(static_cast<int>(cx - w * 0.62f), static_cast<int>(cy - h * 0.62f),
                           static_cast<int>(w), static_cast<int>(h), size * 0.10f, t, color);
            s.stroke_round(static_cast<int>(cx - w * 0.05f), static_cast<int>(cy - h * 0.10f),
                           static_cast<int>(w), static_cast<int>(h), size * 0.10f, t, color);
            break;
        }
        case Icon::Trash: {
            const float w = size * 0.50f;
            s.line(cx - w * 0.62f, cy - size * 0.26f, cx + w * 0.62f, cy - size * 0.26f, t, color);
            s.line(cx - w * 0.20f, cy - size * 0.36f, cx + w * 0.20f, cy - size * 0.36f, t, color);
            const float bin[] = {cx - w * 0.44f, cy - size * 0.20f, cx - w * 0.32f,
                                 cy + size * 0.30f, cx + w * 0.32f, cy + size * 0.30f,
                                 cx + w * 0.44f, cy - size * 0.20f};
            s.polyline(bin, 4, t, color);
            break;
        }
        case Icon::Pause: {
            s.fill_round(static_cast<int>(cx - size * 0.22f), static_cast<int>(cy - size * 0.24f),
                         static_cast<int>(size * 0.14f), static_cast<int>(size * 0.48f),
                         size * 0.04f, color);
            s.fill_round(static_cast<int>(cx + size * 0.08f), static_cast<int>(cy - size * 0.24f),
                         static_cast<int>(size * 0.14f), static_cast<int>(size * 0.48f),
                         size * 0.04f, color);
            break;
        }
        case Icon::Play:
            s.triangle(cx - size * 0.18f, cy - size * 0.26f, cx - size * 0.18f, cy + size * 0.26f,
                       cx + size * 0.26f, cy, color);
            break;
        case Icon::Power: {
            const float r = size * 0.28f;
            s.ring(cx, cy + size * 0.05f, r, t, color);
            s.line(cx, cy - size * 0.34f, cx, cy - size * 0.02f, t * 1.1f, color);
            break;
        }
        case Icon::Gear: {
            const float r = size * 0.26f;
            s.ring(cx, cy, r, t, color);
            for (int i = 0; i < 6; ++i) {
                const float a = static_cast<float>(i) * 3.14159265f / 3.0f;
                s.line(cx + std::cos(a) * r, cy + std::sin(a) * r, cx + std::cos(a) * (r + size * 0.14f),
                       cy + std::sin(a) * (r + size * 0.14f), t, color);
            }
            break;
        }
        case Icon::Check: {
            const float check[] = {cx - size * 0.26f, cy + size * 0.02f, cx - size * 0.06f,
                                   cy + size * 0.22f, cx + size * 0.28f, cy - size * 0.20f};
            s.polyline(check, 3, t * 1.2f, color);
            break;
        }
        case Icon::Warning: {
            const float tri[] = {cx, cy - size * 0.30f, cx + size * 0.32f, cy + size * 0.26f,
                                 cx - size * 0.32f, cy + size * 0.26f, cx, cy - size * 0.30f};
            s.polyline(tri, 4, t, color);
            s.line(cx, cy - size * 0.10f, cx, cy + size * 0.06f, t, color);
            s.circle(cx, cy + size * 0.16f, t * 0.55f, color);
            break;
        }
        case Icon::ChevronLeft: {
            const float chev_l[] = {cx + size * 0.12f, cy - size * 0.22f, cx - size * 0.12f, cy,
                                    cx + size * 0.12f, cy + size * 0.22f};
            s.polyline(chev_l, 3, t, color);
            break;
        }
        case Icon::ChevronRight: {
            const float chev_r[] = {cx - size * 0.12f, cy - size * 0.22f, cx + size * 0.12f, cy,
                                    cx - size * 0.12f, cy + size * 0.22f};
            s.polyline(chev_r, 3, t, color);
            break;
        }
        case Icon::Spark: {
            for (int i = 0; i < 4; ++i) {
                const float a = static_cast<float>(i) * 1.5707963f;
                s.line(cx + std::cos(a) * size * 0.30f, cy + std::sin(a) * size * 0.30f,
                       cx, cy, t, color);
            }
            s.circle(cx, cy, size * 0.07f, color);
            break;
        }
        case Icon::Mouse: {
            s.stroke_round(static_cast<int>(cx - size * 0.20f), static_cast<int>(cy - size * 0.30f),
                           static_cast<int>(size * 0.40f), static_cast<int>(size * 0.60f),
                           size * 0.18f, t, color);
            s.line(cx, cy - size * 0.30f, cx, cy - size * 0.10f, t, color);
            break;
        }
        case Icon::Shift: {
            const float w = size * 0.30f;
            s.line(cx - w, cy + size * 0.02f, cx, cy - size * 0.26f, t, color);
            s.line(cx, cy - size * 0.26f, cx + w, cy + size * 0.02f, t, color);
            s.line(cx, cy - size * 0.12f, cx, cy + size * 0.28f, t, color);
            break;
        }
        case Icon::Backspace: {
            s.line(cx - size * 0.30f, cy - size * 0.22f, cx + size * 0.28f, cy - size * 0.22f, t,
                   color);
            s.line(cx + size * 0.28f, cy - size * 0.22f, cx + size * 0.28f, cy + size * 0.22f, t,
                   color);
            s.line(cx - size * 0.30f, cy - size * 0.06f, cx - size * 0.10f, cy + size * 0.22f, t,
                   color);
            s.line(cx - size * 0.30f, cy - size * 0.06f, cx + size * 0.28f, cy + size * 0.22f, t,
                   color);
            break;
        }
        case Icon::Enter: {
            const float hook[] = {cx + size * 0.26f, cy - size * 0.24f, cx + size * 0.26f,
                                  cy + size * 0.10f, cx - size * 0.26f, cy + size * 0.10f};
            s.polyline(hook, 3, t, color);
            const float arrow[] = {cx - size * 0.10f, cy - size * 0.04f, cx - size * 0.28f,
                                   cy + size * 0.12f, cx - size * 0.10f, cy + size * 0.26f};
            s.polyline(arrow, 3, t, color);
            break;
        }
        case Icon::Tab: {
            s.line(cx - size * 0.30f, cy + size * 0.20f, cx + size * 0.30f, cy + size * 0.20f, t,
                   color);
            s.line(cx + size * 0.10f, cy - size * 0.22f, cx + size * 0.34f, cy, t, color);
            s.line(cx + size * 0.10f, cy + size * 0.22f, cx + size * 0.34f, cy, t, color);
            s.line(cx - size * 0.30f, cy + size * 0.02f, cx - size * 0.30f, cy, t, color);
            break;
        }
        case Icon::Globe: {
            s.ring(cx, cy, size * 0.30f, t, color);
            s.line(cx - size * 0.30f, cy, cx + size * 0.30f, cy, t, color);
            s.line(cx, cy - size * 0.30f, cx, cy + size * 0.30f, t, color);
            break;
        }
        case Icon::Grid: {
            for (int r = 0; r < 2; ++r) {
                for (int c = 0; c < 2; ++c) {
                    s.stroke_round(static_cast<int>(cx - size * 0.30f + c * size * 0.33f),
                                   static_cast<int>(cy - size * 0.30f + r * size * 0.33f),
                                   static_cast<int>(size * 0.24f), static_cast<int>(size * 0.24f),
                                   size * 0.06f, t, color);
                }
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
//  Window helpers
// ---------------------------------------------------------------------------
void set_window_rounded_region(HWND hwnd, int radius) {
    RECT rc;
    GetWindowRect(hwnd, &rc);
    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, radius * 2, radius * 2);
    SetWindowRgn(hwnd, rgn, TRUE);  // window owns the region now
}

}  // namespace amin
