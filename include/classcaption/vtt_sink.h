#pragma once

#include "classcaption/caption_sink.h"
#include "classcaption/vtt_writer.h"

#include <filesystem>
#include <vector>

namespace classcaption {

// Saves final captions as WebVTT cues. A cue ends at a sentence end, at a pause, or when it
// reaches a length that still fits on two lines of a video player.
class VttSink final : public CaptionSink {
  public:
    explicit VttSink(const std::filesystem::path& path);

    void on_final(std::span<const Word> words) override;
    void on_draft(std::span<const Word> /*words*/) override {}
    void on_pause() override;

  private:
    void write_cue();

    VttWriter writer_;
    std::vector<Word> pending_;
};

} // namespace classcaption
