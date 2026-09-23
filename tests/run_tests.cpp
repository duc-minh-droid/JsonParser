// Test runner: conformance files + error positions + value checks.
//
//   json_tests <conformance-dir>
//
// Files named y_*.json must parse; n_*.json must be rejected with a
// JsonParseError. Exits 0 when every check passes, 1 otherwise.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include "../JsonParser.h"

namespace fs = std::filesystem;

static int failures = 0;
static int passed = 0;

static void check(bool ok, const std::string& what) {
    if (ok) { passed++; return; }
    failures++;
    std::cout << "FAIL  " << what << "\n";
}

static std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static void runConformance(const fs::path& dir) {
    std::vector<fs::path> files;
    for (auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".json") files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());

    int y = 0, n = 0;
    for (auto& f : files) {
        std::string name = f.filename().string();
        bool expectValid = name.rfind("y_", 0) == 0;
        bool expectInvalid = name.rfind("n_", 0) == 0;
        if (!expectValid && !expectInvalid) continue;
        (expectValid ? y : n)++;

        std::string text = readFile(f);
        try {
            JsonParser(text).parse();
            check(expectValid, name + ": accepted, but should be rejected");
        } catch (const JsonParseError& e) {
            check(expectInvalid, name + ": rejected (" + e.what() + "), but should be accepted");
        }
    }
    std::cout << "conformance: " << y << " valid + " << n << " invalid files\n";
}

struct ErrorCase { const char* input; int line; int col; const char* contains; };

static void runErrorPositions() {
    const ErrorCase cases[] = {
        {"",                         1, 1,  "unexpected end of input"},
        {"[1, 2,]",                  1, 7,  "trailing comma"},
        {"[1 2]",                    1, 4,  "expected ',' or ']'"},
        {"{\"a\" 1}",                1, 6,  "expected ':'"},
        {"{\n  \"a\": 1,\n  b: 2\n}",  3, 3,  "expected a string key"},
        {"{\"a\": 1 \"b\": 2}",       1, 9,  "expected ',' or '}'"},
        {"012",                      1, 1,  "leading zeros"},
        {"1.",                       1, 3,  "after the decimal point"},
        {"1e+",                      1, 4,  "exponent"},
        {"\"abc",                    1, 5,  "unterminated string (opened at line 1, col 1)"},
        {"\"a\\qb\"",                1, 3,  "invalid escape sequence '\\q'"},
        {"\"\\u12G4\"",              1, 6,  "4 hex digits"},
        {"\"\\ud83d\"",              1, 2,  "unpaired high surrogate"},
        {"[\"a\nb\"]",               1, 4,  "unescaped newline"},
        {"tru",                      1, 4,  "expected 'true'"},
        {"nulL",                     1, 4,  "expected 'null'"},
        {"[1]]",                     1, 4,  "after the top-level value"},
        {"[\n  1,\n  2,\n  @\n]",    4, 3,  "unexpected '@'"},
        // column counts characters, not UTF-8 bytes: "é" is 2 bytes
        {"[\"\xC3\xA9\", x]",        1, 7,  "unexpected 'x'"},
    };
    for (const auto& c : cases) {
        std::string label = std::string("error case ") + jsonEscape(c.input);
        try {
            JsonParser(c.input).parse();
            check(false, label + ": accepted");
        } catch (const JsonParseError& e) {
            bool ok = e.line == c.line && e.column == c.col && e.message.find(c.contains) != std::string::npos;
            check(ok, label + ": got " + e.what() + ", expected line " + std::to_string(c.line) +
                      ", col " + std::to_string(c.col) + " containing \"" + c.contains + "\"");
        }
    }
    std::cout << "error positions: " << std::size(cases) << " cases\n";
}

static void runValueChecks() {
    int before = passed + failures;

    auto v = JsonParser("{\"b\": 1, \"a\": [true, null, -2.5e2], \"c\": \"x\\u00e9\\ud83d\\ude00\"}").parse();
    check(v->getType() == JsonType::OBJECT, "root is an object");
    auto& obj = dynamic_cast<JsonObject&>(*v);
    check(obj.size() == 3, "object has 3 members");

    std::vector<std::string> keys;
    for (auto& m : obj) keys.push_back(m.first);
    check((keys == std::vector<std::string>{"b", "a", "c"}), "object keeps insertion order");

    auto& arr = dynamic_cast<JsonArray&>(obj.get("a"));
    check(arr.size() == 3, "array has 3 elements");
    check(dynamic_cast<JsonBool&>(arr[0]).getValue() == true, "arr[0] is true");
    check(arr[1].getType() == JsonType::NULLL, "arr[1] is null");
    check(dynamic_cast<JsonNumber&>(arr[2]).getValue() == -250.0, "arr[2] is -250");
    check(dynamic_cast<JsonString&>(obj.get("c")).getValue() == "x\xC3\xA9\xF0\x9F\x98\x80",
          "\\u escapes and surrogate pairs decode to UTF-8");

    // double precision (the old float storage lost this)
    auto n = JsonParser("1234567.891").parse();
    check(dynamic_cast<JsonNumber&>(*n).getValue() == 1234567.891, "numbers keep double precision");

    auto d = JsonParser("{\"k\": 1, \"k\": 2}").parse();
    auto& dobj = dynamic_cast<JsonObject&>(*d);
    check(dobj.size() == 1 && dynamic_cast<JsonNumber&>(dobj.get("k")).getValue() == 2,
          "duplicate key: last value wins");

    // trace: balanced enter/exit and one node per value
    JsonTrace t;
    JsonParser tp("[1, {\"a\": \"b\"}]");
    tp.setTrace(&t);
    tp.parse();
    int enters = 0, exits = 0, nodes = 0;
    for (auto& e : t.events) {
        if (e.find("\"ev\":\"enter\"") != std::string::npos) enters++;
        if (e.find("\"ev\":\"exit\"") != std::string::npos) exits++;
        if (e.find("\"ev\":\"node\"") != std::string::npos) nodes++;
    }
    check(enters == exits && enters > 0, "trace enter/exit events balance");
    check(nodes == 4, "trace creates one node per value (4)");

    std::cout << "value checks: " << (passed + failures - before) << " assertions\n";
}

int main(int argc, char** argv) {
    fs::path dir = argc > 1 ? fs::path(argv[1]) : fs::path("tests/conformance");
    if (!fs::is_directory(dir)) {
        std::cerr << "conformance directory not found: " << dir.string() << "\n";
        return 2;
    }
    runConformance(dir);
    runErrorPositions();
    runValueChecks();
    std::cout << passed << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
