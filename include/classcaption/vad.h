#pragma once

#include "classcaption/config.h"

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace classcaption {

// Silero VAD scores audio in 512-sample frames, 32 ms at 16 kHz.
inline constexpr std::size_t kVadFrameSamples = 512;
inline constexpr int kVadFrameMs = 32;

class VadError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Voice activity detection with Silero, through whisper.cpp.
class Vad {
  public:
    explicit Vad(const Config& config);
    ~Vad();
    Vad(const Vad&) = delete;
    Vad& operator=(const Vad&) = delete;
    Vad(Vad&&) = delete;
    Vad& operator=(Vad&&) = delete;

    // Returns one speech probability per completed frame. Audio can arrive in pieces of any size:
    // a partial frame waits for the next call, and the model keeps its memory between calls.
    std::vector<float> process(std::span<const float> audio);

  private:
    struct Engine;
    std::unique_ptr<Engine> engine_;
};

} // namespace classcaption
