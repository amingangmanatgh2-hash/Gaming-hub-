#include "json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace core {

namespace {
const int kMaxDepth = 32;
}

Json Json::make_object() {
    Json j;
    j.type_ = Type::Object;
    return j;
}
Json Json::make_array() {
    Json j;
    j.type_ = Type::Array;
    return j;
}
Json Json::make_string(const std::string& utf8) {
    Json j;
    j.type_ = Type::String;
    j.str_ = utf8;
    return j;
}
Json Json::make_number(double v) {
    Json j;
    j.type_ = Type::Number;
    j.num_ = v;
    return j;
}
Json Json::make_bool(bool v) {
    Json j;
    j.type_ = Type::Bool;
    j.bool_ = v;
    return j;
}

void Json::set(const std::string& key, const Json& value) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        obj_.clear();
    }
    for (auto& kv : obj_) {
        if (kv.first == key) {
            kv.second = value;
            return;
        }
    }
    obj_.emplace_back(key, value);
}

const Json* Json::find(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& kv : obj_) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

std::string Json::get_string(const std::string& key, const std::string& def) const {
    const Json* v = find(key);
    return (v && v->type_ == Type::String) ? v->str_ : def;
}
double Json::get_number(const std::string& key, double def) const {
    const Json* v = find(key);
    return (v && v->type_ == Type::Number) ? v->num_ : def;
}
bool Json::get_bool(const std::string& key, bool def) const {
    const Json* v = find(key);
    return (v && v->type_ == Type::Bool) ? v->bool_ : def;
}

void Json::push_back(const Json& value) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        arr_.clear();
    }
    arr_.push_back(value);
}

size_t Json::size() const {
    if (type_ == Type::Array) return arr_.size();
    if (type_ == Type::Object) return obj_.size();
    if (type_ == Type::String) return str_.size();
    return 0;
}

const Json& Json::at(size_t i) const {
    static const Json kNull;
    if (type_ != Type::Array || i >= arr_.size()) return kNull;
    return arr_[i];
}

// ---------------------------------------------------------------------------
//  Escaping
// ---------------------------------------------------------------------------
std::string json_escape(const std::string& utf8) {
    std::string out;
    out.reserve(utf8.size() + 8);
    for (unsigned char c : utf8) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    // UTF-8 bytes are copied verbatim; the file itself is UTF-8.
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

static void append_utf8_cp(std::string& out, uint32_t cp) {
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

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool json_unescape(const std::string& src, std::string& out, std::string& err) {
    out.clear();
    out.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        const char c = src[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (i + 1 >= src.size()) {
            err = "trailing backslash";
            return false;
        }
        const char e = src[++i];
        switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                if (i + 4 >= src.size()) {
                    err = "short \\u escape";
                    return false;
                }
                int h[4];
                for (int k = 0; k < 4; ++k) {
                    h[k] = hex_val(src[i + 1 + k]);
                    if (h[k] < 0) {
                        err = "bad hex in \\u escape";
                        return false;
                    }
                }
                uint32_t cp = static_cast<uint32_t>((h[0] << 12) | (h[1] << 8) | (h[2] << 4) | h[3]);
                i += 4;
                // Surrogate pair?
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < src.size() && src[i + 1] == '\\' &&
                    src[i + 2] == 'u') {
                    int g[4];
                    bool ok = true;
                    for (int k = 0; k < 4; ++k) {
                        g[k] = hex_val(src[i + 3 + k]);
                        if (g[k] < 0) ok = false;
                    }
                    if (ok) {
                        const uint32_t lo =
                            static_cast<uint32_t>((g[0] << 12) | (g[1] << 8) | (g[2] << 4) | g[3]);
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            i += 6;
                        }
                    }
                }
                append_utf8_cp(out, cp);
                break;
            }
            default:
                err = "unknown escape";
                return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
//  Writer
// ---------------------------------------------------------------------------
static std::string number_to_string(double v) {
    if (!std::isfinite(v)) return "0";
    if (v == std::floor(v) && std::fabs(v) < 1e15) {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%.0f", v);
        return buf;
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

void Json::dump_to(std::string& out, int indent, bool pretty) const {
    auto pad = [&](int n) {
        if (!pretty) return;
        out.push_back('\n');
        out.append(static_cast<size_t>(n) * 2, ' ');
    };
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Number: out += number_to_string(num_); break;
        case Type::String:
            out.push_back('"');
            out += json_escape(str_);
            out.push_back('"');
            break;
        case Type::Array: {
            out.push_back('[');
            for (size_t i = 0; i < arr_.size(); ++i) {
                if (i) out.push_back(',');
                pad(indent + 1);
                arr_[i].dump_to(out, indent + 1, pretty);
            }
            if (!arr_.empty()) pad(indent);
            out.push_back(']');
            break;
        }
        case Type::Object: {
            out.push_back('{');
            for (size_t i = 0; i < obj_.size(); ++i) {
                if (i) out.push_back(',');
                pad(indent + 1);
                out.push_back('"');
                out += json_escape(obj_[i].first);
                out += pretty ? "\": " : "\":";
                obj_[i].second.dump_to(out, indent + 1, pretty);
            }
            if (!obj_.empty()) pad(indent);
            out.push_back('}');
            break;
        }
    }
}

std::string Json::dump(bool pretty) const {
    std::string out;
    dump_to(out, 0, pretty);
    if (pretty) out.push_back('\n');
    return out;
}

// ---------------------------------------------------------------------------
//  Parser (recursive descent, bounded depth, tolerant of trailing whitespace)
// ---------------------------------------------------------------------------
namespace {

class Parser {
public:
    Parser(const std::string& t, std::string& err) : s_(t), i_(0), err_(err), depth_(0) {}

    bool parse_value(Json& out) {
        skip_ws();
        if (i_ >= s_.size()) return fail("unexpected end of input");
        const char c = s_[i_];
        switch (c) {
            case '{': return parse_object(out);
            case '[': return parse_array(out);
            case '"': {
                std::string raw, decoded;
                if (!parse_raw_string(raw)) return false;
                std::string e;
                if (!json_unescape(raw, decoded, e)) return fail(e);
                out = Json::make_string(decoded);
                return true;
            }
            case 't':
                if (s_.compare(i_, 4, "true") == 0) {
                    i_ += 4;
                    out = Json::make_bool(true);
                    return true;
                }
                return fail("invalid literal");
            case 'f':
                if (s_.compare(i_, 5, "false") == 0) {
                    i_ += 5;
                    out = Json::make_bool(false);
                    return true;
                }
                return fail("invalid literal");
            case 'n':
                if (s_.compare(i_, 4, "null") == 0) {
                    i_ += 4;
                    out = Json();
                    return true;
                }
                return fail("invalid literal");
            default:
                if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out);
                return fail("unexpected character");
        }
    }

    bool finished() {
        skip_ws();
        return i_ == s_.size();
    }

private:
    bool fail(const std::string& m) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s at offset %zu", m.c_str(), i_);
        err_ = buf;
        return false;
    }

    void skip_ws() {
        while (i_ < s_.size()) {
            const char c = s_[i_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++i_;
            } else {
                break;
            }
        }
    }

    bool parse_raw_string(std::string& raw) {
        if (i_ >= s_.size() || s_[i_] != '"') return fail("expected string");
        ++i_;
        raw.clear();
        while (i_ < s_.size()) {
            const char c = s_[i_];
            if (c == '"') {
                ++i_;
                return true;
            }
            if (c == '\\') {
                raw.push_back(c);
                ++i_;
                if (i_ >= s_.size()) return fail("unterminated escape");
                raw.push_back(s_[i_]);
                // Copy the 4 hex digits of a \u escape too.
                if (s_[i_] == 'u') {
                    for (int k = 0; k < 4; ++k) {
                        ++i_;
                        if (i_ >= s_.size()) return fail("unterminated \\u escape");
                        raw.push_back(s_[i_]);
                    }
                }
                ++i_;
                continue;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                // Be permissive: control characters are rare but legal users
                // of hand-edited files; keep them as-is.
                raw.push_back(c);
                ++i_;
                continue;
            }
            raw.push_back(c);
            ++i_;
        }
        return fail("unterminated string");
    }

    bool parse_number(Json& out) {
        const size_t start = i_;
        if (i_ < s_.size() && (s_[i_] == '-' || s_[i_] == '+')) ++i_;
        bool any = false;
        while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') {
            ++i_;
            any = true;
        }
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') {
                ++i_;
                any = true;
            }
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '-' || s_[i_] == '+')) ++i_;
            while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
        }
        if (!any) return fail("invalid number");
        const std::string num = s_.substr(start, i_ - start);
        out = Json::make_number(std::strtod(num.c_str(), nullptr));
        return true;
    }

    bool parse_array(Json& out) {
        if (++depth_ > kMaxDepth) return fail("nesting too deep");
        ++i_;  // '['
        out = Json::make_array();
        skip_ws();
        if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            --depth_;
            return true;
        }
        for (;;) {
            Json item;
            if (!parse_value(item)) return false;
            out.push_back(item);
            skip_ws();
            if (i_ >= s_.size()) return fail("unterminated array");
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') {
                ++i_;
                --depth_;
                return true;
            }
            return fail("expected ',' or ']'");
        }
    }

    bool parse_object(Json& out) {
        if (++depth_ > kMaxDepth) return fail("nesting too deep");
        ++i_;  // '{'
        out = Json::make_object();
        skip_ws();
        if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            --depth_;
            return true;
        }
        for (;;) {
            skip_ws();
            std::string raw, decoded;
            if (!parse_raw_string(raw)) return false;
            std::string e;
            if (!json_unescape(raw, decoded, e)) return fail(e);
            skip_ws();
            if (i_ >= s_.size() || s_[i_] != ':') return fail("expected ':'");
            ++i_;
            Json value;
            if (!parse_value(value)) return false;
            out.set(decoded, value);
            skip_ws();
            if (i_ >= s_.size()) return fail("unterminated object");
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') {
                ++i_;
                --depth_;
                return true;
            }
            return fail("expected ',' or '}'");
        }
    }

    const std::string& s_;
    size_t i_;
    std::string& err_;
    int depth_;
};

}  // namespace

bool Json::parse(const std::string& text, Json& out, std::string& err) {
    err.clear();
    // Tolerate a UTF-8 BOM written by Notepad.
    size_t start = 0;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        start = 3;
    }
    // Keep the substring alive for the duration of the parse; Parser stores a
    // reference to it.
    const std::string body = text.substr(start);
    Parser p(body, err);
    out = Json();
    if (!p.parse_value(out)) return false;
    if (!p.finished()) {
        err = "trailing characters after JSON value";
        return false;
    }
    return true;
}

}  // namespace core
