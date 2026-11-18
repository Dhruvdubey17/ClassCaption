#pragma once

namespace classcaption {

// Turns per-frame speech probabilities into "is someone talking right now". It opens at the
// threshold, stays open on a slightly lower bar, and closes only after `hangover_frames` quiet
// frames in a row, so the gaps between words do not split a sentence.
class SpeechGate {
  public:
    SpeechGate(float threshold, int hangover_frames);

    // Feeds one frame and returns whether speech is ongoing after it.
    bool update(float probability);

  private:
    float open_threshold_;
    float stay_open_threshold_;
    int hangover_frames_;
    int quiet_frames_ = 0;
    bool open_ = false;
};

} // namespace classcaption
