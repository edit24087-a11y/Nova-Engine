// Nova Engine — minimal self-contained JSON implementation (C++17).
// Supports: null/bool/number/string/array/object, serialization with escaping,
// parsing with error reporting. Used for project.json and scene serialization.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace nova::json {

class JsonValue;
using Array = std::vector<JsonValue>;
using Object = std::map<std::string, JsonValue>;

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() : type_(Type::Null) {}
    JsonValue(std::nullptr_t) : type_(Type::Null) {}
    JsonValue(bool b) : type_(Type::Bool), bool_(b) {}
    JsonValue(int v) : type_(Type::Number), num_((double)v) {}
    JsonValue(int64_t v) : type_(Type::Number), num_((double)v) {}
    JsonValue(double v) : type_(Type::Number), num_(v) {}
    JsonValue(float v) : type_(Type::Number), num_((double)v) {}
    JsonValue(const char* s) : type_(Type::String), str_(s) {}
    JsonValue(std::string s) : type_(Type::String), str_(std::move(s)) {}
    JsonValue(const Array& a) : type_(Type::Array), arr_(a) {}
    JsonValue(const Object& o) : type_(Type::Object), obj_(o) {}

    static JsonValue array() { return JsonValue(Array{}); }
    static JsonValue object() { return JsonValue(Object{}); }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool(bool def = false) const { return isBool() ? bool_ : def; }
    double asNumber(double def = 0.0) const { return isNumber() ? num_ : def; }
    float asFloat(float def = 0.0f) const { return isNumber() ? (float)num_ : def; }
    int asInt(int def = 0) const { return isNumber() ? (int)num_ : def; }
    const std::string& asString(const std::string& def = std::string()) const { return isString() ? str_ : def; }
    const Array& asArray() const { return arr_; }
    const Object& asObject() const { return obj_; }
    Array& asArray() { return arr_; }
    Object& asObject() { return obj_; }

    // Object helpers (return default when key missing).
    bool has(const std::string& k) const { return isObject() && obj_.count(k) > 0; }
    const JsonValue& get(const std::string& k) const {
        static JsonValue nullv;
        if (!isObject()) return nullv;
        auto it = obj_.find(k);
        return it != obj_.end() ? it->second : nullv;
    }
    const JsonValue& operator[](const std::string& k) const { return get(k); }

    // Mutators.
    void set(const std::string& k, JsonValue v) { if (isObject()) obj_[k] = std::move(v); }
    void push(JsonValue v) { if (isArray()) arr_.push_back(std::move(v)); }
    Array& items() { return arr_; }

    // Serialization.
    std::string dump(int indent = -1) const;
    std::string dumpCompact() const { return dump(-1); }

    // Parsing. Throws std::runtime_error on malformed input.
    static JsonValue parse(const std::string& text);
    static JsonValue parseFile(const std::string& path);

private:
    Type type_;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    Array arr_;
    Object obj_;
};

} // namespace nova::json
