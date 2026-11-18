#pragma once

#include "classcaption/caption_sink.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace classcaption {

// Logs when each draft and final word reached the screen, as tab-separated lines, so the delay
// from speech to caption can be measured afterwards against the words' times in the audio.
class EventLogSink final : public CaptionSink {
  public:
    explicit EventLogSink(const std::filesystem::path& path);

    // Sets time zero. Call right before the audio starts.
    void mark_start();

    void on_final(std::span<const Word> words) override;
    void on_draft(std::span<const Word> words) override;
    void on_pause() override;

  private:
    void log(std::string_view kind, std::span<const Word> words);

    std::ofstream file_;
    std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
};

} // namespace classcaption
