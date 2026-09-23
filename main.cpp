#include <charconv>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include "JsonValue.h"
#include "JsonObject.h"
#include "JsonNumber.h"
#include "JsonString.h"
#include "JsonBool.h"
#include "JsonNull.h"
#include "JsonArray.h"
#include "JsonParser.h"

void printValue(const JsonValue& val, int indent = 0);

void printIndent(int indent) {
    for (int i = 0; i < indent; i++) {
        std::cout << "  ";
    }
}

// Shortest decimal form that round-trips back to the same double.
std::string formatNumber(double v) {
    char buf[32];
    auto res = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, res.ptr);
}

void printValue(const JsonValue& val, int indent) {
    switch (val.getType()) {
        case JsonType::NUMBER:
            std::cout << formatNumber(dynamic_cast<const JsonNumber&>(val).getValue());
            break;
        case JsonType::STRING:
            std::cout << "\"" << jsonEscape(dynamic_cast<const JsonString&>(val).getValue()) << "\"";
            break;
        case JsonType::BOOL:
            std::cout << (dynamic_cast<const JsonBool&>(val).getValue() ? "true" : "false");
            break;
        case JsonType::NULLL:
            std::cout << "null";
            break;
        case JsonType::ARRAY: {
            const JsonArray& arr = dynamic_cast<const JsonArray&>(val);
            if (arr.size() == 0) { std::cout << "[]"; break; }
            std::cout << "[\n";
            for (int i = 0; i < arr.size(); i++) {
                printIndent(indent + 1);
                printValue(arr[i], indent + 1);
                if (i < arr.size() - 1) std::cout << ",";
                std::cout << "\n";
            }
            printIndent(indent);
            std::cout << "]";
            break;
        }
        case JsonType::OBJECT: {
            const JsonObject& obj = dynamic_cast<const JsonObject&>(val);
            if (obj.size() == 0) { std::cout << "{}"; break; }
            std::cout << "{\n";
            int count = 0;
            for (const auto& pair : obj) {
                printIndent(indent + 1);
                std::cout << "\"" << jsonEscape(pair.first) << "\": ";
                printValue(*pair.second, indent + 1);
                if (++count < obj.size()) std::cout << ",";
                std::cout << "\n";
            }
            printIndent(indent);
            std::cout << "}";
            break;
        }
    }
}

void testParse(const std::string& testName, const std::string& jsonStr) {
    std::cout << "=== " << testName << " ===\n";
    std::cout << "Input: " << jsonStr << "\n";

    JsonParser parser(jsonStr);
    try {
        auto result = parser.parse();
        std::cout << "Parsed successfully:\n";
        printValue(*result, 0);
        std::cout << "\n";
    } catch (const JsonParseError& e) {
        std::cout << "Parse FAILED: " << e.what() << "\n";
    }
    std::cout << "\n";
}

int runDemo()
{
    std::cout << "=== JSON Parser Tests ===\n\n";

    // Test 1: Simple number
    testParse("Simple Number", "42");

    // Test 2: Simple string
    testParse("Simple String", "\"hello world\"");

    // Test 3: Boolean true
    testParse("Boolean True", "true");

    // Test 4: Boolean false
    testParse("Boolean False", "false");

    // Test 5: Null
    testParse("Null Value", "null");

    // Test 6: Simple array
    testParse("Simple Array", "[1, 2, 3, 4, 5]");

    // Test 7: Mixed array
    testParse("Mixed Array", "[1, \"hello\", true, null, false]");

    // Test 8: Simple object
    testParse("Simple Object", "{\"name\": \"John\", \"age\": 30}");

    // Test 9: Nested object
    testParse("Nested Object",
        "{\"person\": {\"name\": \"Alice\", \"age\": 25}, \"active\": true}");

    // Test 10: Object with array
    testParse("Object with Array",
        "{\"name\": \"Bob\", \"hobbies\": [\"reading\", \"coding\", \"gaming\"]}");

    // Test 11: Array of objects
    testParse("Array of Objects",
        "[{\"name\": \"Alice\", \"age\": 25}, {\"name\": \"Bob\", \"age\": 30}]");

    // Test 12: Complex nested structure
    testParse("Complex Nested",
        "{\"users\": [{\"name\": \"Alice\", \"scores\": [10, 20, 30]}, "
        "{\"name\": \"Bob\", \"scores\": [15, 25, 35]}], \"count\": 2}");

    // Test 13: String with escape sequences
    testParse("Escaped String", "\"Hello\\nWorld\\t!\"");

    // Test 14: Negative number
    testParse("Negative Number", "-42");

    // Test 15: Decimal number
    testParse("Decimal Number", "3.14159");

    // Test 16: Empty object
    testParse("Empty Object", "{}");

    // Test 17: Empty array
    testParse("Empty Array", "[]");

    // Test 18: Whitespace handling
    testParse("Whitespace Test",
        "  {  \"name\"  :  \"test\"  ,  \"value\"  :  123  }  ");

    // Test 19: Error reporting
    testParse("Missing Colon", "{\"name\" \"test\"}");

    std::cout << "=== All Parser Tests Completed! ===\n";

    return 0;
}

void usage() {
    std::cerr <<
        "usage: jsonparser [--trace] [FILE | -]\n"
        "       jsonparser --demo\n"
        "\n"
        "  FILE      parse FILE and pretty-print it ('-' reads stdin)\n"
        "  --trace   print a JSON trace of every token, grammar rule and node\n"
        "            instead of the value (used by the web visualizer)\n"
        "  --demo    run the built-in example inputs\n"
        "\n"
        "exit status: 0 valid JSON, 1 parse error, 2 usage or I/O error\n";
}

int main(int argc, char** argv)
{
    bool traceMode = false;
    bool demo = false;
    const char* path = nullptr;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--trace") == 0) traceMode = true;
        else if (std::strcmp(argv[i], "--demo") == 0) demo = true;
        else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) { usage(); return 0; }
        else if (argv[i][0] == '-' && argv[i][1] != '\0') { usage(); return 2; }
        else if (!path) path = argv[i];
        else { usage(); return 2; }
    }

    if (demo || (!path && !traceMode)) return runDemo();

    std::string input;
    std::string name = (!path || std::strcmp(path, "-") == 0) ? "<stdin>" : path;
    if (name == "<stdin>") {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        input = ss.str();
    } else {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::cerr << "jsonparser: cannot open " << path << "\n";
            return 2;
        }
        input.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    JsonParser parser(input);
    JsonTrace trace;
    if (traceMode) parser.setTrace(&trace);

    try {
        auto result = parser.parse();
        if (traceMode) {
            std::cout << trace.toJson(input, nullptr);
        } else {
            printValue(*result, 0);
            std::cout << "\n";
        }
        return 0;
    } catch (const JsonParseError& e) {
        if (traceMode) std::cout << trace.toJson(input, &e);
        std::cerr << name << ":" << e.line << ":" << e.column << ": error: " << e.message << "\n";
        return 1;
    }
}
