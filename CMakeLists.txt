cmake_minimum_required(VERSION 3.28)
project(ClassCaption LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CLASSCAPTION_COREML "Run the Whisper encoder on the Neural Engine" ON)

set(WHISPER_COREML ${CLASSCAPTION_COREML} CACHE BOOL "" FORCE)
# Lets the same binary run when the Core ML encoder is missing, which is how cpu and metal modes work.
set(WHISPER_COREML_ALLOW_FALLBACK ON CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/whisper.cpp EXCLUDE_FROM_ALL SYSTEM)

set(CLASSCAPTION_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)

add_library(classcaption_core src/config.cpp)
target_include_directories(classcaption_core PUBLIC include)
target_compile_options(classcaption_core PRIVATE ${CLASSCAPTION_WARNINGS})

enable_testing()
add_executable(classcaption_tests tests/main.cpp tests/config_test.cpp)
target_link_libraries(classcaption_tests PRIVATE classcaption_core)
target_include_directories(classcaption_tests SYSTEM PRIVATE third_party/doctest)
target_compile_options(classcaption_tests PRIVATE ${CLASSCAPTION_WARNINGS})
target_compile_definitions(classcaption_tests PRIVATE CLASSCAPTION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME classcaption_tests COMMAND classcaption_tests)
