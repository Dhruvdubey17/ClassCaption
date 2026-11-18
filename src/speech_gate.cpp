#include "classcaption/speech_gate.h"

#include <algorithm>

using namespace std;

namespace classcaption {
namespace {

// The gap between the open and stay-open bars, as in Silero's reference speech detector.
constexpr float kHysteresis = 0.15F;

} // namespace

SpeechGate::SpeechGate(float threshold, int hangover_frames)
    : open_threshold_(threshold), stay_open_threshold_(max(0.0F, threshold - kHysteresis)),
      hangover_frames_(hangover_frames) {}

bool SpeechGate::update(float probability) {
    if (!open_) {
        open_ = probability >= open_threshold_;
        return open_;
    }
    if (probability >= stay_open_threshold_) {
        quiet_frames_ = 0;
        return true;
    }
    if (++quiet_frames_ > hangover_frames_) {
        open_ = false;
        quiet_frames_ = 0;
    }
    return open_;
}

} // namespace classcaption
