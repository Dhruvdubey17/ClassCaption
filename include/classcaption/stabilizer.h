#pragma once

#include "classcaption/transcriber.h"

#include <chrono>
#include <vector>

namespace classcaption {

// Live captions re-transcribe the current utterance about once a second, and Whisper may revise
// its last few words each time. A word becomes final once two passes in a row agree on it
// (LocalAgreement-2); everything after that is a draft shown in grey. Final words never change.
class Stabilizer {
  public:
    struct Update {
        std::vector<Word> finalized; // words that became final on this pass, in order
        std::vector<Word> draft;     // the rest of this pass, which may still change
    };

    // Takes the newest transcript of the whole utterance so far. Times count from utterance start.
    Update update(const std::vector<Word>& hypothesis);

    // The speaker paused: everything still in draft becomes final and the next update() starts a
    // new utterance.
    std::vector<Word> finish_utterance();

  private:
    // Words of a pass that come after everything already final.
    [[nodiscard]] std::vector<Word> unfinalized(const std::vector<Word>& hypothesis) const;

    std::vector<Word> previous_;
    std::chrono::milliseconds finalized_until_{0};
};

} // namespace classcaption
