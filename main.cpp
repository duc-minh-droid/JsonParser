#include <iostream>
#include <memory>
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

void printValue(const JsonValue& val, int indent) {
    switch (val.getType()) {
        case JsonType::NUMBER:
            std::cout << dynamic_cast<const JsonNumber&>(val).getValue();
            break;
        case JsonType::STRING:
            std::cout << "\"" << dynamic_cast<const JsonString&>(val).getValue() << "\"";
            break;
        case JsonType::BOOL:
            std::cout << (dynamic_cast<const JsonBool&>(val).getValue() ? "true" : "false");
            break;
        case JsonType::NULLL:
            std::cout << "null";
            break;
        case JsonType::ARRAY: {
            const JsonArray& arr = dynamic_cast<const JsonArray&>(val);
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
            std::cout << "{\n";
            int count = 0;
            for (const auto& pair : obj) {
                printIndent(indent + 1);
                std::cout << "\"" << pair.first << "\": ";
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
    auto result = parser.parse();
    
    if (result) {
        std::cout << "Parsed successfully:\n";
        printValue(*result, 0);
        std::cout << "\n";
    } else {
        std::cout << "Parse FAILED!\n";
    }
    std::cout << "\n";
}

int main()
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

    std::cout << "=== All Parser Tests Completed! ===\n";
    
    return 0;   
}
