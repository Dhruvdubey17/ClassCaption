#pragma once

#include <filesystem>
#include <string_view>

namespace fixtures {

inline const std::filesystem::path kSourceDir = CLASSCAPTION_SOURCE_DIR;
inline const std::filesystem::path kJfkWav = kSourceDir / "third_party/whisper.cpp/samples/jfk.wav";
inline const std::filesystem::path kTinyModel = kSourceDir / "models/ggml-tiny.en.bin";
inline const std::filesystem::path kVadModel = kSourceDir / "models/ggml-silero-v6.2.0.bin";

// A fresh path under the system temp directory. Any file left there by an earlier run is removed.
inline std::filesystem::path temp_path(std::string_view name) {
    const auto dir = std::filesystem::temp_directory_path() / "classcaption-tests";
    std::filesystem::create_directories(dir);
    auto path = dir / name;
    std::filesystem::remove_all(path);
    return path;
}

} // namespace fixtures
