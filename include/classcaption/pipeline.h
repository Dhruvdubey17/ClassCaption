#pragma once

#include "classcaption/audio_source.h"
#include "classcaption/caption_sink.h"
#include "classcaption/config.h"
#include "classcaption/speech_gate.h"
#include "classcaption/stabilizer.h"
#include "classcaption/transcriber.h"
#include "classcaption/vad.h"
#include "classcaption/wav.h"

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace classcaption {

// Audio in, captions out. Speech is cut into utterances at pauses; each utterance is
// re-transcribed every config.step_ms of new audio and stabilized, so sinks get a fresh draft on
// every pass and final words once two passes agree.
class Pipeline {
  public:
    Pipeline(Config config, std::vector<CaptionSink*> sinks);

    // Runs on the calling thread until the source has finished and all its audio is captioned, or
    // until `stop` is set. Every sample is also appended to `recording` when one is given.
    void run(AudioSource& source, const std::atomic<bool>& stop, WavWriter* recording = nullptr);

  private:
    void on_frame(std::span<const float> frame, bool is_speech);
    void transcribe_pass();
    void end_utterance();
    [[nodiscard]] std::vector<Word> in_lecture_time(std::vector<Word> words) const;

    Config config_;
    Vad vad_;
    Transcriber transcriber_;
    SpeechGate gate_;
    Stabilizer stabilizer_;
    std::vector<CaptionSink*> sinks_;

    std::vector<float> unscored_;  // audio waiting for its VAD score
    std::vector<float> pre_roll_;  // the most recent non-speech audio
    std::vector<float> utterance_; // audio of the utterance in progress
    std::size_t scored_samples_ = 0;
    std::size_t utterance_start_ = 0;
    std::size_t samples_at_last_pass_ = 0;
};

} // namespace classcaption
