#pragma once
#include <string>
#include <memory>
#include "JsonValue.h"
#include "JsonNumber.h"
#include "JsonString.h"
#include "JsonNull.h"
#include "JsonBool.h"
#include "JsonArray.h"
#include "JsonObject.h"

class JsonParser {
private:
    std::string json;
    size_t pos = 0;  // Current position

public:
    JsonParser(const std::string& str) : json(str) {}

    std::unique_ptr<JsonValue> parse() {
        return parseValue(); 
    }

private:
    std::unique_ptr<JsonValue> parseValue() {
        skipWhitespace();

        // Bounds check before accessing
        if (pos >= json.size()) {
            return nullptr;  // Error: unexpected end
        }

        char c = json[pos];

        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (isdigit(c) || c == '-') return parseNumber();

        return nullptr;  // Error
    }

    void skipWhitespace() {
        while (pos < json.size() && isspace(json[pos])) {
            pos++;
        }
    }

    std::unique_ptr<JsonArray> parseArray() {
        pos++;
        skipWhitespace();

        auto arr = std::make_unique<JsonArray>();

        while (pos < json.size() && json[pos] != ']') {
            auto value = parseValue();
            if (!value) return nullptr; // Handle error
            arr->add(std::move(value));

            skipWhitespace();
            if (pos < json.size() && json[pos] == ',') {
                pos++;
                skipWhitespace();
            }
        }

        pos++;
        return arr;
    }

    std::unique_ptr<JsonObject> parseObject() {
        pos++; // Skip opening brace
        skipWhitespace();

        auto obj = std::make_unique<JsonObject>();

        while (pos < json.size() && json[pos] != '}') {
            // parse key
            std::unique_ptr<JsonString> keyObj = parseString();
            if (!keyObj) return nullptr; // Handle error
            std::string key = keyObj->getValue();

            skipWhitespace();
            pos++; // skip :
            skipWhitespace();

            // parse value
            std::unique_ptr<JsonValue> valueObj = parseValue();

            // add to obj
            obj->add(key, std::move(valueObj));

            // handle comma
            skipWhitespace();
            if (json[pos] == ',') {
                pos++;  // Skip comma, continue loop
                skipWhitespace();
            }
        }
        
        pos++; // Skip closing brace

        return obj;
    }

    std::unique_ptr<JsonString> parseString() {
        pos++; // Skip opening quote

        std::string str;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\') {  // Escape sequence
                pos++;  // Skip backslash
                if (pos >= json.size()) break;

                // Handle common escapes
                if (json[pos] == 'n') str += '\n';
                else if (json[pos] == 't') str += '\t';
                else if (json[pos] == '"') str += '"';
                else if (json[pos] == '\\') str += '\\';
                else str += json[pos];  // Just add the character

                pos++;
            }
            else {
                str += json[pos];
                pos++;
            }
        }
        
        pos++; // Skip closing quote

        return std::make_unique<JsonString>(str);
    }

    std::unique_ptr<JsonNumber> parseNumber() {
        size_t start = pos;
        
        if (json[pos] == '-') {
            pos++;
        }

        while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '.')) {
            pos++;
        }
        std::string numStr = json.substr(start, pos - start);
        double value = std::stod(numStr);

        return std::make_unique<JsonNumber>(value);
    }

    std::unique_ptr<JsonBool> parseBool() {
        if (pos + 4 <= json.size() && json.substr(pos, 4) == "true") {
            pos += 4;
            return std::make_unique<JsonBool>(true);
        }
        else if (pos + 5 <= json.size() && json.substr(pos, 5) == "false") {
            pos += 5;
            return std::make_unique<JsonBool>(false);
        }
        return nullptr;  // Error
    }

    std::unique_ptr<JsonNull> parseNull() {
        if (json.substr(pos, 4) == "null") {
            pos += 4;
            return std::make_unique<JsonNull>();
        }
        return nullptr;  // Error
    }
};

