cmake_minimum_required(VERSION 3.16)
project(JsonParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(MINGW)
    # avoid picking up a mismatched libstdc++ DLL from another MinGW on PATH
    add_link_options(-static)
  endif()
endif()

# The parser is header-only; the .cpp files next to the headers are empty
# translation units kept for the Visual Studio project.
add_executable(jsonparser main.cpp)
add_executable(json_tests tests/run_tests.cpp)

enable_testing()
add_test(NAME conformance COMMAND json_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance)
