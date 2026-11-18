#pragma once

#include "classcaption/transcriber.h"

#include <span>

namespace classcaption {

// Where captions go: the window, a .vtt file, the terminal. Word times count from lecture start.
// Calls come from the inference thread.
class CaptionSink {
  public:
    CaptionSink() = default;
    CaptionSink(const CaptionSink&) = delete;
    CaptionSink& operator=(const CaptionSink&) = delete;
    CaptionSink(CaptionSink&&) = delete;
    CaptionSink& operator=(CaptionSink&&) = delete;
    virtual ~CaptionSink() = default;

    // Words that just became final. They follow every earlier final word and never change.
    virtual void on_final(std::span<const Word> words) = 0;
    // The current guess for words after the final ones. Replaces the previous draft; may be empty.
    virtual void on_draft(std::span<const Word> words) = 0;
    // The speaker paused, so the current line of captions is complete.
    virtual void on_pause() = 0;
};

} // namespace classcaption
