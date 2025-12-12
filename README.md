# JSON Parser in C++

A lightweight, header-based JSON parser implemented in C++ that supports parsing and working with JSON data structures.

## Features

- **Complete JSON Support**: Parse all JSON data types
  - Numbers (integers and decimals)
  - Strings (with escape sequence support)
  - Booleans (`true`/`false`)
  - Null values
  - Arrays
  - Objects
  
- **Nested Structures**: Full support for deeply nested objects and arrays
- **Type-Safe**: Uses C++ type system with polymorphism
- **Memory Safe**: Leverages `std::unique_ptr` for automatic memory management
- **Easy to Use**: Simple API for parsing and accessing JSON data

## Table of Contents

- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Usage Examples](#usage-examples)
- [API Reference](#api-reference)
- [Building](#building)
- [Testing](#testing)
- [Known Limitations](#known-limitations)
- [Contributing](#contributing)
- [License](#license)

## Project Structure

```
JsonParser/
├── JsonValue.h / .cpp      # Base class for all JSON types
├── JsonNumber.h / .cpp     # Number type implementation
├── JsonString.h / .cpp     # String type implementation
├── JsonBool.h / .cpp       # Boolean type implementation
├── JsonNull.h / .cpp       # Null type implementation
├── JsonArray.h / .cpp      # Array type implementation
├── JsonObject.h / .cpp     # Object type implementation
├── JsonParser.h / .cpp     # Main parser implementation
├── main.cpp                # Test/example program
├── JsonParser.vcxproj      # Visual Studio project file
└── README.md               # This file
```

## Getting Started

### Prerequisites

- C++11 or later compiler (GCC, Clang, MSVC)
- Standard C++ library

### Quick Start

1. **Clone the repository**
   ```bash
   git clone https://github.com/duc-minh-droid/JsonParser.git
   cd JsonParser
   ```

2. **Include the headers**
   ```cpp
   #include "JsonParser.h"
   ```

3. **Parse JSON**
   ```cpp
   std::string json = R"({"name": "John", "age": 30})";
   JsonParser parser(json);
   auto result = parser.parse();
   ```

## 💡 Usage Examples

### Basic Parsing

```cpp
#include "JsonParser.h"
#include <iostream>

int main() {
    // Parse a simple JSON object
    std::string json = R"({"name": "Alice", "age": 25, "active": true})";
    JsonParser parser(json);
    auto result = parser.parse();
    
    if (result) {
        std::cout << "Parsing successful!\n";
    }
    
    return 0;
}
```

### Working with Objects

```cpp
// Parse JSON object
std::string json = R"({"name": "Bob", "age": 30})";
JsonParser parser(json);
auto result = parser.parse();

if (result && result->getType() == JsonType::OBJECT) {
    JsonObject& obj = dynamic_cast<JsonObject&>(*result);
    
    // Access values
    JsonValue& name = obj.get("name");
    if (name.getType() == JsonType::STRING) {
        std::cout << "Name: " << dynamic_cast<JsonString&>(name).getValue() << "\n";
    }
}
```

### Working with Arrays

```cpp
// Parse JSON array
std::string json = R"([1, 2, 3, 4, 5])";
JsonParser parser(json);
auto result = parser.parse();

if (result && result->getType() == JsonType::ARRAY) {
    JsonArray& arr = dynamic_cast<JsonArray&>(*result);
    
    for (int i = 0; i < arr.size(); i++) {
        JsonValue& val = arr[i];
        if (val.getType() == JsonType::NUMBER) {
            std::cout << dynamic_cast<JsonNumber&>(val).getValue() << " ";
        }
    }
}
```

### Complex Nested Structures

```cpp
std::string json = R"({
    "users": [
        {"name": "Alice", "scores": [10, 20, 30]},
        {"name": "Bob", "scores": [15, 25, 35]}
    ],
    "count": 2
})";

JsonParser parser(json);
auto result = parser.parse();

if (result && result->getType() == JsonType::OBJECT) {
    JsonObject& obj = dynamic_cast<JsonObject&>(*result);
    JsonValue& users = obj.get("users");
    
    if (users.getType() == JsonType::ARRAY) {
        JsonArray& userArray = dynamic_cast<JsonArray&>(users);
        // Process each user...
    }
}
```

### Handling Escape Sequences

```cpp
// String with escape sequences
std::string json = R"("Hello\nWorld\t!")";
JsonParser parser(json);
auto result = parser.parse();

if (result && result->getType() == JsonType::STRING) {
    std::cout << dynamic_cast<JsonString&>(*result).getValue() << "\n";
    // Output: Hello
    //         World   !
}
```

## API Reference

### JsonParser

Main parser class for converting JSON strings into object structures.

```cpp
class JsonParser {
public:
    JsonParser(const std::string& json);
    std::unique_ptr<JsonValue> parse();
};
```

### JsonValue (Base Class)

All JSON types inherit from `JsonValue`.

```cpp
enum class JsonType { NUMBER, STRING, BOOL, NULLL, ARRAY, OBJECT };

class JsonValue {
public:
    virtual JsonType getType() const = 0;
    virtual ~JsonValue() = default;
};
```

### JsonNumber

```cpp
class JsonNumber : public JsonValue {
public:
    JsonNumber(float value);
    float getValue() const;
    JsonType getType() const override;
};
```

### JsonString

```cpp
class JsonString : public JsonValue {
public:
    JsonString(const std::string& value);
    std::string getValue() const;
    JsonType getType() const override;
};
```

### JsonBool

```cpp
class JsonBool : public JsonValue {
public:
    JsonBool(bool value);
    bool getValue() const;
    JsonType getType() const override;
};
```

### JsonNull

```cpp
class JsonNull : public JsonValue {
public:
    JsonNull();
    bool isNull() const;
    JsonType getType() const override;
};
```

### JsonArray

```cpp
class JsonArray : public JsonValue {
public:
    void add(std::unique_ptr<JsonValue> value);
    JsonValue& operator[](int index);
    int size() const;
    JsonType getType() const override;
};
```

### JsonObject

```cpp
class JsonObject : public JsonValue {
public:
    void add(const std::string& key, std::unique_ptr<JsonValue> value);
    JsonValue& get(const std::string& key);
    int size() const;
    JsonType getType() const override;
    
    // Iterator support
    auto begin() const;
    auto end() const;
};
```

## Building

### Visual Studio

1. Open `JsonParser.vcxproj` or `JsonParser.slnx`
2. Build the project (Ctrl+Shift+B)
3. Run (F5)

### Command Line (MSVC)

```bash
cl /EHsc /std:c++17 main.cpp JsonValue.cpp JsonNumber.cpp JsonString.cpp JsonBool.cpp JsonNull.cpp JsonArray.cpp JsonObject.cpp JsonParser.cpp
```

### GCC/Clang

```bash
g++ -std=c++17 -o jsonparser main.cpp JsonValue.cpp JsonNumber.cpp JsonString.cpp JsonBool.cpp JsonNull.cpp JsonArray.cpp JsonObject.cpp JsonParser.cpp
./jsonparser
```

## Testing

The `main.cpp` file includes comprehensive tests covering:

- Basic types (numbers, strings, booleans, null)
- Simple arrays and objects
- Nested structures
- Mixed-type arrays
- Escape sequences
- Whitespace handling
- Empty objects and arrays
- Complex nested JSON

Run the tests:

```bash
# After building
./jsonparser
```

Expected output will show parse results for 18 different test cases.

### Recommended Improvements

- Add bounds checking in all parsing functions
- Implement proper error handling with descriptive messages
- Add support for Unicode escape sequences (`\uXXXX`)
- Support scientific notation in numbers
- Use `double` instead of `float` for better precision
- Add comprehensive input validation

## Contributing

Contributions are welcome! Here's how you can help:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Areas for Contribution

- Bug fixes (especially bounds checking and validation)
- Performance improvements
- Better error messages
- Additional test cases
- Documentation improvements
- Unicode support

## License

This project is available for educational and personal use.

## Author

**duc-minh-droid**

- GitHub: [@duc-minh-droid](https://github.com/duc-minh-droid)
- Repository: [JsonParser](https://github.com/duc-minh-droid/JsonParser)

## Acknowledgments

- Inspired by JSON.org specification
- Built as a learning project for C++ and parsing techniques

---

**Note**: This is an educational project. For production use, consider mature libraries like [nlohmann/json](https://github.com/nlohmann/json) or [RapidJSON](https://github.com/Tencent/rapidjson).
