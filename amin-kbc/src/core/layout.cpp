#include "layout.h"

#include <cmath>

namespace core {

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
#define K(label, shift_label) \
    { label, shift_label, label, shift_label, 0, KeyAction::Character, 1.0f }
#define KU(units, label, shift_label) \
    { label, shift_label, label, shift_label, 0, KeyAction::Character, units }
#define KA(action, label, shift_label, units) \
    { label, shift_label, nullptr, nullptr, 0, KeyAction::action, units }
#define KVK(action, vk, label, shift_label, units) \
    { label, shift_label, nullptr, nullptr, vk, KeyAction::action, units }

// ===========================================================================
//  ENGLISH - letters page
// ===========================================================================
static const KeyDef kEnRow0[] = {
    K(u"`", u"~"), K(u"1", u"!"), K(u"2", u"@"), K(u"3", u"#"), K(u"4", u"$"),
    K(u"5", u"%"), K(u"6", u"^"), K(u"7", u"&"), K(u"8", u"*"), K(u"9", u"("),
    K(u"0", u")"), K(u"-", u"_"), K(u"=", u"+"),
    KVK(Backspace, 0x08, u"", nullptr, 2.0f),
};
static const KeyDef kEnRow1b[] = {
    KVK(Tab, 0x09, u"", nullptr, 1.5f),
    K(u"q", u"Q"), K(u"w", u"W"), K(u"e", u"E"), K(u"r", u"R"), K(u"t", u"T"),
    K(u"y", u"Y"), K(u"u", u"U"), K(u"i", u"I"), K(u"o", u"O"), K(u"p", u"P"),
    K(u"[", u"{"), K(u"]", u"}"), KU(1.5f, u"\\", u"|"),
};
static const KeyDef kEnRow2[] = {
    KA(CapsLock, u"Caps", nullptr, 1.75f),
    K(u"a", u"A"), K(u"s", u"S"), K(u"d", u"D"), K(u"f", u"F"), K(u"g", u"G"),
    K(u"h", u"H"), K(u"j", u"J"), K(u"k", u"K"), K(u"l", u"L"), K(u";", u":"),
    K(u"'", u"\""),
    KVK(Enter, 0x0D, u"", nullptr, 2.25f),
};
static const KeyDef kEnRow3[] = {
    KA(Shift, u"Shift", nullptr, 2.25f),
    K(u"z", u"Z"), K(u"x", u"X"), K(u"c", u"C"), K(u"v", u"V"), K(u"b", u"B"),
    K(u"n", u"N"), K(u"m", u"M"), K(u",", u"<"), K(u".", u">"), K(u"/", u"?"),
    KA(Shift, u"Shift", nullptr, 2.75f),
};
static const KeyDef kEnRow4[] = {
    KA(Language, u"\u0641\u0627\u0631\u0633\u06cc", nullptr, 1.6f),  // "فارسی"
    KA(Symbols, u"!@#", nullptr, 1.4f), KA(Space, u"", nullptr, 9.4f),
    KVK(Escape, 0x1B, u"Esc", nullptr, 1.2f),
    KA(HideKeyboard, u"", nullptr, 1.4f),
};
static const RowDef kEnLetterRows[] = {
    {kEnRow0, 14}, {kEnRow1b, 14}, {kEnRow2, 13}, {kEnRow3, 12}, {kEnRow4, 5},
};

// ===========================================================================
//  ENGLISH - symbols page
// ===========================================================================
static const KeyDef kEnSym0[] = {
    K(u"!", u"!"), K(u"@", u"@"), K(u"#", u"#"), K(u"$", u"$"), K(u"%", u"%"),
    K(u"^", u"^"), K(u"&", u"&"), K(u"*", u"*"), K(u"(", u"("), K(u")", u")"),
    K(u"-", u"-"), K(u"=", u"="), K(u"+", u"+"),
    KVK(Backspace, 0x08, u"", nullptr, 2.0f),
};
static const KeyDef kEnSym1[] = {
    KVK(Tab, 0x09, u"", nullptr, 1.5f),
    K(u"[", u"["), K(u"]", u"]"), K(u"{", u"{"), K(u"}", u"}"), K(u"\\", u"\\"),
    K(u"|", u"|"), K(u";", u";"), K(u":", u":"), K(u"\"", u"\""), K(u"'", u"'"),
    K(u"<", u"<"), K(u">", u">"), KU(1.5f, u"/", u"/"),
};
static const KeyDef kEnSym2[] = {
    KVK(Escape, 0x1B, u"Esc", nullptr, 1.75f),
    K(u"~", u"~"), K(u"`", u"`"), K(u"_", u"_"), K(u"\u20ac", u"\u20ac"),  // €
    K(u"\u00a3", u"\u00a3"),                                              // £
    K(u"\u00a5", u"\u00a5"),                                              // ¥
    K(u"\u2022", u"\u2022"),                                              // •
    K(u"\u00a9", u"\u00a9"),                                              // ©
    K(u"\u00ae", u"\u00ae"),                                              // ®
    K(u"\u00b0", u"\u00b0"),                                              // °
    K(u"\u00b5", u"\u00b5"),                                              // µ
    KVK(Enter, 0x0D, u"", nullptr, 2.25f),
};
static const KeyDef kEnSym3[] = {
    KA(Shift, u"Shift", nullptr, 2.25f),
    K(u"\u00b1", u"\u00b1"), K(u"\u00d7", u"\u00d7"), K(u"\u00f7", u"\u00f7"),
    K(u"\u2260", u"\u2260"), K(u"\u2264", u"\u2264"), K(u"\u2265", u"\u2265"),
    K(u"\u221e", u"\u221e"), K(u"\u00a7", u"\u00a7"), K(u"\u00b6", u"\u00b6"),
    K(u"\u2026", u"\u2026"),
    KA(Shift, u"Shift", nullptr, 2.75f),
};
static const KeyDef kEnSym4[] = {
    KA(Language, u"\u0641\u0627\u0631\u0633\u06cc", nullptr, 1.6f),
    KA(Symbols, u"abc", nullptr, 1.4f), KA(Space, u"", nullptr, 9.4f),
    KVK(Escape, 0x1B, u"Esc", nullptr, 1.2f),
    KA(HideKeyboard, u"", nullptr, 1.4f),
};
static const RowDef kEnSymbolRows[] = {
    {kEnSym0, 14}, {kEnSym1, 14}, {kEnSym2, 13}, {kEnSym3, 12}, {kEnSym4, 5},
};

static const PageDef kEnPages[] = {
    {u"ABC", kEnLetterRows, 5},
    {u"!@#", kEnSymbolRows, 5},
};

const LayoutDef kLayoutEnglish = {u"en", u"English", 1, kEnPages, 2, false};

// ===========================================================================
//  PERSIAN - letters page
//
//  Letter positions follow the standard Windows/ISIRI Persian layout:
//      row1: ض ص ث ق ف غ ع ه خ ح ج چ
//      row2: ش س ی ب ل ا ت ن م ک گ
//      row3: ظ ط ز ر ذ د پ و . /
//  Shift adds the letters Persian typists need most:
//      Shift+ا = آ, Shift+ز = ژ, Shift+. = ،, Shift+/ = ؟
//  plus harakat on row1. The number row types Persian digits.
// ===========================================================================
static const KeyDef kFaRow0[] = {
    K(u"\u06f1", u"!"),  // ۱
    K(u"\u06f2", u"@"),  // ۲
    K(u"\u06f3", u"#"),  // ۳
    K(u"\u06f4", u"$"),  // ۴
    K(u"\u06f5", u"%"),  // ۵
    K(u"\u06f6", u"^"),  // ۶
    K(u"\u06f7", u"&"),  // ۷
    K(u"\u06f8", u"*"),  // ۸
    K(u"\u06f9", u"("),  // ۹
    K(u"\u06f0", u")"),  // ۰
    K(u"\u066b", u"\u066b"),  // ٫ Arabic decimal separator
    K(u"\u066c", u"\u066c"),  // ٬ Arabic thousands separator
    KVK(Backspace, 0x08, u"", nullptr, 3.0f),
};
static const KeyDef kFaRow1[] = {
    KVK(Tab, 0x09, u"", nullptr, 1.5f),
    K(u"\u0636", u"\u064b"),  // ض / ً
    K(u"\u0635", u"\u064c"),  // ص / ٌ
    K(u"\u062b", u"\u064d"),  // ث / ٍ
    K(u"\u0642", u"\u064e"),  // ق / َ
    K(u"\u0641", u"\u064f"),  // ف / ُ
    K(u"\u063a", u"\u0650"),  // غ / ِ
    K(u"\u0639", u"\u0651"),  // ع / ّ
    K(u"\u0647", u"\u0652"),  // ه / ْ
    K(u"\u062e", u"\u00ab"),  // خ / «
    K(u"\u062d", u"\u00bb"),  // ح / »
    K(u"\u062c", u"\u061b"),  // ج / ؛
    K(u"\u0686", u"\u061f"),  // چ / ؟
    {u"\u0646\u06cc\u0645\u200c\u0641\u0627\u0635\u0644\u0647",
     u"\u0646\u06cc\u0645\u200c\u0641\u0627\u0635\u0644\u0647", u"\u200c", u"\u200c", 0,
     KeyAction::Character, 1.5f},  // نیم‌فاصله (zero-width non-joiner)
};
static const KeyDef kFaRow2[] = {
    KA(CapsLock, u"Caps", nullptr, 1.75f),
    K(u"\u0634", u"\u0634"),   // ش
    K(u"\u0633", u"\u0633"),   // س
    K(u"\u06cc", u"\u064a"),   // ی / ي
    K(u"\u0628", u"\u0628"),   // ب
    K(u"\u0644", u"\u0644"),   // ل
    K(u"\u0627", u"\u0622"),   // ا / آ
    K(u"\u062a", u"\u062a"),   // ت
    K(u"\u0646", u"\u0646"),   // ن
    K(u"\u0645", u"\u0645"),   // م
    K(u"\u06a9", u"\u0643"),   // ک / ك
    K(u"\u06af", u"\u06af"),   // گ
    KVK(Enter, 0x0D, u"", nullptr, 2.25f),
};
static const KeyDef kFaRow3[] = {
    KA(Shift, u"Shift", nullptr, 2.25f),
    K(u"\u0638", u"\u0638"),   // ظ
    K(u"\u0637", u"\u0637"),   // ط
    K(u"\u0632", u"\u0698"),   // ز / ژ
    K(u"\u0631", u"\u0631"),   // ر
    K(u"\u0630", u"\u0630"),   // ذ
    K(u"\u062f", u"\u062f"),   // د
    K(u"\u067e", u"\u067e"),   // پ
    K(u"\u0648", u"\u0648"),   // و
    K(u".", u"\u060c"),        // . / ،
    K(u"/", u"\u061f"),        // / / ؟
    KA(Shift, u"Shift", nullptr, 2.75f),
};
static const KeyDef kFaRow4[] = {
    KA(Language, u"EN", nullptr, 1.6f),
    KA(Symbols, u"\u06f1\u06f2\u06f3", nullptr, 1.4f),  // ۱۲۳
    KA(Space, u"", nullptr, 9.4f),
    KVK(Escape, 0x1B, u"Esc", nullptr, 1.2f),
    KA(HideKeyboard, u"", nullptr, 1.4f),
};
static const RowDef kFaLetterRows[] = {
    {kFaRow0, 13}, {kFaRow1, 14}, {kFaRow2, 13}, {kFaRow3, 12}, {kFaRow4, 5},
};

// ===========================================================================
//  PERSIAN - symbols page
// ===========================================================================
static const KeyDef kFaSym0[] = {
    K(u"\u06f0", u"\u06f0"), K(u"\u06f1", u"\u06f1"), K(u"\u06f2", u"\u06f2"),
    K(u"\u06f3", u"\u06f3"), K(u"\u06f4", u"\u06f4"), K(u"\u06f5", u"\u06f5"),
    K(u"\u06f6", u"\u06f6"), K(u"\u06f7", u"\u06f7"), K(u"\u06f8", u"\u06f8"),
    K(u"\u06f9", u"\u06f9"), K(u"\u066b", u"\u066b"), K(u"\u066c", u"\u066c"),
    K(u"/", u"/"), KVK(Backspace, 0x08, u"", nullptr, 2.0f),
};
static const KeyDef kFaSym1[] = {
    KVK(Tab, 0x09, u"", nullptr, 1.5f),
    K(u"\u060c", u"\u060c"),  // ،
    K(u"\u061b", u"\u061b"),  // ؛
    K(u"\u061f", u"\u061f"),  // ؟
    K(u"\u00ab", u"\u00ab"),  // «
    K(u"\u00bb", u"\u00bb"),  // »
    K(u"\u066a", u"\u066a"),  // ٪
    K(u"\ufdfc", u"\ufdfc"),  // ﷼
    K(u"(", u"("),
    K(u")", u")"),
    K(u"[", u"["),
    K(u"]", u"]"),
    KU(2.5f, u"\u2013", u"\u2014"),  // – / —
};
static const KeyDef kFaSym2[] = {
    KVK(Escape, 0x1B, u"Esc", nullptr, 1.75f),
    K(u"\u064b", u"\u064b"), K(u"\u064c", u"\u064c"), K(u"\u064d", u"\u064d"),
    K(u"\u064e", u"\u064e"), K(u"\u064f", u"\u064f"), K(u"\u0650", u"\u0650"),
    K(u"\u0651", u"\u0651"), K(u"\u0652", u"\u0652"), K(u"\u0640", u"\u0640"),
    K(u"*", u"*"), K(u"@", u"@"),
    KVK(Enter, 0x0D, u"", nullptr, 2.25f),
};
static const KeyDef kFaSym3[] = {
    KA(Shift, u"Shift", nullptr, 2.25f),
    K(u"#", u"#"), K(u"$", u"$"), K(u"%", u"%"), K(u"^", u"^"), K(u"&", u"&"),
    K(u"|", u"|"), K(u"\\", u"\\"), K(u"~", u"~"), K(u"\u2026", u"\u2026"),
    K(u"+", u"+"),
    KA(Shift, u"Shift", nullptr, 2.75f),
};
static const KeyDef kFaSym4[] = {
    KA(Language, u"EN", nullptr, 1.6f),
    KA(Symbols, u"ABC", nullptr, 1.4f), KA(Space, u"", nullptr, 9.4f),
    KVK(Escape, 0x1B, u"Esc", nullptr, 1.2f),
    KA(HideKeyboard, u"", nullptr, 1.4f),
};
static const RowDef kFaSymbolRows[] = {
    {kFaSym0, 14}, {kFaSym1, 13}, {kFaSym2, 13}, {kFaSym3, 12}, {kFaSym4, 5},
};

static const PageDef kFaPages[] = {
    {u"\u0627\u0628\u062c", kFaLetterRows, 5},   // ابجد
    {u"\u06f1\u06f2\u06f3", kFaSymbolRows, 5},   // ۱۲۳
};

const LayoutDef kLayoutPersian = {u"fa", u"\u0641\u0627\u0631\u0633\u06cc", 0, kFaPages, 2, true};

// ---------------------------------------------------------------------------
//  Table lookup
// ---------------------------------------------------------------------------
const LayoutDef& layout_by_index(int lang_index) {
    return (lang_index == 0) ? kLayoutPersian : kLayoutEnglish;
}

int layout_count() { return 2; }

const char16_t* key_label(const KeyDef& k, bool shift) {
    if (shift && k.label_shift && k.label_shift[0]) return k.label_shift;
    return k.label ? k.label : u"";
}

const char16_t* key_text(const KeyDef& k, bool shift) {
    if (shift && k.text_shift && k.text_shift[0]) return k.text_shift;
    return k.text ? k.text : u"";
}

void compute_row(const RowDef& row, int inner_width, int gap, int y, int h,
                 std::vector<KeyPlacement>* out) {
    if (!out) return;
    out->clear();
    if (row.count <= 0 || inner_width <= 0) return;
    if (gap < 0) gap = 0;

    // Total units (defensive: fall back to 1.0 per key if the table is broken).
    float total_units = 0.0f;
    for (int i = 0; i < row.count; ++i) {
        float u = row.keys[i].units;
        if (!(u > 0.0f)) u = 1.0f;
        total_units += u;
    }
    if (total_units <= 0.0f) total_units = static_cast<float>(row.count);

    const int total_gap = gap * (row.count - 1);
    const int usable = inner_width - total_gap;
    if (usable <= 0) return;
    const float unit_px = static_cast<float>(usable) / total_units;

    float cursor = 0.0f;
    for (int i = 0; i < row.count; ++i) {
        float u = row.keys[i].units;
        if (!(u > 0.0f)) u = 1.0f;
        const float w = u * unit_px;
        KeyPlacement p;
        p.x = static_cast<int>(std::lround(cursor));
        p.y = y;
        p.h = h;
        const int x2 = static_cast<int>(std::lround(cursor + w));
        p.w = x2 - p.x;
        if (i == row.count - 1) {
            // Snap the last key to the right edge to absorb rounding error.
            p.w = inner_width - p.x;
        }
        if (p.w < 1) p.w = 1;
        out->push_back(p);
        cursor += w + static_cast<float>(gap);
    }
}

}  // namespace core
