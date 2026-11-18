#pragma once

#include "classcaption/audio_source.h"

#include <memory>

namespace classcaption {

// Captures the default microphone as 16 kHz mono. The OS audio thread pushes straight into the
// ring buffer, and samples are dropped rather than blocking if the buffer is full.
class MicSource final : public AudioSource {
  public:
    MicSource();
    ~MicSource() override;
    MicSource(const MicSource&) = delete;
    MicSource& operator=(const MicSource&) = delete;
    MicSource(MicSource&&) = delete;
    MicSource& operator=(MicSource&&) = delete;

    void start(RingBuffer& out) override;
    void stop() override;
    [[nodiscard]] bool finished() const override { return false; }

  private:
    // Keeps miniaudio out of this header.
    struct Device;
    std::unique_ptr<Device> device_;
};

} // namespace classcaption
