#pragma once

#include "classcaption/audio_source.h"
#include "classcaption/config.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace classcaption {

// One recognized word. Times count from the start of the audio passed to transcribe().
struct Word {
    std::string text;
    std::chrono::milliseconds start{};
    std::chrono::milliseconds end{};
};

class TranscriberError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Speech to text. whisper.cpp is used only in this class's source file, so switching to another
// engine means rewriting that one file.
class Transcriber {
  public:
    // Loads the model for config.backend. Throws TranscriberError if anything is missing.
    Transcriber(const Config& config, const std::filesystem::path& model);
    ~Transcriber();
    Transcriber(const Transcriber&) = delete;
    Transcriber& operator=(const Transcriber&) = delete;
    Transcriber(Transcriber&&) = delete;
    Transcriber& operator=(Transcriber&&) = delete;

    // Transcribes up to 30 s of 16 kHz mono audio. Not thread-safe: use one per thread.
    std::vector<Word> transcribe(std::span<const float> audio);

  private:
    struct Engine;
    std::unique_ptr<Engine> engine_;
};

} // namespace classcaption
