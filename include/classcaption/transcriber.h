#pragma once

#include "classcaption/audio_source.h"
#include "classcaption/config.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
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

// Whether whisper.cpp's own VAD drops silence before transcribing. Live captions already send only
// speech. A whole recording needs it, or Whisper can repeat one phrase through a long silence.
enum class Silence : std::uint8_t { keep, skip };

class TranscriberError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Speech to text. whisper.cpp is used only in this class's source file, so switching to another
// engine means rewriting that one file.
class Transcriber {
  public:
    // Loads the model for config.backend. Throws TranscriberError if anything is missing.
    Transcriber(const Config& config, const std::filesystem::path& model,
                Silence silence = Silence::keep);
    ~Transcriber();
    Transcriber(const Transcriber&) = delete;
    Transcriber& operator=(const Transcriber&) = delete;
    Transcriber(Transcriber&&) = delete;
    Transcriber& operator=(Transcriber&&) = delete;

    // Transcribes 16 kHz mono audio of any length, reporting progress as a percentage if asked.
    // Not thread-safe: use one Transcriber per thread.
    std::vector<Word> transcribe(std::span<const float> audio,
                                 const std::function<void(int)>& on_progress = {});

  private:
    struct Engine;
    std::unique_ptr<Engine> engine_;
};

} // namespace classcaption
