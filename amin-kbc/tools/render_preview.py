#!/usr/bin/env python3
"""
QA tool: renders the keyboard and the dashboard from the REAL data in the source
tree so the layout can be inspected without a Windows machine.

  * key tables and row definitions are parsed out of src/core/layout.cpp
  * compute_row() below is a 1:1 port of core::compute_row()
  * colours are parsed out of src/win/amin.h
  * the dashboard geometry is a port of compute_layout() in src/win/ui_dashboard.cpp

The images are an approximation: the application itself draws with GDI +
Uniscribe (and Tahoma for Persian), while this script uses Pillow + Vazirmatn.
It is a development aid, NOT a screenshot of the running program.

Usage:
    python3 tools/render_preview.py [--out docs/preview]
Requires: pillow, arabic-reshaper, python-bidi
          a Persian TTF (Vazirmatn works; set AMIN_PERSIAN_FONT)
"""
import argparse
import math
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

try:
    import arabic_reshaper
    from bidi.algorithm import get_display
except ImportError:  # pragma: no cover
    arabic_reshaper = None

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------
#  Palette (parsed from src/win/amin.h)
# ---------------------------------------------------------------------------
def load_palette():
    text = open(os.path.join(ROOT, "src/win/amin.h"), encoding="utf-8").read()
    palette = {}
    for name, value in re.findall(r"constexpr uint32_t (k\w+)\s*=\s*0x(FF[0-9A-Fa-f]{6});", text):
        palette[name] = "#" + value[2:]
    return palette


C = {}


def col(name, alpha=None):
    hexv = C[name]
    if alpha is None:
        return hexv
    r, g, b = (int(hexv[i:i + 2], 16) for i in (1, 3, 5))
    return (r, g, b, alpha)


def mix(a, b, t):
    """Blend two '#rrggbb' colours, t=0 -> a, t=1 -> b."""
    ra, ga, ba = (int(a[i:i + 2], 16) for i in (1, 3, 5))
    rb, gb, bb = (int(b[i:i + 2], 16) for i in (1, 3, 5))
    return "#%02x%02x%02x" % (
        int(ra + (rb - ra) * t), int(ga + (gb - ga) * t), int(ba + (bb - ba) * t))


# ---------------------------------------------------------------------------
#  Fonts
# ---------------------------------------------------------------------------
def find_font(env, candidates, fallback):
    path = os.environ.get(env)
    if path and os.path.exists(path):
        return path
    for c in candidates:
        if os.path.exists(c):
            return c
    return fallback


LATIN = find_font("AMIN_LATIN_FONT", [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
], "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")
LATIN_BOLD = find_font("AMIN_LATIN_FONT_BOLD", [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
], LATIN)
PERSIAN = find_font("AMIN_PERSIAN_FONT", [
    "/tmp/vazirmatn-master/fonts/ttf/Vazirmatn-Regular.ttf",
    os.path.join(ROOT, "tools/fonts/Vazirmatn-Regular.ttf"),
], LATIN)
PERSIAN_BOLD = find_font("AMIN_PERSIAN_FONT_BOLD", [
    "/tmp/vazirmatn-master/fonts/ttf/Vazirmatn-Bold.ttf",
    os.path.join(ROOT, "tools/fonts/Vazirmatn-Bold.ttf"),
], PERSIAN)

_font_cache = {}


def F(path, size):
    key = (path, size)
    if key not in _font_cache:
        _font_cache[key] = ImageFont.truetype(path, size)
    return _font_cache[key]


def shape(text, rtl):
    """Shapes Persian text the way Uniscribe does (presentation forms + bidi)."""
    if not rtl or arabic_reshaper is None:
        return text
    return get_display(arabic_reshaper.reshape(text))


def text_size(draw, text, font):
    box = draw.textbbox((0, 0), text, font=font)
    return box[2] - box[0], box[3] - box[1]


def draw_text(draw, box, text, font, fill, rtl=False, align="left", valign="center"):
    if not text:
        return
    s = shape(text, rtl)
    w, h = text_size(draw, s, font)
    x0, y0, x1, y1 = box
    if align == "center":
        x = x0 + (x1 - x0 - w) // 2
    elif align == "right":
        x = x1 - w
    else:
        x = x0
    if valign == "center":
        y = y0 + (y1 - y0 - h) // 2 - draw.textbbox((0, 0), s, font=font)[1]
    elif valign == "bottom":
        y = y1 - h
    else:
        y = y0
    draw.text((x, y), s, font=font, fill=fill)


# ---------------------------------------------------------------------------
#  Parse src/core/layout.cpp
# ---------------------------------------------------------------------------
def cxx_string(token):
    """Decodes a C++ u"..." literal (with \\uXXXX escapes)."""
    token = token.strip()
    m = re.match(r'u"((?:[^"\\]|\\.)*)"', token)
    if not m:
        return None
    body = m.group(1)
    out = []
    i = 0
    while i < len(body):
        ch = body[i]
        if ch == "\\" and i + 1 < len(body):
            nxt = body[i + 1]
            if nxt == "u":
                out.append(chr(int(body[i + 2:i + 6], 16)))
                i += 6
                continue
            if nxt == '"':
                out.append('"')
                i += 2
                continue
            if nxt == "\\":
                out.append("\\")
                i += 2
                continue
            out.append(nxt)
            i += 2
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def split_entries(block):
    """Splits an initialiser body into top-level entries.

    Tracks brace and parenthesis depth and - crucially - ignores anything inside
    a C++ string literal, so keys such as u"(" or u"," cannot confuse the split.

    The entry count is also checked against the RowDef table below, which is what
    the C++ build does natively; a mismatch here would mean the real tables are
    inconsistent.
    """
    entries, brace, paren, current = [], 0, 0, ""
    in_string = False
    escape = False
    for ch in block:
        if in_string:
            current += ch
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            continue
        if ch == '"':
            in_string = True
            current += ch
        elif ch == "{":
            brace += 1
            current += ch
        elif ch == "}":
            brace -= 1
            current += ch
            if brace == 0 and paren == 0:
                entries.append(current.strip())
                current = ""
        elif ch == "(":
            paren += 1
            current += ch
        elif ch == ")":
            paren -= 1
            current += ch
        elif ch == "," and brace == 0 and paren == 0:
            if current.strip():
                entries.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        entries.append(current.strip())
    return [e for e in entries if e]


def parse_layouts():
    src = open(os.path.join(ROOT, "src/core/layout.cpp"), encoding="utf-8").read()
    src_no_comments = re.sub(r"//[^\n]*", "", src)

    key_tables = {}
    for m in re.finditer(r"static const KeyDef (k\w+)\[\] = \{(.*?)\n\};", src_no_comments, re.S):
        name, body = m.group(1), m.group(2)
        keys = []
        for entry in split_entries(body):
            entry = entry.strip()
            if entry.startswith("{"):                      # raw ZWNJ entry
                parts = [p.strip() for p in split_entries(entry[1:-1])]
                keys.append({
                    "label": cxx_string(parts[0]), "label_shift": cxx_string(parts[1]),
                    "text": cxx_string(parts[2]), "text_shift": cxx_string(parts[3]),
                    "action": parts[5].split("::")[-1], "units": float(parts[6].rstrip("f")),
                })
                continue
            macro = re.match(r"(K|KU|KA|KVK)\((.*)\)$", entry, re.S)
            if not macro:
                continue
            kind, args = macro.group(1), [a.strip() for a in macro.group(2).split(",")]
            if kind == "K":
                keys.append({"label": cxx_string(args[0]), "label_shift": cxx_string(args[1]),
                             "text": cxx_string(args[0]), "text_shift": cxx_string(args[1]),
                             "action": "Character", "units": 1.0})
            elif kind == "KU":
                keys.append({"label": cxx_string(args[1]), "label_shift": cxx_string(args[2]),
                             "text": cxx_string(args[1]), "text_shift": cxx_string(args[2]),
                             "action": "Character", "units": float(args[0].rstrip("f"))})
            elif kind == "KA":
                keys.append({"label": cxx_string(args[1]), "label_shift": cxx_string(args[2]),
                             "text": None, "text_shift": None,
                             "action": args[0], "units": float(args[3].rstrip("f"))})
            elif kind == "KVK":
                keys.append({"label": cxx_string(args[2]), "label_shift": cxx_string(args[3]) if args[3] != "nullptr" else None,
                             "text": None, "text_shift": None,
                             "action": args[0], "units": float(args[4].rstrip("f"))})
        key_tables[name] = keys

    row_tables = {}
    for m in re.finditer(r"static const RowDef (k\w+)\[\] = \{(.*?)\};", src_no_comments, re.S):
        row_tables[m.group(1)] = [(n, int(c)) for n, c in
                                 re.findall(r"\{\s*(k\w+)\s*,\s*(\d+)\s*\}", m.group(2))]

    page_tables = {}
    for m in re.finditer(r"static const PageDef (k\w+)\[\] = \{(.*?)\n\};", src_no_comments, re.S):
        pages = []
        for pm in re.finditer(r"\{\s*u\"[^\"]*\"\s*,\s*(k\w+)\s*,\s*(\d+)\s*\}", m.group(2)):
            pages.append((pm.group(1), int(pm.group(2))))
        page_tables[m.group(1)] = pages

    layouts = {}
    for code, ptable, rtl in (("fa", "kFaPages", True), ("en", "kEnPages", False)):
        pages = []
        for rows_name, count in page_tables[ptable]:
            rows = []
            for rk, declared in row_tables[rows_name]:
                actual = len(key_tables[rk])
                if actual != declared:
                    print(f"  WARNING: {rk} has {actual} keys but RowDef says {declared}")
                rows.append(key_tables[rk])
            pages.append(rows)
        layouts[code] = {"pages": pages, "rtl": rtl}
    return layouts


ROW_UNITS = 15.0


def compute_row(row, inner_width, gap):
    """1:1 port of core::compute_row()."""
    out = []
    if not row:
        return out
    total = sum(k["units"] if k["units"] > 0 else 1.0 for k in row)
    usable = inner_width - gap * (len(row) - 1)
    if usable <= 0:
        return out
    unit = usable / total
    cursor = 0.0
    for i, key in enumerate(row):
        u = key["units"] if key["units"] > 0 else 1.0
        w = u * unit
        x = int(round(cursor))
        x2 = int(round(cursor + w))
        pw = x2 - x
        if i == len(row) - 1:
            pw = inner_width - x
        out.append({"x": x, "w": max(1, pw)})
        cursor += w + gap
    return out


# ---------------------------------------------------------------------------
#  Keyboard window
# ---------------------------------------------------------------------------
def icon(draw, name, cx, cy, size, color, width=2):
    h = size * 0.5
    if name == "Backspace":
        draw.line([cx - h * 0.75, cy - h * 0.45, cx + h * 0.6, cy - h * 0.45], fill=color, width=width)
        draw.line([cx + h * 0.6, cy - h * 0.45, cx + h * 0.6, cy + h * 0.45], fill=color, width=width)
        draw.line([cx - h * 0.75, cy - h * 0.2, cx + h * 0.6, cy + h * 0.45], fill=color, width=width)
        draw.line([cx - h * 0.75, cy - h * 0.2, cx - h * 0.2, cy + h * 0.45], fill=color, width=width)
    elif name == "Enter":
        draw.line([cx + h * 0.55, cy - h * 0.5, cx + h * 0.55, cy + h * 0.2,
                   cx - h * 0.5, cy + h * 0.2], fill=color, width=width, joint="curve")
        draw.line([cx - h * 0.2, cy - h * 0.1, cx - h * 0.6, cy + h * 0.25,
                   cx - h * 0.2, cy + h * 0.55], fill=color, width=width, joint="curve")
    elif name == "Tab":
        draw.line([cx - h * 0.7, cy + h * 0.5, cx + h * 0.7, cy + h * 0.5], fill=color, width=width)
        draw.line([cx + h * 0.2, cy - h * 0.5, cx + h * 0.75, cy], fill=color, width=width)
        draw.line([cx + h * 0.2, cy + h * 0.5, cx + h * 0.75, cy], fill=color, width=width)
    elif name == "Shift":
        draw.line([cx - h * 0.65, cy + h * 0.05, cx, cy - h * 0.55, cx + h * 0.65, cy + h * 0.05],
                  fill=color, width=width, joint="curve")
        draw.line([cx, cy - h * 0.25, cx, cy + h * 0.6], fill=color, width=width)
    elif name == "HideKeyboard":
        draw.line([cx - h * 0.45, cy - h * 0.45, cx + h * 0.45, cy + h * 0.45], fill=color, width=width)
        draw.line([cx + h * 0.45, cy - h * 0.45, cx - h * 0.45, cy + h * 0.45], fill=color, width=width)
    elif name == "Keyboard":
        w, hh = size * 0.92, size * 0.62
        draw.rounded_rectangle([cx - w / 2, cy - hh / 2, cx + w / 2, cy + hh / 2],
                               radius=size * 0.14, outline=color, width=width)
        k = size * 0.075
        for r in range(2):
            for c in range(4):
                x = cx - w / 2 + w * 0.10 + c * w * 0.20
                y = cy - hh / 2 + hh * 0.22 + r * hh * 0.24
                draw.rectangle([x, y, x + k, y + k], fill=color)
        draw.rectangle([cx - w / 2 + w * 0.10, cy - hh / 2 + hh * 0.70,
                        cx - w / 2 + w * 0.60, cy - hh / 2 + hh * 0.70 + k * 0.9], fill=color)
    elif name == "Pin":
        draw.ellipse([cx - size * 0.14, cy - size * 0.2, cx + size * 0.14, cy + size * 0.08],
                     outline=color, width=width)
        draw.line([cx, cy + size * 0.08, cx, cy + size * 0.3], fill=color, width=width)


def render_keyboard(layouts, lang="fa", page=0, out="keyboard.png", target_hint="Notepad"):
    pad, gap, title_h = 12, 6, 42
    screen_w = 1366
    width = min(1060, int(screen_w * 0.72))
    width = max(width, 620)
    unit = (width - pad * 2 - gap * 14) // 15
    key_h = max(30, int(unit * 0.95))
    height = title_h + pad * 2 + key_h * 5 + gap * 4

    img = Image.new("RGB", (width, height), C["kBg"])
    d = ImageDraw.Draw(img)
    # title bar gradient
    for y in range(title_h):
        d.line([(0, y), (width, y)], fill=mix("#1a2032", "#121724", y / max(1, title_h - 1)))

    icon(d, "Keyboard", pad + 9, title_h / 2, 18, C["kAccent"], 2)
    persian = (lang == "fa")
    title = "صفحه کلید" if persian else "Keyboard"
    draw_text(d, (pad + 26, 0, pad + 240, title_h),
              title, F(PERSIAN_BOLD if persian else LATIN_BOLD, 14), C["kText"], persian)
    tw, _ = text_size(d, shape(title, persian), F(PERSIAN_BOLD if persian else LATIN_BOLD, 14))
    bx = pad + 26 + tw + 10
    d.rounded_rectangle([bx, (title_h - 22) / 2, bx + 64, (title_h + 22) / 2], radius=11,
                        fill=C["kAccentSoft"])
    draw_text(d, (bx, 0, bx + 64, title_h), "فارسی" if persian else "English",
              F(PERSIAN if persian else LATIN, 11), C["kAccent"], persian, "center")

    # target hint + window buttons
    btn = 30
    by = (title_h - btn) // 2
    close = (width - 10 - btn, by, btn, btn)
    pin = (width - 10 - btn - 6 - btn, by, btn, btn)
    d.rounded_rectangle([pin[0] + 6, pin[1], pin[0] + btn - 6, pin[1] + btn], radius=9,
                        outline=col("kAccent", 160), width=1)
    icon(d, "Pin", pin[0] + btn / 2, pin[1] + btn / 2, 16, C["kTextDim"], 2)
    icon(d, "HideKeyboard", close[0] + btn / 2, close[1] + btn / 2, 16, C["kTextDim"], 2)
    if target_hint:
        draw_text(d, (pin[0] - 320, 0, pin[0] - 14, title_h), "← " + target_hint,
                  F(LATIN, 11), C["kTextFaint"], False, "right")

    rows = layouts[lang]["pages"][page]
    y = title_h + pad
    for row in rows:
        places = compute_row(row, width - pad * 2, gap)
        for key, pl in zip(row, places):
            x = pad + pl["x"]
            w = pl["w"]
            action = key["action"]
            radius = min(9, min(w, key_h) * 0.28)
            fill = C["kKey"] if action == "Character" else C["kKeySpecial"]
            fg = C["kKeyText"]
            if action == "Enter":
                fill = C["kAccent2"]
            elif action == "Language":
                fill = C["kKeySpecial"]
                fg = C["kTextDim"]
            elif action in ("Symbols", "Escape"):
                fg = C["kTextDim"]
            d.rounded_rectangle([x, y, x + w - 1, y + key_h - 1], radius=radius, fill=fill,
                                outline=C["kBorderSoft"], width=1)
            label = key["label"] or ""
            if action in ("Backspace", "Enter", "Tab", "Shift", "HideKeyboard"):
                icon(d, action, x + w / 2, y + key_h / 2, 20, fg, 2)
            elif action == "Space":
                for k in (-1, 0, 1):
                    d.line([x + w / 2 + k * 9 - 3, y + key_h / 2, x + w / 2 + k * 9 + 3,
                            y + key_h / 2], fill=col("kTextFaint", 150), width=2)
            elif label:
                rtl = layouts[lang]["rtl"]
                size = 19
                font = F(PERSIAN if rtl else LATIN, size)
                while text_size(d, shape(label, rtl), font)[0] > w - 6 and size > 9:
                    size -= 1
                    font = F(PERSIAN if rtl else LATIN, size)
                if action == "Language":
                    d.ellipse([x + 8, y + key_h / 2 - 7, x + 22, y + key_h / 2 + 7],
                              outline=fg, width=2)
                    draw_text(d, (x + 26, y, x + w - 4, y + key_h), label, font, fg, rtl)
                else:
                    draw_text(d, (x + 3, y, x + w - 3, y + key_h), label, font, fg, rtl, "center")
        y += key_h + gap

    draw_text(d, (10, height - 18, width - 10, height - 4),
              "PREVIEW rendered from the real key tables - not a screenshot",
              F(LATIN, 9), "#4a5266")
    img.save(out)
    return out


# ---------------------------------------------------------------------------
#  Dashboard
# ---------------------------------------------------------------------------
def _right_column(d, right_x, right_w, content_y, content_h, hotkey_text="Ctrl+Alt+V"):
    """Status + settings cards (identical for both tabs)."""
    sc = (right_x, content_y, right_x + right_w, content_y + 184)
    d.rounded_rectangle(sc, radius=14, fill=C["kCard"], outline=C["kBorderSoft"])
    draw_text(d, (sc[0] + 16, sc[1] + 12, sc[2] - 16, sc[1] + 34), "وضعیت", F(PERSIAN_BOLD, 11),
              C["kTextDim"], True)
    rows_status = [("ثبت کلیپ‌بورد", "فعال", C["kSuccess"]),
                   ("میان‌بر باز کردن کلیپ‌بورد", hotkey_text, C["kSuccess"]),
                   ("اجرا با ویندوز", "خاموش", C["kTextFaint"])]
    ry = sc[1] + 44
    for label, value, dot in rows_status:
        d.ellipse([sc[0] + 18, ry + 14, sc[0] + 24, ry + 20], fill=dot)
        draw_text(d, (sc[0] + 32, ry, sc[2] - 130, ry + 32), label, F(PERSIAN, 10), C["kText"], True)
        draw_text(d, (sc[2] - 130, ry, sc[2] - 16, ry + 32), value, F(LATIN, 10), C["kTextDim"],
                  False, "right")
        ry += 32
    draw_text(d, (sc[0] + 16, ry + 2, sc[2] - 16, ry + 18), "ارسال به", F(PERSIAN, 9),
              C["kTextFaint"], True)
    draw_text(d, (sc[0] + 16, ry + 17, sc[2] - 16, ry + 37), "Notepad", F(PERSIAN, 9),
              C["kTextDim"], True)

    gc = (right_x, sc[3] + 14, right_x + right_w, content_y + content_h)
    d.rounded_rectangle(gc, radius=14, fill=C["kCard"], outline=C["kBorderSoft"])
    draw_text(d, (gc[0] + 16, gc[1] + 12, gc[2] - 16, gc[1] + 34), "تنظیمات", F(PERSIAN_BOLD, 11),
              C["kTextDim"], True)
    gy = gc[1] + 40
    d.rounded_rectangle([gc[0] + 14, gy, gc[0] + 58, gy + 24], radius=12, fill=C["kBorder"])
    d.ellipse([gc[0] + 17, gy + 3, gc[0] + 35, gy + 21], fill=C["kTextDim"])
    draw_text(d, (gc[0] + 70, gy - 8, gc[2] - 14, gy + 16), "اجرا با ویندوز", F(PERSIAN_BOLD, 10),
              C["kText"], True)
    draw_text(d, (gc[0] + 70, gy + 12, gc[2] - 14, gy + 30), "بدون نیاز به دسترسی مدیر",
              F(PERSIAN, 8), C["kTextFaint"], True)
    gy += 42
    two = (right_w - 28 - 8) // 2
    for i, label in enumerate(["ساخت میانبر", "پوشه داده‌ها", "راه‌اندازی اولیه", "خودآزمایی"]):
        bx = gc[0] + 14 + (i % 2) * (two + 8)
        byy = gy + (i // 2) * 42
        d.rounded_rectangle([bx, byy, bx + two, byy + 34], radius=10, fill=C["kCard"],
                            outline=C["kBorder"])
        draw_text(d, (bx + 10, byy, bx + two - 6, byy + 34), label, F(PERSIAN_BOLD, 9),
                  C["kText"], True)
    gy += 2 * 42
    d.rounded_rectangle([gc[0] + 14, gy, gc[2] - 14, gy + 34], radius=10,
                        fill=mix(C["kCard"], C["kDanger"], 0.16), outline=C["kBorder"])
    draw_text(d, (gc[0] + 14, gy, gc[2] - 14, gy + 34), "خروج از برنامه", F(PERSIAN_BOLD, 10),
              C["kDanger"], True, "center")


CLIP_ENTRIES = [
    ("سلام دنیا، این یک متن نمونه فارسی است.", "۲ دقیقه پیش"),
    ("https://github.com/amingangmanatgh2-hash/Gaming-hub-", "۸ دقیقه پیش"),
    ("AMIN Keyboard & Clipboard 1.0.0 build notes", "۲۵ دقیقه پیش"),
    ("نیم‌فاصله در «می‌شود» و «کتاب‌ها» درست تایپ می‌شود.", "۱ ساعت پیش"),
    ("Ctrl+Alt+V", "۳ ساعت پیش"),
    ("صورت‌حساب ماهانه: ۱۲۵٬۰۰۰ تومان", "دیروز"),
]


def _clipboard_tab(d, left_x, content_y, left_w, content_h):
    search = (left_x, content_y, left_x + left_w, content_y + 38)
    d.rounded_rectangle(search, radius=10, fill=C["kPanel"], outline=C["kBorder"])
    cx, cy = search[0] + 20, (search[1] + search[3]) / 2
    d.ellipse([cx - 6, cy - 6, cx + 6, cy + 6], outline=C["kTextFaint"], width=2)
    d.line([cx + 4, cy + 4, cx + 9, cy + 9], fill=C["kTextFaint"], width=2)
    draw_text(d, (search[0] + 36, search[1] + 8, search[2] - 12, search[3] - 8),
              "جستجو در تاریخچه…", F(PERSIAN, 10), C["kTextFaint"], True)

    list_top = search[3] + 12
    buttons_y = content_y + content_h - 46
    lst = (left_x, list_top, left_x + left_w, buttons_y - 32)
    d.rounded_rectangle(lst, radius=12, fill=C["kPanel"], outline=C["kBorderSoft"])
    row_h = 56
    for i, (text, when) in enumerate(CLIP_ENTRIES):
        ry = lst[1] + 6 + i * row_h
        if ry + row_h - 4 > lst[3]:
            break
        row = (lst[0] + 6, ry, lst[2] - 18, ry + row_h - 4)
        if i == 0:
            d.rounded_rectangle(row, radius=9, fill=C["kAccentSoft"], outline=col("kAccent", 150))
        elif i == 2:
            d.rounded_rectangle(row, radius=9, fill=C["kCardHover"])
        rtl = any("\u0600" <= ch <= "\u06ff" for ch in text)
        draw_text(d, (row[0] + 12, row[1] + 6, row[2] - 10, row[1] + 30), text,
                  F(PERSIAN if rtl else LATIN, 10), C["kText"], rtl, "right" if rtl else "left")
        draw_text(d, (row[0] + 12, row[1] + 30, row[2] - 10, row[3] - 4),
                  when + "   ·   " + str(len(text)) + " کاراکتر",
                  F(PERSIAN, 8), C["kTextFaint"], True, "right" if rtl else "left")

    bw = 96
    copy = (left_x, buttons_y, left_x + bw, buttons_y + 46)
    for i in range(copy[2] - copy[0]):
        d.line([(copy[0] + i, copy[1]), (copy[0] + i, copy[3])],
               fill=mix("#7c5cff", "#4d8dff", i / bw))
    d.rounded_rectangle([copy[0] + 11, copy[1] + 15, copy[0] + 27, copy[1] + 31], outline="#ffffff",
                        width=2)
    draw_text(d, (copy[0] + 33, copy[1], copy[2], copy[3]), "کپی", F(PERSIAN_BOLD, 10),
              "#ffffff", True)
    clear = (copy[2] + 8, buttons_y, copy[2] + 8 + 112, buttons_y + 46)
    d.rounded_rectangle(clear, radius=10, fill=C["kCard"], outline=C["kBorder"])
    draw_text(d, (clear[0] + 10, clear[1], clear[2] - 6, clear[3]), "پاک کردن همه",
              F(PERSIAN_BOLD, 9), C["kTextDim"], True)
    pause = (clear[2] + 8, buttons_y, clear[2] + 8 + 96, buttons_y + 46)
    d.rounded_rectangle(pause, radius=10, fill=C["kCard"], outline=C["kBorder"])
    draw_text(d, (pause[0] + 10, pause[1], pause[2] - 6, pause[3]), "توقف ثبت",
              F(PERSIAN_BOLD, 9), C["kWarn"], True)
    note = (lst[0], lst[3] + 4, lst[2], lst[3] + 22)
    draw_text(d, note,
              "همه چیز فقط روی همین رایانه ذخیره می‌شود؛ هیچ داده‌ای به اینترنت فرستاده نمی‌شود.",
              F(PERSIAN, 8), C["kTextFaint"], True)


def render_dashboard(layouts, out="dashboard.png", tab=0):
    W, H = 980, 640
    pad = 22
    img = Image.new("RGB", (W, H), C["kBg"])
    d = ImageDraw.Draw(img)

    header_h = 76
    for y in range(header_h):
        d.line([(0, y), (W, y)], fill=mix("#171d2c", C["kBg"], y / header_h))
    lx, ly, logo = 22, 18, 40
    for i in range(logo):
        d.rounded_rectangle([lx, ly, lx + logo, ly + logo], radius=12,
                            fill=mix(C["kAccent"], C["kAccent2"], i / logo))
    icon(d, "Keyboard", lx + logo / 2, ly + logo / 2, 22, "#ffffff", 2)
    draw_text(d, (lx + logo + 14, ly - 4, W - 120, ly + 26), "صفحه کلید و کلیپ‌بورد امین",
              F(PERSIAN_BOLD, 16), C["kText"], True)
    draw_text(d, (lx + logo + 14, ly + 22, W - 120, ly + 42), "AMIN Keyboard & Clipboard",
              F(LATIN, 11), C["kTextFaint"])
    d.line([(W - pad - 72, 20), (W - pad - 40, 52)], fill=C["kTextDim"], width=2)
    d.line([(W - pad - 40, 20), (W - pad - 72, 52)], fill=C["kTextDim"], width=2)
    d.rectangle([W - pad - 104, 36, W - pad - 84, 38], fill=C["kTextDim"])
    d.line([(0, header_h - 1), (W, header_h - 1)], fill=C["kBorderSoft"])

    tabs_y = 88
    tab_w, tab_h = 158, 40
    tk = (pad, tabs_y, pad + tab_w, tabs_y + tab_h)
    tc = (pad + 166, tabs_y, pad + 166 + tab_w, tabs_y + tab_h)
    d.rounded_rectangle([pad, tabs_y, pad + 166 + tab_w, tabs_y + tab_h],
                        radius=tab_h // 2, fill=C["kPanel"])
    active = tk if tab == 0 else tc
    for i in range(tab_w):
        d.line([(active[0] + i, active[1]), (active[0] + i, active[3])],
               fill=mix(C["kAccent"], C["kAccent2"], i / tab_w))
    kb_fg = "#ffffff" if tab == 0 else C["kTextDim"]
    cl_fg = "#ffffff" if tab == 1 else C["kTextDim"]
    d.ellipse([tk[0] + (tab_w / 2 - 52) + 8, tabs_y + 12, tk[0] + (tab_w / 2 - 52) + 24, tabs_y + 28],
              outline=kb_fg, width=2)
    draw_text(d, (tk[0] + tab_w / 2 - 20, tabs_y, tk[0] + tab_w / 2 + 40, tabs_y + tab_h),
              "صفحه کلید", F(PERSIAN_BOLD, 12), kb_fg, True)
    d.rectangle([tc[0] + 30, tc[1] + 13, tc[0] + 46, tc[1] + 29], outline=cl_fg, width=2)
    draw_text(d, (tc[0] + 54, tabs_y, tc[0] + tab_w, tabs_y + tab_h), "کلیپ‌بورد",
              F(PERSIAN_BOLD, 12), cl_fg, True)

    content_y = tabs_y + tab_h + 18
    status_h = 40
    content_h = H - content_y - status_h - 12
    right_w = 300
    left_x = pad
    left_w = W - pad * 2 - right_w - 16
    right_x = left_x + left_w + 16

    if tab == 1:
        _clipboard_tab(d, left_x, content_y, left_w, content_h)
    else:
        pc = (left_x, content_y, left_x + left_w, content_y + 88)
        for i in range(pc[2] - pc[0]):
            d.line([(pc[0] + i, pc[1]), (pc[0] + i, pc[3])],
                   fill=mix("#7c5cff", "#4d8dff", i / left_w))
        icon(d, "Keyboard", pc[0] + 22 + 15, (pc[1] + pc[3]) / 2, 30, "#ffffff", 3)
        draw_text(d, (pc[0] + 22 + 30 + 14, pc[1] + 18, pc[2] - 16, pc[1] + 48), "نمایش صفحه کلید",
                  F(PERSIAN_BOLD, 16), "#ffffff", True)
        draw_text(d, (pc[0] + 22 + 30 + 14, pc[1] + 48, pc[2] - 16, pc[1] + 70), "Show keyboard",
                  F(LATIN, 11), "#e8e8ff")

        by = pc[3] + 14
        half = (left_w - 12) // 2
        for (bx, label, icon_name) in ((left_x, "بستن صفحه کلید", "HideKeyboard"),
                                       (left_x + half + 12, "باز کردن کلیپ‌بورد", None)):
            d.rounded_rectangle([bx, by, bx + half, by + 46], radius=11, fill=C["kCard"],
                                outline=C["kBorder"])
            if icon_name:
                icon(d, icon_name, bx + 22, by + 23, 17, C["kText"], 2)
            else:
                d.rounded_rectangle([bx + 14, by + 14, bx + 32, by + 32], outline=C["kText"], width=2)
            draw_text(d, (bx + 44, by, bx + half - 10, by + 46), label, F(PERSIAN_BOLD, 11),
                      C["kText"], True)

        ty = by + 46 + 18
        for label, sub, on in (("شناور و همیشه روی پنجره‌ها", "On top of other windows", True),
                               ("حالت بدون فوکوس", "Never takes keyboard focus", False)):
            d.rounded_rectangle([left_x, ty + 5, left_x + 44, ty + 29], radius=12,
                                fill=mix("#7c5cff", "#4d8dff", 0.35) if on else C["kBorder"])
            knob_x = left_x + 24 if on else left_x + 3
            d.ellipse([knob_x, ty + 8, knob_x + 18, ty + 26],
                      fill="#ffffff" if on else C["kTextDim"])
            draw_text(d, (left_x + 56, ty - 7, left_x + left_w, ty + 17), label,
                      F(PERSIAN_BOLD, 11), C["kText"], True)
            draw_text(d, (left_x + 56, ty + 13, left_x + left_w, ty + 31), sub, F(LATIN, 9),
                      C["kTextFaint"])
            ty += 46

        pv = (left_x, ty + 12, left_x + left_w, content_y + content_h)
        d.rounded_rectangle(pv, radius=14, fill=C["kCard"], outline=C["kBorderSoft"])
        draw_text(d, (pv[0] + 16, pv[1] + 10, pv[2] - 16, pv[1] + 32), "پیش‌نمایش متن ارسال‌شده",
                  F(PERSIAN_BOLD, 11), C["kTextDim"], True)
        box = (pv[0] + 14, pv[1] + 38, pv[2] - 14, pv[3] - 36)
        d.rounded_rectangle(box, radius=10, fill=C["kBgDeep"], outline=C["kBorderSoft"])
        draw_text(d, (box[0] + 12, box[1], box[2] - 12, box[3]),
                  "سلام دنیا، این متن نمونه است.", F(PERSIAN, 13), C["kText"], True, "right")
        draw_text(d, (pv[0] + 4, pv[3] - 30, pv[2], pv[3] - 10),
                  "هر کلید به پنجره‌ای فرستاده می‌شود که قبل از باز شدن صفحه کلید فعال بوده است.",
                  F(PERSIAN, 9), C["kTextFaint"], True)

    _right_column(d, right_x, right_w, content_y, content_h)

    d.line([(0, H - status_h), (W, H - status_h)], fill=C["kBorderSoft"])
    d.ellipse([26, H - status_h / 2 - 3, 32, H - status_h / 2 + 3], fill=C["kSuccess"])
    draw_text(d, (38, H - status_h, W - 300, H), "آماده", F(PERSIAN, 10), C["kSuccess"], True)
    draw_text(d, (W - 300, H - status_h, W - 24, H), "v1.0.0   ·   ذخیره محلی", F(PERSIAN, 9),
              C["kTextFaint"], True, "right")
    draw_text(d, (0, H - 12, W - 8, H - 1),
              "PREVIEW rendered from the real layout code - not a screenshot of the running app",
              F(LATIN, 8), "#3d4457", False, "right")
    img.save(out)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "docs/preview"))
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    global C
    C = load_palette()
    layouts = parse_layouts()

    files = []
    files.append(render_keyboard(layouts, "fa", 0, os.path.join(args.out, "keyboard-persian.png")))
    files.append(render_keyboard(layouts, "en", 0, os.path.join(args.out, "keyboard-english.png")))
    files.append(render_keyboard(layouts, "fa", 1, os.path.join(args.out, "keyboard-symbols.png")))
    files.append(render_dashboard(layouts, os.path.join(args.out, "dashboard.png"), tab=0))
    files.append(render_dashboard(layouts, os.path.join(args.out, "dashboard-clipboard.png"), tab=1))
    for f in files:
        print("wrote", f)

    # Sanity report against the real invariants.
    for code, data in layouts.items():
        for pi, page in enumerate(data["pages"]):
            for ri, row in enumerate(page):
                total = sum(k["units"] for k in row)
                status = "ok" if abs(total - ROW_UNITS) < 0.001 else "MISMATCH"
                print(f"  {code} page {pi} row {ri}: {len(row)} keys, {total:.2f} units [{status}]")


if __name__ == "__main__":
    sys.exit(main())
