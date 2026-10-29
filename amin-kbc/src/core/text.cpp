#include "text.h"

#include <algorithm>
#include <cstdio>

namespace core {

// ---------------------------------------------------------------------------
//  Code point helpers
// ---------------------------------------------------------------------------
static bool is_high_surrogate(char16_t c) { return c >= 0xD800 && c <= 0xDBFF; }
static bool is_low_surrogate(char16_t c) { return c >= 0xDC00 && c <= 0xDFFF; }

// Decodes one code point starting at index i. Returns the number of UTF-16
// code units consumed (1 or 2). Invalid lone surrogates are yielded as U+FFFD.
static size_t next_cp(const u16& s, size_t i, uint32_t* cp) {
    const char16_t c = s[i];
    if (is_high_surrogate(c) && i + 1 < s.size() && is_low_surrogate(s[i + 1])) {
        *cp = 0x10000u + ((static_cast<uint32_t>(c) - 0xD800u) << 10) +
              (static_cast<uint32_t>(s[i + 1]) - 0xDC00u);
        return 2;
    }
    if (is_high_surrogate(c) || is_low_surrogate(c)) {
        *cp = 0xFFFD;
        return 1;
    }
    *cp = c;
    return 1;
}

// ---------------------------------------------------------------------------
//  UTF-8 <-> UTF-16
// ---------------------------------------------------------------------------
std::string u16_to_utf8(const u16& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        uint32_t cp = 0;
        i += next_cp(s, i, &cp);
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

u16 utf8_to_u16(const std::string& s) {
    u16 out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char b0 = static_cast<unsigned char>(s[i]);
        uint32_t cp = 0xFFFD;
        size_t len = 1;
        if (b0 < 0x80) {
            cp = b0;
        } else if ((b0 & 0xE0) == 0xC0) {
            len = 2;
            if (i + len <= s.size()) {
                const unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
                if ((b1 & 0xC0) == 0x80) cp = ((b0 & 0x1Fu) << 6) | (b1 & 0x3Fu);
            }
            if (cp < 0x80) cp = 0xFFFD;  // overlong
        } else if ((b0 & 0xF0) == 0xE0) {
            len = 3;
            if (i + len <= s.size()) {
                const unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
                const unsigned char b2 = static_cast<unsigned char>(s[i + 2]);
                if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80)
                    cp = ((b0 & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu);
            }
            if (cp < 0x800) cp = 0xFFFD;  // overlong
            if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;  // surrogate half
        } else if ((b0 & 0xF8) == 0xF0) {
            len = 4;
            if (i + len <= s.size()) {
                const unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
                const unsigned char b2 = static_cast<unsigned char>(s[i + 2]);
                const unsigned char b3 = static_cast<unsigned char>(s[i + 3]);
                if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80)
                    cp = ((b0 & 0x07u) << 18) | ((b1 & 0x3Fu) << 12) | ((b2 & 0x3Fu) << 6) |
                         (b3 & 0x3Fu);
            }
            if (cp < 0x10000 || cp > 0x10FFFF) cp = 0xFFFD;
        }
        i += len;
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
//  Script detection
//
//  Covers the Arabic block (which Persian uses), Arabic Supplement, Arabic
//  Presentation Forms A/B and the "Extended Arabic-Indic" / Persian digits.
//  Everything in these ranges is treated as right-to-left.
// ---------------------------------------------------------------------------
bool is_arabic_script(char16_t c) {
    return (c >= 0x0600 && c <= 0x06FF) ||  // Arabic (includes Persian letters)
           (c >= 0x0750 && c <= 0x077F) ||  // Arabic Supplement
           (c >= 0x08A0 && c <= 0x08FF) ||  // Arabic Extended-A
           (c >= 0xFB50 && c <= 0xFDFF) ||  // Presentation Forms-A
           (c >= 0xFE70 && c <= 0xFEFF);    // Presentation Forms-B
}

bool is_rtl_char(char16_t c) {
    // Hebrew is included for completeness even though the UI is FA/EN.
    if (c >= 0x0590 && c <= 0x05FF) return true;
    if (c >= 0xFB1D && c <= 0xFDFF) return true;
    if (c >= 0xFE70 && c <= 0xFEFF) return true;
    if (is_arabic_script(c)) {
        // Arabic digits are weak (they render left-to-right even in RTL text),
        // but the paragraphs around them are still RTL, so keep them out of the
        // "RTL letter" test to avoid mis-detecting a pure number as RTL.
        if (c >= 0x0660 && c <= 0x0669) return false;  // Arabic-Indic digits
        if (c >= 0x06F0 && c <= 0x06F9) return false;  // Persian digits
        return true;
    }
    return false;
}

bool has_rtl(const u16& s) {
    for (char16_t c : s) {
        if (is_rtl_char(c)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
//  Cleaning
// ---------------------------------------------------------------------------
u16 strip_nul(const u16& s) {
    u16 out;
    out.reserve(s.size());
    for (char16_t c : s) {
        if (c != 0) out.push_back(c);
    }
    return out;
}

u16 normalize_newlines(const u16& s) {
    u16 out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == u'\r') {
            out.push_back(u'\n');
            if (i + 1 < s.size() && s[i + 1] == u'\n') ++i;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

u16 clean_clipboard_text(const u16& s) { return normalize_newlines(strip_nul(s)); }

bool is_blank(const u16& s) {
    for (char16_t c : s) {
        if (c != u' ' && c != u'\t' && c != u'\n' && c != u'\r' && c != 0x00A0 &&
            c != 0x200B && c != 0x200C && c != 0xFEFF)
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  Comparison / search
// ---------------------------------------------------------------------------
char16_t fold_char(char16_t c) {
    if (c >= u'A' && c <= u'Z') return static_cast<char16_t>(c - u'A' + u'a');
    // Persian/Arabic letter variants users expect to be equivalent in a search.
    switch (c) {
        case 0x064A: return 0x06CC;  // ARABIC YEH  -> FARSI YEH
        case 0x0643: return 0x06A9;  // ARABIC KAF  -> KEHEH
        case 0x0629: return 0x0647;  // TEH MARBUTA -> HEH
        case 0x0623:                 // ALEF WITH HAMZA ABOVE
        case 0x0625:                 // ALEF WITH HAMZA BELOW
        case 0x0622: return 0x0627;  // ALEF WITH MADDA -> ALEF
        case 0x0649: return 0x06CC;  // ALEF MAKSURA -> FARSI YEH
        default: return c;
    }
}

u16 trim(const u16& s) {
    size_t b = 0, e = s.size();
    auto is_space = [](char16_t c) {
        return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == 0x00A0 ||
               c == 0x200B || c == 0xFEFF;
    };
    while (b < e && is_space(s[b])) ++b;
    while (e > b && is_space(s[e - 1])) --e;
    return s.substr(b, e - b);
}

bool equals_ci(const u16& a, const u16& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (fold_char(a[i]) != fold_char(b[i])) return false;
    }
    return true;
}

bool contains_ci(const u16& haystack, const u16& needle) {
    if (needle.empty()) return true;
    if (needle.size() > haystack.size()) return false;
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (fold_char(haystack[i + j]) != fold_char(needle[j])) {
                ok = false;
                break;
            }
        }
        if (ok) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
//  Display helpers
// ---------------------------------------------------------------------------
u16 make_preview(const u16& s, size_t max_chars) {
    u16 out;
    out.reserve(std::min(s.size(), max_chars + 1));
    bool last_space = true;
    for (size_t i = 0; i < s.size(); ++i) {
        char16_t c = s[i];
        if (c == u'\n' || c == u'\r' || c == u'\t' || c == u' ') {
            if (!last_space && !out.empty()) out.push_back(u' ');
            last_space = true;
        } else {
            if (out.size() >= max_chars) {
                u16 truncated = out;
                // Avoid a trailing space before the ellipsis.
                while (!truncated.empty() && truncated.back() == u' ') truncated.pop_back();
                truncated.push_back(0x2026);  // horizontal ellipsis
                return truncated;
            }
            out.push_back(c);
            last_space = false;
        }
    }
    while (!out.empty() && out.back() == u' ') out.pop_back();
    if (out.size() > max_chars) {
        out.resize(max_chars);
        out.push_back(0x2026);
    }
    return out;
}

u16 to_persian_digits(const u16& s) {
    u16 out = s;
    for (char16_t& c : out) {
        if (c >= u'0' && c <= u'9') c = static_cast<char16_t>(c - u'0' + 0x06F0);
    }
    return out;
}

u16 format_number(int64_t value, bool persian) {
    std::string ascii;
    bool neg = value < 0;
    uint64_t v = neg ? static_cast<uint64_t>(-(value + 1)) + 1 : static_cast<uint64_t>(value);
    if (v == 0) ascii = "0";
    while (v > 0) {
        ascii.insert(ascii.begin(), static_cast<char>('0' + (v % 10)));
        v /= 10;
    }
    if (neg) ascii.insert(ascii.begin(), '-');
    u16 out = u16_from_ascii(ascii.c_str());
    return persian ? to_persian_digits(out) : out;
}

u16 format_relative_time(int64_t now_unix, int64_t then_unix, bool persian) {
    int64_t d = now_unix - then_unix;
    if (d < 0) d = 0;
    if (d < 10) return persian ? u16(u"همین حالا") : u16(u"just now");
    if (d < 60) {
        u16 n = format_number(d, persian);
        return persian ? n + u" ثانیه پیش" : n + u" seconds ago";
    }
    if (d < 3600) {
        u16 n = format_number(d / 60, persian);
        return persian ? n + u" دقیقه پیش" : n + u" minutes ago";
    }
    if (d < 86400) {
        u16 n = format_number(d / 3600, persian);
        return persian ? n + u" ساعت پیش" : n + u" hours ago";
    }
    if (d < 86400 * 7) {
        u16 n = format_number(d / 86400, persian);
        return persian ? n + u" روز پیش" : n + u" days ago";
    }
    // Older entries: show the calendar date (Gregorian, unambiguous).
    // Timestamps are stored as Unix seconds; convert without <ctime> so the
    // function stays deterministic and portable.
    const int64_t days = then_unix / 86400;
    int64_t z = days + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t y = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    const int64_t dd = doy - (153 * mp + 2) / 5 + 1;
    const int64_t mm = mp < 10 ? mp + 3 : mp - 9;
    const int64_t yy = (mm <= 2) ? y + 1 : y;
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", static_cast<int>(yy), static_cast<int>(mm),
                  static_cast<int>(dd));
    u16 out = u16_from_ascii(buf);
    return persian ? to_persian_digits(out) : out;
}

u16 u16_from_ascii(const char* s) {
    u16 out;
    if (!s) return out;
    for (const char* p = s; *p; ++p) out.push_back(static_cast<char16_t>(static_cast<unsigned char>(*p)));
    return out;
}

std::string ascii_from_u16(const u16& s) {
    std::string out;
    out.reserve(s.size());
    for (char16_t c : s) out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    return out;
}

bool starts_with(const u16& s, const u16& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

u16 replace_all(const u16& s, const u16& from, const u16& to) {
    if (from.empty()) return s;
    u16 out;
    size_t i = 0;
    while (i < s.size()) {
        if (s.compare(i, from.size(), from) == 0) {
            out += to;
            i += from.size();
        } else {
            out.push_back(s[i]);
            ++i;
        }
    }
    return out;
}

}  // namespace core
