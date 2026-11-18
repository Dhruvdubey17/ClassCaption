#pragma once

#include "classcaption/audio_source.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <thread>
#include <vector>

namespace classcaption {

// Plays a WAV file into the pipeline. Real-time pacing makes a recording behave like a live
// lecture, which is how tests and latency measurements run without a microphone.
class FileSource final : public AudioSource {
  public:
    enum class Pace : std::uint8_t { as_fast_as_possible, real_time };

    FileSource(const std::filesystem::path& path, Pace pace);
    ~FileSource() override;
    FileSource(const FileSource&) = delete;
    FileSource& operator=(const FileSource&) = delete;
    FileSource(FileSource&&) = delete;
    FileSource& operator=(FileSource&&) = delete;

    void start(RingBuffer& out) override;
    void stop() override;
    [[nodiscard]] bool finished() const override;
    [[nodiscard]] std::size_t sample_count() const { return samples_.size(); }

  private:
    void deliver(RingBuffer& out);

    std::vector<float> samples_;
    Pace pace_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> finished_{false};
    std::thread thread_;
};

} // namespace classcaption
