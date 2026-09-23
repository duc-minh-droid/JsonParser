#pragma once
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "JsonValue.h"
#include "JsonNumber.h"
#include "JsonString.h"
#include "JsonNull.h"
#include "JsonBool.h"
#include "JsonArray.h"
#include "JsonObject.h"

// Thrown by JsonParser::parse() when the input is not valid JSON (RFC 8259).
// line and column are 1-based; column counts characters (UTF-8 aware), not bytes.
class JsonParseError : public std::runtime_error {
public:
    JsonParseError(const std::string& msg, size_t offset, int line, int column)
        : std::runtime_error("line " + std::to_string(line) + ", col " + std::to_string(column) + ": " + msg),
          message(msg), offset(offset), line(line), column(column) {}

    std::string message;  // message without the position prefix
    size_t offset;        // byte offset into the input
    int line;
    int column;
};

// Escapes a string for embedding inside a JSON string literal.
inline std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20 || c == 0x7f) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

// Optional recorder for the parser's internal steps. When attached with
// JsonParser::setTrace(), every token, grammar-rule entry/exit and node
// creation is appended as one JSON object. toJson() produces the document
// that the web visualizer replays.
class JsonTrace {
public:
    std::vector<std::string> events;

    std::string toJson(const std::string& input, const JsonParseError* error) const {
        std::string out = "{\n  \"input\": \"" + jsonEscape(input) + "\",\n";
        out += "  \"ok\": " + std::string(error ? "false" : "true") + ",\n";
        if (error) {
            out += "  \"error\": {\"message\": \"" + jsonEscape(error->message) + "\", \"pos\": " +
                   std::to_string(error->offset) + ", \"line\": " + std::to_string(error->line) +
                   ", \"col\": " + std::to_string(error->column) + "},\n";
        } else {
            out += "  \"error\": null,\n";
        }
        out += "  \"events\": [\n";
        for (size_t i = 0; i < events.size(); i++) {
            out += "    " + events[i];
            out += (i + 1 < events.size()) ? ",\n" : "\n";
        }
        out += "  ]\n}\n";
        return out;
    }
};

class JsonParser {
private:
    std::string json;
    size_t pos = 0;  // Current position
    int depth = 0;   // Current array/object nesting depth

    static constexpr int kMaxDepth = 512;

    // Tracing state (unused unless a JsonTrace is attached).
    JsonTrace* trace = nullptr;
    int nextNodeId = 0;
    struct Container { int id; bool isObject; std::string key; int index; };
    std::vector<Container> containers;
    size_t lcPos = 0;   // cached position for incremental line/col lookup
    int lcLine = 1, lcCol = 1;

public:
    JsonParser(const std::string& str) : json(str) {}

    void setTrace(JsonTrace* t) { trace = t; }

    // Parses the whole input as a single JSON document.
    // Throws JsonParseError (with line:col) on invalid input.
    std::unique_ptr<JsonValue> parse() {
        pos = 0;
        depth = 0;
        nextNodeId = 0;
        containers.clear();
        Rule rule(this, "document");

        skipWhitespace();
        auto value = parseValue();
        skipWhitespace();
        if (pos < json.size()) {
            fail("unexpected " + describe(pos) + " after the top-level value");
        }
        token("EOF", pos, pos);
        return value;
    }

    // Line/column (1-based) of a byte offset. Continuation bytes of a UTF-8
    // sequence do not advance the column.
    void lineCol(size_t offset, int& line, int& col) {
        if (offset < lcPos) { lcPos = 0; lcLine = 1; lcCol = 1; }
        for (; lcPos < offset && lcPos < json.size(); lcPos++) {
            unsigned char c = static_cast<unsigned char>(json[lcPos]);
            if (c == '\n') { lcLine++; lcCol = 1; }
            else if ((c & 0xC0) != 0x80) { lcCol++; }
        }
        line = lcLine;
        col = lcCol;
    }

private:
    // ---- tracing helpers ------------------------------------------------

    // RAII marker for a grammar rule: emits "enter" now and "exit" on scope
    // end. While an exception unwinds, no exit is emitted, so the last trace
    // state still shows the full rule stack at the point of failure.
    struct Rule {
        JsonParser* p;
        const char* name;
        int uncaught;
        Rule(JsonParser* parser, const char* n) : p(parser), name(n), uncaught(std::uncaught_exceptions()) {
            if (p->trace) {
                p->trace->events.push_back("{\"ev\":\"enter\",\"rule\":\"" + std::string(name) +
                                           "\",\"pos\":" + std::to_string(p->pos) + "}");
            }
        }
        ~Rule() {
            if (p->trace && std::uncaught_exceptions() == uncaught) {
                p->trace->events.push_back("{\"ev\":\"exit\",\"rule\":\"" + std::string(name) +
                                           "\",\"pos\":" + std::to_string(p->pos) + "}");
            }
        }
    };

    void token(const char* kind, size_t start, size_t end) {
        if (!trace) return;
        int line, col;
        lineCol(start, line, col);
        std::string text = json.substr(start, end - start);
        if (text.size() > 48) text = text.substr(0, 45) + "...";
        trace->events.push_back("{\"ev\":\"token\",\"tok\":\"" + std::string(kind) + "\",\"text\":\"" +
                                jsonEscape(text) + "\",\"start\":" + std::to_string(start) +
                                ",\"end\":" + std::to_string(end) + ",\"line\":" + std::to_string(line) +
                                ",\"col\":" + std::to_string(col) + "}");
    }

    // Records creation of a JsonValue subclass and where it hangs in the tree.
    int node(const char* cls, const std::string& preview, size_t start) {
        if (!trace) return -1;
        int id = nextNodeId++;
        std::string ev = "{\"ev\":\"node\",\"id\":" + std::to_string(id) + ",\"cls\":\"" + cls + "\"";
        if (containers.empty()) {
            ev += ",\"parent\":null";
        } else {
            const Container& c = containers.back();
            ev += ",\"parent\":" + std::to_string(c.id);
            if (c.isObject) ev += ",\"key\":\"" + jsonEscape(c.key) + "\"";
            else ev += ",\"index\":" + std::to_string(c.index);
        }
        std::string pv = preview.size() > 32 ? preview.substr(0, 29) + "..." : preview;
        ev += ",\"preview\":\"" + jsonEscape(pv) + "\",\"pos\":" + std::to_string(start) + "}";
        trace->events.push_back(ev);
        return id;
    }

    // ---- errors ---------------------------------------------------------

    std::string describe(size_t at) const {
        if (at >= json.size()) return "end of input";
        unsigned char c = static_cast<unsigned char>(json[at]);
        if (c == '\n') return "newline";
        if (c == '\t') return "tab";
        if (c == '\r') return "carriage return";
        if (c < 0x20 || c == 0x7f) {
            char buf[16];
            std::snprintf(buf, sizeof buf, "control character U+%04X", c);
            return buf;
        }
        if (c >= 0x80) {
            char buf[16];
            std::snprintf(buf, sizeof buf, "byte 0x%02X", c);
            return buf;
        }
        return std::string("'") + static_cast<char>(c) + "'";
    }

    [[noreturn]] void failAt(size_t at, const std::string& msg) {
        int line, col;
        lineCol(at, line, col);
        if (trace) {
            trace->events.push_back("{\"ev\":\"error\",\"pos\":" + std::to_string(at) + ",\"line\":" +
                                    std::to_string(line) + ",\"col\":" + std::to_string(col) +
                                    ",\"message\":\"" + jsonEscape(msg) + "\"}");
        }
        throw JsonParseError(msg, at, line, col);
    }
    [[noreturn]] void fail(const std::string& msg) { failAt(pos, msg); }

    // ---- grammar --------------------------------------------------------

    std::unique_ptr<JsonValue> parseValue() {
        Rule rule(this, "value");
        skipWhitespace();

        // Bounds check before accessing
        if (pos >= json.size()) {
            fail("unexpected end of input, expected a value");
        }

        char c = json[pos];

        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (isDigit(c) || c == '-') return parseNumber();

        fail("unexpected " + describe(pos) + ", expected a value");
    }

    static bool isDigit(char c) { return c >= '0' && c <= '9'; }

    // JSON whitespace is exactly space, tab, newline and carriage return.
    void skipWhitespace() {
        while (pos < json.size() &&
               (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
            pos++;
        }
    }

    void enterContainer() {
        if (++depth > kMaxDepth) {
            fail("nesting deeper than " + std::to_string(kMaxDepth) + " levels");
        }
    }

    std::unique_ptr<JsonArray> parseArray() {
        Rule rule(this, "array");
        enterContainer();
        size_t start = pos;
        token("LBRACKET", pos, pos + 1);
        pos++;

        auto arr = std::make_unique<JsonArray>();
        int id = node("JsonArray", "[]", start);
        containers.push_back({id, false, "", 0});

        skipWhitespace();
        if (pos < json.size() && json[pos] == ']') {
            token("RBRACKET", pos, pos + 1);
            pos++;
        } else {
            while (true) {
                containers.back().index = arr->size();
                arr->add(parseValue());

                skipWhitespace();
                if (pos < json.size() && json[pos] == ',') {
                    token("COMMA", pos, pos + 1);
                    pos++;
                    skipWhitespace();
                    if (pos < json.size() && json[pos] == ']') {
                        fail("trailing comma before ']'");
                    }
                } else if (pos < json.size() && json[pos] == ']') {
                    token("RBRACKET", pos, pos + 1);
                    pos++;
                    break;
                } else {
                    fail("expected ',' or ']' after array element, found " + describe(pos));
                }
            }
        }

        containers.pop_back();
        depth--;
        return arr;
    }

    std::unique_ptr<JsonObject> parseObject() {
        Rule rule(this, "object");
        enterContainer();
        size_t start = pos;
        token("LBRACE", pos, pos + 1);
        pos++; // Skip opening brace

        auto obj = std::make_unique<JsonObject>();
        int id = node("JsonObject", "{}", start);
        containers.push_back({id, true, "", 0});

        skipWhitespace();
        if (pos < json.size() && json[pos] == '}') {
            token("RBRACE", pos, pos + 1);
            pos++;
        } else {
            while (true) {
                Rule member(this, "member");

                // parse key
                if (pos >= json.size() || json[pos] != '"') {
                    fail("expected a string key in object, found " + describe(pos));
                }
                std::string key = parseStringLiteral();
                containers.back().key = key;

                skipWhitespace();
                if (pos >= json.size() || json[pos] != ':') {
                    fail("expected ':' after object key, found " + describe(pos));
                }
                token("COLON", pos, pos + 1);
                pos++; // skip :

                // parse value and add to obj
                obj->add(key, parseValue());

                // handle comma
                skipWhitespace();
                if (pos < json.size() && json[pos] == ',') {
                    token("COMMA", pos, pos + 1);
                    pos++;  // Skip comma, continue loop
                    skipWhitespace();
                    if (pos < json.size() && json[pos] == '}') {
                        fail("trailing comma before '}'");
                    }
                } else if (pos < json.size() && json[pos] == '}') {
                    token("RBRACE", pos, pos + 1);
                    pos++; // Skip closing brace
                    break;
                } else {
                    fail("expected ',' or '}' after object member, found " + describe(pos));
                }
            }
        }

        containers.pop_back();
        depth--;
        return obj;
    }

    std::unique_ptr<JsonString> parseString() {
        size_t start = pos;
        std::string str = parseStringLiteral();
        int id = node("JsonString", str, start);
        (void)id;
        return std::make_unique<JsonString>(str);
    }

    static int hexValue(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    unsigned readHex4() {
        unsigned v = 0;
        for (int i = 0; i < 4; i++) {
            int h = pos < json.size() ? hexValue(json[pos]) : -1;
            if (h < 0) fail("invalid \\u escape: expected 4 hex digits, found " + describe(pos));
            v = v * 16 + h;
            pos++;
        }
        return v;
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    // Reads a quoted string starting at the opening quote and returns its
    // decoded contents. Used for both string values and object keys.
    std::string parseStringLiteral() {
        Rule rule(this, "string");
        size_t start = pos;
        pos++; // Skip opening quote

        std::string str;
        while (true) {
            if (pos >= json.size()) {
                int line, col;
                lineCol(start, line, col);
                fail("unterminated string (opened at line " + std::to_string(line) + ", col " +
                     std::to_string(col) + ")");
            }
            unsigned char c = static_cast<unsigned char>(json[pos]);
            if (c == '"') break;
            if (c < 0x20) {
                fail("unescaped " + describe(pos) + " inside string");
            }
            if (c == '\\') {  // Escape sequence
                size_t escStart = pos;
                pos++;  // Skip backslash
                if (pos >= json.size()) continue;  // reported as unterminated above

                switch (json[pos]) {
                    case '"':  str += '"';  pos++; break;
                    case '\\': str += '\\'; pos++; break;
                    case '/':  str += '/';  pos++; break;
                    case 'b':  str += '\b'; pos++; break;
                    case 'f':  str += '\f'; pos++; break;
                    case 'n':  str += '\n'; pos++; break;
                    case 'r':  str += '\r'; pos++; break;
                    case 't':  str += '\t'; pos++; break;
                    case 'u': {
                        pos++;
                        unsigned cp = readHex4();
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            // High surrogate: must be followed by \uDC00-\uDFFF.
                            if (pos + 1 < json.size() && json[pos] == '\\' && json[pos + 1] == 'u') {
                                pos += 2;
                                unsigned lo = readHex4();
                                if (lo < 0xDC00 || lo > 0xDFFF) {
                                    failAt(escStart, "invalid UTF-16 surrogate pair in \\u escape");
                                }
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            } else {
                                failAt(escStart, "unpaired high surrogate in \\u escape");
                            }
                        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                            failAt(escStart, "unpaired low surrogate in \\u escape");
                        }
                        appendUtf8(str, cp);
                        break;
                    }
                    default:
                        failAt(escStart, "invalid escape sequence '\\" + std::string(1, json[pos]) + "'");
                }
            }
            else {
                str += static_cast<char>(c);
                pos++;
            }
        }

        pos++; // Skip closing quote
        token("STRING", start, pos);
        return str;
    }

    // number = [ "-" ] ( "0" | [1-9] digit* ) [ "." digit+ ] [ ("e"|"E") ["+"|"-"] digit+ ]
    std::unique_ptr<JsonNumber> parseNumber() {
        Rule rule(this, "number");
        size_t start = pos;

        if (json[pos] == '-') {
            pos++;
        }

        if (pos >= json.size() || !isDigit(json[pos])) {
            fail("expected a digit in number, found " + describe(pos));
        }
        if (json[pos] == '0') {
            pos++;
            if (pos < json.size() && isDigit(json[pos])) {
                failAt(pos - 1, "leading zeros are not allowed in numbers");
            }
        } else {
            while (pos < json.size() && isDigit(json[pos])) pos++;
        }

        if (pos < json.size() && json[pos] == '.') {
            pos++;
            if (pos >= json.size() || !isDigit(json[pos])) {
                fail("expected a digit after the decimal point, found " + describe(pos));
            }
            while (pos < json.size() && isDigit(json[pos])) pos++;
        }

        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            pos++;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) pos++;
            if (pos >= json.size() || !isDigit(json[pos])) {
                fail("expected a digit in the exponent, found " + describe(pos));
            }
            while (pos < json.size() && isDigit(json[pos])) pos++;
        }

        std::string numStr = json.substr(start, pos - start);
        double value = std::strtod(numStr.c_str(), nullptr);  // grammar already validated

        token("NUMBER", start, pos);
        node("JsonNumber", numStr, start);
        return std::make_unique<JsonNumber>(value);
    }

    // Matches a keyword exactly, reporting the first character that differs.
    void expectLiteral(const char* word, const char* tok) {
        Rule rule(this, "literal");
        size_t start = pos;
        for (size_t i = 0; word[i]; i++) {
            if (pos >= json.size() || json[pos] != word[i]) {
                fail("invalid literal, expected '" + std::string(word) + "' but found " + describe(pos));
            }
            pos++;
        }
        token(tok, start, pos);
    }

    std::unique_ptr<JsonBool> parseBool() {
        size_t start = pos;
        bool v = json[pos] == 't';
        expectLiteral(v ? "true" : "false", v ? "TRUE" : "FALSE");
        node("JsonBool", v ? "true" : "false", start);
        return std::make_unique<JsonBool>(v);
    }

    std::unique_ptr<JsonNull> parseNull() {
        size_t start = pos;
        expectLiteral("null", "NULL");
        node("JsonNull", "null", start);
        return std::make_unique<JsonNull>();
    }
};
