#pragma once

namespace classcaption {

class RingBuffer;

// Whisper expects 16 kHz mono audio, so every source delivers exactly that.
inline constexpr int kSampleRate = 16'000;

class AudioSource {
  public:
    AudioSource() = default;
    AudioSource(const AudioSource&) = delete;
    AudioSource& operator=(const AudioSource&) = delete;
    AudioSource(AudioSource&&) = delete;
    AudioSource& operator=(AudioSource&&) = delete;
    virtual ~AudioSource() = default;

    // Starts delivering samples into `out` from a background thread. `out` must outlive the source.
    virtual void start(RingBuffer& out) = 0;
    virtual void stop() = 0;
    // True once a finite source has delivered its last sample. A microphone never finishes.
    [[nodiscard]] virtual bool finished() const = 0;
};

} // namespace classcaption
