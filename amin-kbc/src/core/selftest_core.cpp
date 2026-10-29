#include "selftest_core.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "hotkey.h"
#include "json.h"
#include "layout.h"
#include "settings.h"
#include "store.h"
#include "text.h"

namespace core {
namespace {

class Harness {
public:
    void check(bool cond, const std::string& what) {
        if (cond) {
            ++passed_;
        } else {
            ++failed_;
            log_ += "[FAIL] " + what + "\n";
        }
    }
    void info(const std::string& what) { log_ += "       " + what + "\n"; }

    TestSummary summary() const {
        TestSummary s;
        s.passed = passed_;
        s.failed = failed_;
        s.log = log_;
        return s;
    }

private:
    int passed_ = 0;
    int failed_ = 0;
    std::string log_;
};

// Persian letters that a Persian keyboard must be able to type.
const char16_t* const kPersianLetters[] = {
    u"\u0627", u"\u0628", u"\u067e", u"\u062a", u"\u062b", u"\u062c", u"\u0686", u"\u062d",
    u"\u062e", u"\u062f", u"\u0630", u"\u0631", u"\u0632", u"\u0698", u"\u0633", u"\u0634",
    u"\u0635", u"\u0636", u"\u0637", u"\u0638", u"\u0639", u"\u063a", u"\u0641", u"\u0642",
    u"\u06a9", u"\u06af", u"\u0644", u"\u0645", u"\u0646", u"\u0648", u"\u0647", u"\u06cc",
};

// Collects every character that can be typed with a layout (both pages, both
// shift states).
std::u16string typed_characters(const LayoutDef& layout) {
    std::u16string all;
    for (int p = 0; p < layout.page_count; ++p) {
        const PageDef& page = layout.pages[p];
        for (int r = 0; r < page.row_count; ++r) {
            const RowDef& row = page.rows[r];
            for (int k = 0; k < row.count; ++k) {
                const KeyDef& key = row.keys[k];
                if (key.action == KeyAction::Space) {
                    all += u" ";
                    continue;
                }
                if (key.action != KeyAction::Character) continue;
                const char16_t* t = key_text(key, false);
                if (t) all += t;
                t = key_text(key, true);
                if (t) all += t;
            }
        }
    }
    return all;
}

bool contains_cp(const std::u16string& s, char16_t c) { return s.find(c) != std::u16string::npos; }

void test_text(Harness& h) {
    const std::u16string fa = u"\u0633\u0644\u0627\u0645 \u062f\u0646\u06cc\u0627";  // سلام دنیا
    const std::string utf8 = u16_to_utf8(fa);
    h.check(utf8 == "\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85 \xD8\xAF\xD9\x86\xDB\x8C\xD8\xA7",
            "UTF-8 encoding of Persian text is correct");
    h.check(utf8_to_u16(utf8) == fa, "UTF-8 -> UTF-16 round trip (Persian)");

    const std::u16string emoji = u"\U0001F600 OK";  // surrogate pair + ASCII
    h.check(utf8_to_u16(u16_to_utf8(emoji)) == emoji, "UTF-8 round trip (surrogate pair)");

    const std::string bad = "\xFF\xFE abc";
    const std::u16string fixed = utf8_to_u16(bad);
    h.check(fixed.find(0xFFFD) != std::u16string::npos, "invalid UTF-8 becomes U+FFFD");

    h.check(has_rtl(fa), "Persian text detected as RTL");
    h.check(!has_rtl(u"hello"), "English text is not RTL");
    h.check(!has_rtl(u"12345"), "ASCII digits are not RTL");

    h.check(contains_ci(u"Hello World", u"WORLD"), "ASCII case-insensitive search");
    h.check(contains_ci(u"\u06cc\u0627\u062f\u062f\u0627\u0634\u062a", u"\u064a\u0627"),  // يادداشت vs يا
            "Persian yeh/kaf variants match in search");
    h.check(!contains_ci(u"abc", u"abcd"), "search does not match longer needle");

    const std::u16string multi = u"line one\nline two\t  end";
    h.check(make_preview(multi, 100) == u"line one line two end", "preview collapses whitespace");
    const std::u16string clipped = make_preview(u"abcdefghij", 4);
    h.check(clipped.size() == 5 && clipped.back() == 0x2026, "preview adds ellipsis");

    h.check(to_persian_digits(u"2026") == u"\u06f2\u06f0\u06f2\u06f6", "ASCII digits -> Persian digits");
    h.check(format_number(-120, false) == u"-120", "negative number formatting");
    h.check(format_number(1234567, true).size() == 7, "large Persian number formatting");

    h.check(format_relative_time(1000, 999, true) == u"\u0647\u0645\u06cc\u0646 \u062d\u0627\u0644\u0627",
            "relative time: just now (Persian)");
    h.check(format_relative_time(1000 + 120, 1000, false) == u"2 minutes ago", "relative time: minutes");
    h.check(format_relative_time(1000 + 3 * 86400, 1000, false) == u"3 days ago", "relative time: days");
    h.check(format_relative_time(0, 0, false) == u"just now", "relative time: epoch");

    h.check(strip_nul(std::u16string(u"a\0b", 3)) == u"ab", "NUL characters stripped");
    h.check(normalize_newlines(u"a\r\nb\rc") == u"a\nb\nc", "CRLF/CR normalised");
    h.check(is_blank(u" \t\n\u200c"), "whitespace-only text detected");
    h.check(!is_blank(u" . "), "punctuation is not blank");
    h.check(trim(u"  x  ") == u"x", "trim");
    h.check(equals_ci(u"ABC", u"abc"), "equals_ci");
}

void test_json(Harness& h) {
    Json root = Json::make_object();
    root.set("name", Json::make_string(u8"سلام \"quoted\" \\ back"));
    root.set("n", Json::make_number(42));
    root.set("neg", Json::make_number(-3.5));
    root.set("flag", Json::make_bool(true));
    Json arr = Json::make_array();
    arr.push_back(Json::make_string(u8"یک"));
    arr.push_back(Json::make_number(2));
    root.set("list", arr);

    const std::string text = root.dump(true);
    Json parsed;
    std::string err;
    h.check(Json::parse(text, parsed, err), "JSON round trip parses (" + err + ")");
    h.check(parsed.get_string("name") == u8"سلام \"quoted\" \\ back", "JSON string escaping");
    h.check(std::fabs(parsed.get_number("n") - 42.0) < 1e-9, "JSON number");
    h.check(std::fabs(parsed.get_number("neg") + 3.5) < 1e-9, "JSON negative number");
    h.check(parsed.get_bool("flag"), "JSON bool");
    h.check(parsed.find("list") && parsed.find("list")->size() == 2, "JSON array length");

    // \u escapes (what other tools write)
    Json esc;
    h.check(Json::parse("{\"x\":\"\\u0633\\u0644\\u0627\\u0645\"}", esc, err), "JSON \\u escapes parse");
    h.check(esc.get_string("x") == u8"سلام", "JSON \\u escapes decode to Persian");
    Json pair;
    h.check(Json::parse("{\"x\":\"\\ud83d\\ude00\"}", pair, err), "JSON surrogate pair escapes");
    h.check(pair.get_string("x") == "\xF0\x9F\x98\x80", "JSON surrogate pair decodes to UTF-8");

    Json bad;
    h.check(!Json::parse("{\"x\": }", bad, err), "malformed JSON rejected");
    h.check(!Json::parse("{} extra", bad, err), "trailing garbage rejected");
    h.check(!Json::parse("", bad, err), "empty input rejected");
    h.check(!Json::parse("[1,2", bad, err), "unterminated array rejected");

    std::string deep;
    for (int i = 0; i < 60; ++i) deep += "[";
    h.check(!Json::parse(deep, bad, err), "too deep nesting rejected");

    // Missing keys return defaults instead of crashing.
    Json obj = Json::make_object();
    h.check(obj.get_string("nope", "dflt") == "dflt", "missing key default (string)");
    h.check(obj.get_number("nope", 7) == 7, "missing key default (number)");
    h.check(!obj.get_bool("nope", false), "missing key default (bool)");
}

void test_store(Harness& h) {
    ClipboardStore s;
    h.check(s.size() == 0, "store starts empty");

    h.check(s.add(u"first", 100) == ClipboardStore::AddResult::kAdded, "add first entry");
    h.check(s.at(0).text == u"first", "newest entry is index 0");

    h.check(s.add(u"second", 200) == ClipboardStore::AddResult::kAdded, "add second entry");
    h.check(s.at(0).text == u"second", "newest first after second add");
    h.check(s.at(1).text == u"first", "older entry pushed down");

    h.check(s.add(u"first", 300) == ClipboardStore::AddResult::kMovedToTop, "duplicate detected");
    h.check(s.size() == 2, "duplicate is not stored twice");
    h.check(s.at(0).text == u"first", "duplicate moved to top");

    h.check(s.add(u"   ", 400) == ClipboardStore::AddResult::kRejectedEmpty, "blank rejected");
    h.check(s.add(std::u16string(ClipboardStore::kMaxChars + 1, u'x'), 500) ==
                ClipboardStore::AddResult::kRejectedTooBig,
            "oversized entry rejected");
    h.check(s.size() == 2, "rejected entries are not stored");

    for (int i = 0; i < 120; ++i) {
        s.add(u16_from_ascii(("entry " + std::to_string(i)).c_str()), 600 + i);
    }
    h.check(s.size() == ClipboardStore::kMaxEntries, "history capped at 100 entries");
    h.check(s.at(0).text == u"entry 119", "newest is first after many adds");
    h.check(s.at(99).text == u"entry 20", "oldest entries dropped");

    ClipboardStore fa;
    fa.add(u"\u062a\u0633\u062a \u06cc\u06a9", 10);       // تست یک
    fa.add(u"\u062a\u0633\u062a \u062f\u0648", 20);       // تست دو
    fa.add(u"Hello World", 30);
    h.check(fa.search(u"\u06cc\u06a9").size() == 1, "Persian search finds one entry");
    h.check(fa.search(u"hello").size() == 1, "case-insensitive search");
    h.check(fa.search(u"").size() == 3, "empty search returns everything");
    h.check(fa.search(u"nothing-here").empty(), "search with no match");
    h.check(fa.search(u"\u062a\u0633\u062a").size() == 2, "Persian prefix search");

    h.check(fa.remove_at(0), "delete first entry");
    h.check(fa.size() == 2, "size after delete");
    h.check(!fa.remove_at(99), "delete out of range fails");

    // Persistence
    ClipboardStore p;
    p.add(u"line1\nline2 \"quoted\" \\ \u0633\u0644\u0627\u0645", 1700000000);
    p.add(u"emoji \U0001F600", 1700000001);
    const std::string json = p.to_json();
    ClipboardStore q;
    std::string err;
    h.check(q.from_json(json, err), "store JSON parses (" + err + ")");
    h.check(q.size() == 2, "store JSON keeps entries");
    h.check(q.at(0).text == p.at(0).text, "store JSON preserves newest entry");
    h.check(q.at(1).text == p.at(1).text, "store JSON preserves multi-line Persian text");
    h.check(q.at(1).ts == 1700000000, "store JSON preserves timestamps");

    ClipboardStore bad;
    h.check(!bad.from_json("{not json", err), "corrupt history rejected");
    h.check(bad.size() == 0, "corrupt history leaves an empty store");
    h.check(!bad.from_json("{\"entries\": 5}", err), "wrong history shape rejected");

    ClipboardStore half;
    h.check(half.from_json("{\"entries\":[{\"x\":\"ok\"},{\"nope\":1},{\"x\":\"\"}]}", err),
            "history tolerates partial entries");
    h.check(half.size() == 1, "invalid entries skipped");

    ClipboardStore empty_store;
    h.check(empty_store.from_json(empty_store.to_json(), err), "empty store round trip");
    h.check(empty_store.size() == 0, "empty store stays empty");
}

void test_settings(Harness& h) {
    Settings s = default_settings();
    std::string err;
    Settings t;
    h.check(settings_from_json(settings_to_json(s), t, err), "settings round trip parses");
    h.check(t.clipboard_hotkey == s.clipboard_hotkey, "settings round trip keeps hotkey");
    h.check(t.ui_language == s.ui_language, "settings round trip keeps language");

    Settings partial;
    h.check(settings_from_json("{\"wizard_done\": true}", partial, err), "partial settings parse");
    h.check(partial.wizard_done, "partial settings apply present keys");
    h.check(partial.clipboard_hotkey == "auto", "partial settings keep defaults");

    Settings bad;
    h.check(!settings_from_json("{", bad, err), "broken settings rejected");
    h.check(bad.wizard_done == false, "broken settings fall back to defaults");

    Settings junk;
    settings_from_json("{\"ui_language\": 99, \"clipboard_hotkey\": \"nonsense\"}", junk, err);
    h.check(junk.ui_language >= 0 && junk.ui_language <= 1, "language index clamped");
    h.check(junk.clipboard_hotkey == "auto", "invalid hotkey reset to auto");

    Settings auto_ok;
    auto_ok.clipboard_hotkey = "Win+V";
    settings_from_json(settings_to_json(auto_ok), junk, err);
    h.check(junk.clipboard_hotkey == "Win+V", "Win+V stored as given");
}

void test_hotkeys(Harness& h) {
    HotkeySpec spec;
    std::string err;
    h.check(parse_hotkey("Ctrl+Alt+V", spec, err) && spec.ctrl && spec.alt && spec.key == 'V',
            "parse Ctrl+Alt+V");
    h.check(format_hotkey(spec) == "Ctrl+Alt+V", "format Ctrl+Alt+V");

    h.check(parse_hotkey("win+v", spec, err) && spec.win && spec.key == 'V', "parse win+v (lower)");
    h.check(format_hotkey(spec) == "Win+V", "format Win+V");
    h.check(is_win_v(spec), "Win+V recognised");

    h.check(parse_hotkey(" Ctrl + Shift + K ", spec, err) && spec.ctrl && spec.shift && spec.key == 'K',
            "parse with spaces");
    h.check(format_hotkey(spec) == "Ctrl+Shift+K", "format Ctrl+Shift+K");

    h.check(!parse_hotkey("V", spec, err), "bare key rejected");
    h.check(!parse_hotkey("Ctrl+", spec, err), "trailing plus rejected");
    h.check(!parse_hotkey("Ctrl+Alt+VV", spec, err), "multi-char key rejected");
    h.check(!parse_hotkey("Hyper+Alt+V", spec, err), "unknown modifier rejected");
    h.check(!parse_hotkey("Ctrl+Ctrl+V", spec, err), "duplicate modifier rejected");
    h.check(!parse_hotkey("", spec, err), "empty shortcut rejected");

    const std::vector<std::string> cands = auto_clipboard_candidates();
    h.check(!cands.empty() && cands[0] == "Win+V", "first auto candidate is Win+V");
    bool all_valid = true;
    for (const std::string& c : cands) {
        HotkeySpec cs;
        std::string e;
        if (!parse_hotkey(c, cs, e)) all_valid = false;
    }
    h.check(all_valid, "every auto candidate is a valid shortcut");

    bool selectable_ok = true;
    for (const std::string& c : selectable_clipboard_hotkeys()) {
        if (c == "auto" || c == "none") continue;
        HotkeySpec cs;
        std::string e;
        if (!parse_hotkey(c, cs, e)) selectable_ok = false;
    }
    h.check(selectable_ok, "every selectable shortcut is valid");

    h.check(hotkey_failure_text("conflict", false).find("already owned") != std::string::npos,
            "conflict explanation (English)");
    h.check(hotkey_failure_text("conflict", true).size() > 10, "conflict explanation (Persian)");
}

void test_layouts(Harness& h) {
    for (int li = 0; li < layout_count(); ++li) {
        const LayoutDef& layout = layout_by_index(li);
        const std::string name = ascii_from_u16(layout.code);
        for (int p = 0; p < layout.page_count; ++p) {
            const PageDef& page = layout.pages[p];
            for (int r = 0; r < page.row_count; ++r) {
                const RowDef& row = page.rows[r];
                float units = 0.0f;
                for (int k = 0; k < row.count; ++k) units += row.keys[k].units;
                char msg[160];
                std::snprintf(msg, sizeof(msg), "%s page %d row %d sums to %.2f units (expected %.0f)",
                              name.c_str(), p, r, units, kRowUnits);
                h.check(std::fabs(units - kRowUnits) < 0.001f, msg);
            }
        }
        // Geometry across a range of widths: no overlap, exact span.
        for (int width = 320; width <= 1600; width += 61) {
            for (int p = 0; p < layout.page_count; ++p) {
                const PageDef& page = layout.pages[p];
                for (int r = 0; r < page.row_count; ++r) {
                    std::vector<KeyPlacement> place;
                    compute_row(page.rows[r], width, 6, 0, 40, &place);
                    bool ok = place.size() == static_cast<size_t>(page.rows[r].count);
                    int prev_right = -1;
                    for (size_t i = 0; i < place.size() && ok; ++i) {
                        if (place[i].w < 1) ok = false;
                        if (place[i].x < prev_right) ok = false;
                        prev_right = place[i].x + place[i].w;
                    }
                    if (ok && !place.empty()) {
                        ok = place.front().x == 0 && (place.back().x + place.back().w) == width;
                    }
                    char msg[200];
                    std::snprintf(msg, sizeof(msg), "%s page %d row %d lays out at width %d",
                                  name.c_str(), p, r, width);
                    h.check(ok, msg);
                }
            }
        }
    }

    const std::u16string fa_typed = typed_characters(kLayoutPersian);
    for (const char16_t* letter : kPersianLetters) {
        h.check(contains_cp(fa_typed, letter[0]), "Persian layout can type every letter");
    }
    const std::u16string en_typed = typed_characters(kLayoutEnglish);
    for (char16_t c = u'a'; c <= u'z'; ++c) {
        h.check(contains_cp(en_typed, c), "English layout can type letter " + ascii_from_u16(std::u16string(1, c)));
    }
    for (char16_t c = u'0'; c <= u'9'; ++c) {
        h.check(contains_cp(en_typed, c), "English layout can type digit " + ascii_from_u16(std::u16string(1, c)));
        h.check(contains_cp(fa_typed, static_cast<char16_t>(0x06F0 + (c - u'0'))),
                "Persian layout can type Persian digit");
    }
    // Essential keys / punctuation
    const char16_t* must_have[] = {u" ", u".", u",", u"/", u"?", u"!", u"@", u"#", u"-", u"=",
                                   u"(", u")", u"[", u"]", u"'", u"\u061f", u"\u060c"};
    for (const char16_t* ch : must_have) {
        h.check(fa_typed.find(ch[0]) != std::u16string::npos ||
                    en_typed.find(ch[0]) != std::u16string::npos,
                "layouts provide punctuation: " + ascii_from_u16(std::u16string(1, ch[0])));
    }
    h.check(fa_typed.find(0x200C) != std::u16string::npos, "Persian layout types ZWNJ (نیم‌فاصله)");
    h.check(kLayoutPersian.rtl && !kLayoutEnglish.rtl, "layout RTL flags");

    // Special keys exist exactly once per letters page.
    for (int li = 0; li < layout_count(); ++li) {
        const LayoutDef& layout = layout_by_index(li);
        const PageDef& page = layout.pages[0];
        int space = 0, backspace = 0, enter = 0, shift = 0, lang = 0, symbols = 0, tab = 0;
        for (int r = 0; r < page.row_count; ++r) {
            for (int k = 0; k < page.rows[r].count; ++k) {
                switch (page.rows[r].keys[k].action) {
                    case KeyAction::Space: ++space; break;
                    case KeyAction::Backspace: ++backspace; break;
                    case KeyAction::Enter: ++enter; break;
                    case KeyAction::Shift: ++shift; break;
                    case KeyAction::Language: ++lang; break;
                    case KeyAction::Symbols: ++symbols; break;
                    case KeyAction::Tab: ++tab; break;
                    default: break;
                }
            }
        }
        const std::string n = ascii_from_u16(layout.code);
        h.check(space == 1 && backspace == 1 && enter == 1 && shift == 2 && lang == 1 &&
                    symbols == 1 && tab == 1,
                n + " layout has the expected special keys");
        // Virtual keys carry their VK code.
        bool vk_ok = true;
        for (int r = 0; r < page.row_count; ++r) {
            for (int k = 0; k < page.rows[r].count; ++k) {
                const KeyDef& key = page.rows[r].keys[k];
                if (key.action == KeyAction::Backspace && key.vk != 0x08) vk_ok = false;
                if (key.action == KeyAction::Enter && key.vk != 0x0D) vk_ok = false;
                if (key.action == KeyAction::Tab && key.vk != 0x09) vk_ok = false;
                if (key.action == KeyAction::Character && key.text == nullptr) vk_ok = false;
            }
        }
        h.check(vk_ok, n + " layout virtual keys carry VK codes");
    }
}

}  // namespace

TestSummary run_core_selftests() {
    Harness h;
    test_text(h);
    test_json(h);
    test_store(h);
    test_settings(h);
    test_hotkeys(h);
    test_layouts(h);
    TestSummary s = h.summary();
    s.log += "\n";
    char buf[128];
    std::snprintf(buf, sizeof(buf), "core tests: %d passed, %d failed\n", s.passed, s.failed);
    s.log += buf;
    return s;
}

}  // namespace core
