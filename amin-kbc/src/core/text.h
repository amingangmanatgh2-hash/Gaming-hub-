// =============================================================================
//  AMIN Keyboard & Clipboard - core/text.h
//  Portable text helpers (UTF-8 <-> UTF-16, bidi detection, previews, numbers).
//
//  This file MUST stay free of Windows headers: it is compiled and unit-tested
//  on the build host as well as on Windows (see tests/ and --selftest).
// =============================================================================
#pragma once

#include <cstdint>
#include <string>

namespace core {

using u16 = std::u16string;

// ---- UTF-8 <-> UTF-16 -------------------------------------------------------
// Invalid input is replaced with U+FFFD; never throws, never returns partial
// surrogate pairs.
std::string u16_to_utf8(const u16& s);
u16 utf8_to_u16(const std::string& s);

// ---- Bidi / Persian detection ----------------------------------------------
bool is_rtl_char(char16_t c);
bool has_rtl(const u16& s);
bool is_arabic_script(char16_t c);

// ---- Cleaning ---------------------------------------------------------------
// Drops U+0000 (clipboard text is NUL terminated; some apps leak embedded NULs).
u16 strip_nul(const u16& s);
// CRLF -> LF, lone CR -> LF.
u16 normalize_newlines(const u16& s);
// Both of the above, in the order the clipboard monitor needs them.
u16 clean_clipboard_text(const u16& s);
bool is_blank(const u16& s);   // empty or only spaces/tabs/newlines

// ---- Comparison / search ----------------------------------------------------
// ASCII case folding only (Persian has no case); other code points compare
// exactly. This is what the clipboard search box uses.
char16_t fold_char(char16_t c);
bool equals_ci(const u16& a, const u16& b);
bool contains_ci(const u16& haystack, const u16& needle);
u16 trim(const u16& s);

// ---- Display ---------------------------------------------------------------
// Single line, whitespace collapsed, ellipsis when longer than max_chars.
u16 make_preview(const u16& s, size_t max_chars);
// Converts ASCII digits to Persian digits (U+06F0..U+06F9).
u16 to_persian_digits(const u16& s);
// "۱۲ دقیقه پیش" / "12 minutes ago"
u16 format_relative_time(int64_t now_unix, int64_t then_unix, bool persian);
u16 format_number(int64_t value, bool persian);

// ---- Small utilities -------------------------------------------------------
u16 u16_from_ascii(const char* s);
std::string ascii_from_u16(const u16& s);   // non-ASCII bytes become '?'
bool starts_with(const u16& s, const u16& prefix);
u16 replace_all(const u16& s, const u16& from, const u16& to);

}  // namespace core
