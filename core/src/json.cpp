// Nova Engine — core JSON implementation.
#include "nova/json.hpp"
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <functional>
#include "nova/util/fs.hpp"

namespace nova::json {

static void escapeToString(const std::string& s, std::string& out) {
    out.push_back('"');
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if ((unsigned char)c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                out += buf;
            } else {
                out.push_back(c);
            }
        }
    }
    out.push_back('"');
}

std::string JsonValue::dump(int indent) const {
    std::string out;
    std::function<void(const JsonValue&, int)> write = [&](const JsonValue& v, int level) {
        switch (v.type()) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += v.asBool() ? "true" : "false"; break;
        case Type::Number: {
            std::ostringstream os;
            os.precision(10);
            os << v.asNumber();
            out += os.str();
            break;
        }
        case Type::String: escapeToString(v.asString(), out); break;
        case Type::Array: {
            if (v.asArray().empty()) { out += "[]"; return; }
            out.push_back('[');
            for (size_t i = 0; i < v.asArray().size(); ++i) {
                if (indent >= 0) for (int k = 0; k < indent * (level + 1); ++k) out.push_back(' ');
                write(v.asArray()[i], level + 1);
                if (i + 1 < v.asArray().size()) out.push_back(',');
            }
            if (indent >= 0) for (int k = 0; k < indent * level; ++k) out.push_back(' ');
            out.push_back(']');
            break;
        }
        case Type::Object: {
            if (v.asObject().empty()) { out += "{}"; return; }
            out.push_back('{');
            bool first = true;
            for (const auto& kv : v.asObject()) {
                if (!first) out.push_back(',');
                first = false;
                if (indent >= 0) for (int k = 0; k < indent * (level + 1); ++k) out.push_back(' ');
                escapeToString(kv.first, out);
                out.push_back(':');
                if (indent >= 0) out.push_back(' ');
                write(kv.second, level + 1);
            }
            if (indent >= 0) for (int k = 0; k < indent * level; ++k) out.push_back(' ');
            out.push_back('}');
            break;
        }
        }
    };
    write(*this, 0);
    return out;
}

namespace {

struct Parser {
    const std::string& s;
    size_t i = 0;
    std::string err;

    explicit Parser(const std::string& text) : s(text) {}

    [[noreturn]] void fail(const std::string& m) {
        throw std::runtime_error(m + " at position " + std::to_string(i));
    }

    void skipWs() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i;
            else break;
        }
    }

    char peek() { skipWs(); return i < s.size() ? s[i] : '\0'; }
    char next() { skipWs(); if (i >= s.size()) fail("unexpected end"); return s[i++]; }

    JsonValue parseValue() {
        char c = peek();
        switch (c) {
        case '{': return parseObject();
        case '[': return parseArray();
        case '"': return JsonValue(parseString());
        case 't':
            if (s.compare(i, 4, "true") == 0) { i += 4; return JsonValue(true); }
            fail("invalid literal");
        case 'f':
            if (s.compare(i, 5, "false") == 0) { i += 5; return JsonValue(false); }
            fail("invalid literal");
        case 'n':
            if (s.compare(i, 4, "null") == 0) { i += 4; return JsonValue(); }
            fail("invalid literal");
        default:
            if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
            fail("unexpected character");
        }
    }

    JsonValue parseNumber() {
        size_t start = i;
        if (peek() == '-') ++i;
        while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' ||
               s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) {
            if ((s[i] == '+' || s[i] == '-') && !(s[i - 1] == 'e' || s[i - 1] == 'E')) break;
            ++i;
        }
        std::string num = s.substr(start, i - start);
        if (num.empty()) fail("invalid number");
        return JsonValue(std::stod(num));
    }

    std::string parseString() {
        next(); // opening quote
        std::string out;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return out;
            if (c == '\\') {
                if (i >= s.size()) fail("bad escape");
                char e = s[i++];
                switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'u': {
                    if (i + 4 > s.size()) fail("bad unicode escape");
                    unsigned code = std::stoul(s.substr(i, 4), nullptr, 16);
                    i += 4;
                    // Encode as UTF-8 (BMP only; surrogate pairs pass through).
                    if (code < 0x80) out.push_back((char)code);
                    else if (code < 0x800) {
                        out.push_back((char)(0xC0 | (code >> 6)));
                        out.push_back((char)(0x80 | (code & 0x3F)));
                    } else {
                        out.push_back((char)(0xE0 | (code >> 12)));
                        out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
                        out.push_back((char)(0x80 | (code & 0x3F)));
                    }
                    break;
                }
                default: fail("unknown escape");
                }
            } else {
                out.push_back(c);
            }
        }
        fail("unterminated string");
    }

    JsonValue parseArray() {
        next(); // [
        Array arr;
        while (peek() != ']') {
            if (i >= s.size()) fail("unterminated array");
            arr.push_back(parseValue());
            if (peek() == ',') { next(); continue; }
            break;
        }
        if (next() != ']') fail("expected ]");
        return JsonValue(arr);
    }

    JsonValue parseObject() {
        next(); // {
        Object obj;
        while (peek() != '}') {
            if (i >= s.size()) fail("unterminated object");
            std::string key = parseString();
            if (next() != ':') fail("expected :");
            obj[key] = parseValue();
            if (peek() == ',') { next(); continue; }
            break;
        }
        if (next() != '}') fail("expected }");
        return JsonValue(obj);
    }
};

} // namespace

JsonValue JsonValue::parse(const std::string& text) {
    Parser p(text);
    JsonValue v = p.parseValue();
    p.skipWs();
    return v;
}

JsonValue JsonValue::parseFile(const std::string& path) {
    std::string content;
    if (!fs::readFile(path, content)) {
        throw std::runtime_error("cannot read file: " + path);
    }
    return parse(content);
}

} // namespace nova::json
