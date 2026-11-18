#pragma once

#include "classcaption/caption_sink.h"

#include <mutex>
#include <string>
#include <vector>

namespace classcaption {

// The text behind the caption window. The inference thread writes through the CaptionSink calls
// and the UI thread reads snapshots, so every access takes the lock.
class CaptionLog final : public CaptionSink {
  public:
    struct Snapshot {
        std::vector<std::string> lines; // final captions, one line per utterance
        std::string draft;              // words that may still change, shown after the last line
    };

    void on_final(std::span<const Word> words) override;
    void on_draft(std::span<const Word> words) override;
    void on_pause() override;

    [[nodiscard]] Snapshot snapshot() const;

  private:
    mutable std::mutex mutex_;
    std::vector<std::string> lines_;
    bool line_open_ = false;
    std::string draft_;
};

// The view sticks to the newest captions unless the reader has scrolled up to reread something.
// Takes the scroll position from the previous frame and the furthest it can scroll.
bool keep_following(float scroll_y, float scroll_max);

} // namespace classcaption
