cmake_minimum_required(VERSION 3.28)
project(ClassCaption LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CLASSCAPTION_COREML "Run the Whisper encoder on the Neural Engine" ON)
option(CLASSCAPTION_TSAN "Build ClassCaption code with ThreadSanitizer" OFF)

set(WHISPER_COREML ${CLASSCAPTION_COREML} CACHE BOOL "" FORCE)
# Lets the same binary run when the Core ML encoder is missing, which is how cpu and metal modes work.
set(WHISPER_COREML_ALLOW_FALLBACK ON CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/whisper.cpp EXCLUDE_FROM_ALL SYSTEM)

set(CLASSCAPTION_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
# Only our own code is instrumented. whisper.cpp's thread pool trips ThreadSanitizer on its own
# lock-free sync and makes every test many times slower.
if(CLASSCAPTION_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

add_library(miniaudio STATIC third_party/miniaudio/miniaudio.c)
target_include_directories(miniaudio SYSTEM PUBLIC third_party/miniaudio)
target_compile_definitions(miniaudio PUBLIC MA_NO_DECODING MA_NO_ENCODING MA_NO_GENERATION)
target_link_libraries(miniaudio PRIVATE "-framework CoreFoundation" "-framework CoreAudio"
                                        "-framework AudioToolbox")

add_library(classcaption_core
    src/config.cpp
    src/file_source.cpp
    src/mic_source.cpp
    src/ring_buffer.cpp
    src/speech_gate.cpp
    src/vtt_writer.cpp
    src/wav.cpp
    src/whisper_transcriber.cpp
    src/whisper_vad.cpp
)
target_link_libraries(classcaption_core PRIVATE miniaudio whisper)
if(CLASSCAPTION_COREML)
    target_compile_definitions(classcaption_core PRIVATE CLASSCAPTION_COREML)
endif()
target_include_directories(classcaption_core PUBLIC include)
target_compile_options(classcaption_core PRIVATE ${CLASSCAPTION_WARNINGS})

enable_testing()
add_executable(classcaption_tests
    tests/main.cpp
    tests/config_test.cpp
    tests/file_source_test.cpp
    tests/mic_source_test.cpp
    tests/ring_buffer_test.cpp
    tests/speech_gate_test.cpp
    tests/transcriber_test.cpp
    tests/vad_test.cpp
    tests/vtt_writer_test.cpp
    tests/wav_test.cpp
)
target_link_libraries(classcaption_tests PRIVATE classcaption_core)
target_include_directories(classcaption_tests SYSTEM PRIVATE third_party/doctest)
target_compile_options(classcaption_tests PRIVATE ${CLASSCAPTION_WARNINGS})
target_compile_definitions(classcaption_tests PRIVATE CLASSCAPTION_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME classcaption_tests COMMAND classcaption_tests)
set_tests_properties(classcaption_tests PROPERTIES TIMEOUT 300)
