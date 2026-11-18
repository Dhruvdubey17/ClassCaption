#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace classcaption {

enum class Backend : std::uint8_t { coreml, metal, cpu };

// Every tunable lives here, so switching model or backend is a config edit, never a code change.
struct Config {
    Backend backend = Backend::coreml;
    std::filesystem::path live_model = "models/ggml-large-v3-turbo-q5_0.bin";
    std::filesystem::path final_model = "models/ggml-large-v3-q5_0.bin";
    std::filesystem::path vad_model = "models/ggml-silero-v6.2.0.bin";
    std::string language = "en";
    float vad_threshold = 0.5F;
    int step_ms = 1000;
    std::filesystem::path output_dir = "lectures";
    bool save_audio = true;
};

class ConfigError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Reads `key = value` lines on top of the defaults. `#` starts a comment line.
Config parse_config(std::string_view text);

// Applies `--key value` or `--key=value` arguments on top of an existing config.
Config apply_args(Config config, std::span<const std::string> args);

} // namespace classcaption
