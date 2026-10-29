// =============================================================================
//  AMIN Keyboard & Clipboard - core/json.h
//  Tiny, allocation-conservative JSON reader/writer used for the local
//  settings and clipboard-history files. No third-party code, no network.
// =============================================================================
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace core {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() : type_(Type::Null), bool_(false), num_(0) {}

    static Json make_object();
    static Json make_array();
    static Json make_string(const std::string& utf8);
    static Json make_number(double v);
    static Json make_bool(bool v);

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_object() const { return type_ == Type::Object; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_string() const { return type_ == Type::String; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_bool() const { return type_ == Type::Bool; }

    // Object access -----------------------------------------------------------
    void set(const std::string& key, const Json& value);   // replaces existing key
    const Json* find(const std::string& key) const;

    // Typed getters with defaults; never throw.
    std::string get_string(const std::string& key, const std::string& def = std::string()) const;
    double get_number(const std::string& key, double def = 0.0) const;
    bool get_bool(const std::string& key, bool def = false) const;

    // Array access ------------------------------------------------------------
    void push_back(const Json& value);
    size_t size() const;
    const Json& at(size_t i) const;

    const std::string& string_value() const { return str_; }
    double number_value() const { return num_; }
    bool bool_value() const { return bool_; }

    const std::vector<std::pair<std::string, Json>>& members() const { return obj_; }
    const std::vector<Json>& items() const { return arr_; }

    // Serialization -----------------------------------------------------------
    // When pretty is true the output is human readable (2 space indent).
    std::string dump(bool pretty = true) const;

    // Parsing -----------------------------------------------------------------
    // Returns false and fills err on malformed input. Depth is limited.
    static bool parse(const std::string& text, Json& out, std::string& err);

private:
    void dump_to(std::string& out, int indent, bool pretty) const;

    Type type_;
    bool bool_;
    double num_;
    std::string str_;
    std::vector<Json> arr_;
    std::vector<std::pair<std::string, Json>> obj_;
};

// Escapes/unescapes a UTF-8 string for use inside JSON quotes.
std::string json_escape(const std::string& utf8);
bool json_unescape(const std::string& escaped, std::string& out, std::string& err);

}  // namespace core
