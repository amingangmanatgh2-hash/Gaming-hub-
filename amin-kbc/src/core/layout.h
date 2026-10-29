// =============================================================================
//  AMIN Keyboard & Clipboard - core/layout.h
//  Keyboard definitions (English + Persian) and row geometry math.
//
//  Every row is exactly kRowUnits (15) key-units wide so that keys line up
//  vertically between rows. tests/test_logic.cpp asserts this.
// =============================================================================
#pragma once

#include <cstdint>
#include <vector>

#include "text.h"

namespace core {

const float kRowUnits = 15.0f;

enum class KeyAction : uint8_t {
    Character,    // types text/text_shift with SendInput KEYEVENTF_UNICODE
    VirtualKey,   // sends vk (Backspace, Enter, Tab, Escape)
    Shift,        // toggles shift for the next character
    CapsLock,     // locks shift
    Language,     // switch Persian <-> English
    Symbols,      // switch letters <-> symbols page
    Space,
    Backspace,
    Enter,
    Tab,
    Escape,
    HideKeyboard
};

struct KeyDef {
    const char16_t* label;        // shown when shift is inactive
    const char16_t* label_shift;  // shown when shift is active (nullptr = same)
    const char16_t* text;         // typed text, shift inactive
    const char16_t* text_shift;   // typed text, shift active
    uint16_t vk;                  // VirtualKey target
    KeyAction action;
    float units;                  // width, kRowUnits per full row
};

struct RowDef {
    const KeyDef* keys;
    int count;
};

struct PageDef {
    const char16_t* name;
    const RowDef* rows;
    int row_count;
};

struct LayoutDef {
    const char16_t* code;      // "fa" / "en"
    const char16_t* name;      // native name
    int lang_index;            // index in the language switch order
    const PageDef* pages;
    int page_count;
    bool rtl;                  // text direction of the key labels
};

extern const LayoutDef kLayoutEnglish;
extern const LayoutDef kLayoutPersian;

const LayoutDef& layout_by_index(int lang_index);
int layout_count();

// Label/text of a key taking the shift state into account.
const char16_t* key_label(const KeyDef& k, bool shift);
const char16_t* key_text(const KeyDef& k, bool shift);

// ---- Geometry --------------------------------------------------------------
struct KeyPlacement {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// Lays out one row of keys across inner_width pixels. gap pixels separate the
// keys. The result has no overlaps and spans exactly [0, inner_width).
void compute_row(const RowDef& row, int inner_width, int gap, int y, int h,
                 std::vector<KeyPlacement>* out);

}  // namespace core
