// Randomized robustness test for the parsers and text helpers.
// Build & run:  g++ -std=c++17 -g -fsanitize=address,undefined -o /tmp/fuzz tests/fuzz_core.cpp src/core/*.cpp && /tmp/fuzz
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

#include "../src/core/json.h"
#include "../src/core/layout.h"
#include "../src/core/settings.h"
#include "../src/core/store.h"
#include "../src/core/text.h"

using namespace core;

int main() {
    std::mt19937 rng(0xC0FFEE);
    int iterations = 0;
    int parse_ok = 0;

    const char* alphabet = "{}[]\",:0-9.eEtrufalsn \t\n\r\\/u\u00e2\u0080\u0083\xd8\xb3";
    for (int iter = 0; iter < 20000; ++iter) {
        // 1. Random JSON-ish garbage.
        std::string s;
        const int len = static_cast<int>(rng() % 80);
        for (int i = 0; i < len; ++i) {
            if (rng() % 3 == 0) {
                s.push_back(static_cast<char>(rng() & 0xFF));  // raw bytes incl. invalid UTF-8
            } else {
                s.push_back(alphabet[rng() % std::strlen(alphabet)]);
            }
        }
        Json j;
        std::string err;
        if (Json::parse(s, j, err)) {
            ++parse_ok;
            (void)j.dump(true);            // must not crash on anything we accepted
            const Json* e = j.find("entries");
            if (e && e->is_array()) {
                for (size_t k = 0; k < e->size() && k < 5; ++k) {
                    (void)e->at(k).get_string("x");
                }
            }
            Settings st;
            std::string serr;
            (void)settings_from_json(s, st, serr);
            ClipboardStore store;
            (void)store.from_json(s, serr);
        }
        // 2. Random byte strings through the UTF-8 decoder and back.
        std::u16string u = utf8_to_u16(s);
        (void)u16_to_utf8(u);
        (void)has_rtl(u);
        (void)make_preview(u, static_cast<size_t>(rng() % 200));
        (void)normalize_newlines(u);
        (void)contains_ci(u, u"\u0633\u0644\u0627\u0645");
        // 3. Random geometry.
        for (int li = 0; li < layout_count(); ++li) {
            const LayoutDef& layout = layout_by_index(li);
            for (int p = 0; p < layout.page_count; ++p) {
                const PageDef& page = layout.pages[p];
                for (int r = 0; r < page.row_count; ++r) {
                    std::vector<KeyPlacement> out;
                    compute_row(page.rows[r], static_cast<int>(rng() % 2000) - 50,
                                static_cast<int>(rng() % 20) - 5, 0,
                                static_cast<int>(rng() % 100), &out);
                }
            }
        }
        ++iterations;
    }
    // ---- Mutation fuzzing of a valid document: reaches the deep parser paths.
    {
        Json root = Json::make_object();
        root.set("app", Json::make_string("AMIN Keyboard & Clipboard"));
        root.set("version", Json::make_number(1));
        root.set("wizard_done", Json::make_bool(true));
        root.set("clipboard_hotkey", Json::make_string("Win+V"));
        Json arr = Json::make_array();
        for (int i = 0; i < 6; ++i) {
            Json item = Json::make_object();
            item.set("t", Json::make_number(1700000000 + i));
            item.set("x", Json::make_string(u8"خط \"نقل قول\" \\ slash\nnewline 😀"));
            arr.push_back(item);
        }
        root.set("entries", arr);
        const std::string seed = root.dump(true);
        int mutated_ok = 0;
        for (int iter = 0; iter < 20000; ++iter) {
            std::string doc = seed;
            const int edits = 1 + static_cast<int>(rng() % 4);
            for (int e = 0; e < edits; ++e) {
                if (doc.empty()) break;
                const size_t pos = rng() % doc.size();
                switch (rng() % 4) {
                    case 0: doc.erase(pos, 1 + rng() % 3); break;
                    case 1: doc.insert(pos, 1, static_cast<char>(rng() & 0xFF)); break;
                    case 2: doc[pos] = static_cast<char>(rng() & 0xFF); break;
                    case 3: doc.insert(pos, doc.substr(0, std::min<size_t>(doc.size(), 8))); break;
                }
            }
            Json parsed;
            std::string err;
            if (Json::parse(doc, parsed, err)) {
                ++mutated_ok;
                (void)parsed.dump(true);
                (void)parsed.dump(false);
                ClipboardStore st;
                std::string serr;
                if (st.from_json(doc, serr)) {
                    (void)st.to_json();
                    (void)st.search(u"خط");
                }
                Settings settings;
                (void)settings_from_json(doc, settings, serr);
                (void)settings_to_json(settings);
            }
        }
        std::printf("fuzz (mutated valid docs): %d accepted of 20000, no crash\n", mutated_ok);
    }

    // ---- Random UTF-16 through the encoders (lone surrogates, NULs, ...).
    {
        for (int iter = 0; iter < 20000; ++iter) {
            std::u16string u;
            const int len = static_cast<int>(rng() % 40);
            for (int i = 0; i < len; ++i) {
                const uint32_t r = rng() % 5;
                if (r == 0) u.push_back(static_cast<char16_t>(0xD800 + (rng() % 0x400)));
                else if (r == 1) u.push_back(static_cast<char16_t>(0xDC00 + (rng() % 0x400)));
                else if (r == 2) u.push_back(0);
                else u.push_back(static_cast<char16_t>(rng() & 0xFFFF));
            }
            const std::string utf8 = u16_to_utf8(u);
            (void)utf8_to_u16(utf8);
            (void)has_rtl(u);
            (void)is_blank(u);
            (void)make_preview(u, 12);
            (void)format_relative_time(1700000000, 1600000000, true);
        }
        std::printf("fuzz (random UTF-16 incl. lone surrogates): 20000 cases, no crash\n");
    }

    std::printf("fuzz: %d iterations, %d accepted as valid JSON, no crash\n", iterations, parse_ok);
    return 0;
}
