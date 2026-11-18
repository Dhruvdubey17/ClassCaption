#pragma once

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace classcaption {

// Carries audio from the microphone callback to the inference thread. Exactly one thread pushes
// and one thread pops. Neither side locks or allocates, so the audio callback never stalls.
class RingBuffer {
  public:
    explicit RingBuffer(std::size_t capacity);

    // Copies as many samples as fit and returns that count. Samples that do not fit are dropped.
    std::size_t push(std::span<const float> samples);

    // Moves up to out.size() samples into out and returns how many were moved.
    std::size_t pop(std::span<float> out);

    [[nodiscard]] std::size_t size() const;

  private:
    std::vector<float> storage_;
    // Both counters only ever grow; the slot is the counter modulo capacity.
    std::atomic<std::size_t> written_{0};
    std::atomic<std::size_t> read_{0};
};

} // namespace classcaption
