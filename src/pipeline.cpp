#include "classcaption/pipeline.h"

#include "classcaption/ring_buffer.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

using namespace std;

namespace classcaption {
namespace {

// 30 s of audio: room for inference to fall behind briefly without the microphone dropping audio.
constexpr size_t kRingSamples = static_cast<size_t>(kSampleRate) * 30;
// Silero reacts a little after a word starts, so keep this much audio from before the trigger.
constexpr size_t kPreRollSamples = kVadFrameSamples * 6;
// A pause longer than this (16 frames of 32 ms) ends an utterance.
constexpr int kPauseFrames = 16;
// Whisper sees at most 30 s at once; cut long unbroken speech before it gets there.
constexpr size_t kMaxUtteranceSamples = static_cast<size_t>(kSampleRate) * 25;

chrono::milliseconds samples_to_ms(size_t samples) {
    return chrono::milliseconds(static_cast<int64_t>(samples) * 1000 / kSampleRate);
}

size_t ms_to_samples(int ms) {
    return static_cast<size_t>(ms) * kSampleRate / 1000;
}

} // namespace

Pipeline::Pipeline(Config config, vector<CaptionSink*> sinks)
    : config_(std::move(config)), vad_(config_), transcriber_(config_, config_.live_model),
      gate_(config_.vad_threshold, kPauseFrames), sinks_(std::move(sinks)) {}

void Pipeline::run(AudioSource& source, const atomic<bool>& stop, WavWriter* recording) {
    RingBuffer ring(kRingSamples);
    source.start(ring);
    vector<float> chunk(static_cast<size_t>(kSampleRate) / 10);
    const size_t step_samples = ms_to_samples(config_.step_ms);

    while (!stop) {
        const size_t count = ring.pop(chunk);
        if (count == 0) {
            if (source.finished() && ring.size() == 0) {
                break;
            }
            this_thread::sleep_for(10ms);
            continue;
        }
        const span<const float> audio = span(chunk).first(count);
        if (recording != nullptr) {
            recording->append(audio);
        }

        unscored_.insert(unscored_.end(), audio.begin(), audio.end());
        for (const float probability : vad_.process(audio)) {
            const auto frame = span(unscored_).first(kVadFrameSamples);
            on_frame(frame, gate_.update(probability));
            unscored_.erase(unscored_.begin(),
                            unscored_.begin() + static_cast<ptrdiff_t>(kVadFrameSamples));
        }

        if (!utterance_.empty() && utterance_.size() - samples_at_last_pass_ >= step_samples) {
            transcribe_pass();
        }
    }
    source.stop();
    end_utterance();
}

void Pipeline::on_frame(span<const float> frame, bool is_speech) {
    scored_samples_ += frame.size();
    if (!is_speech) {
        if (!utterance_.empty()) {
            end_utterance();
        }
        pre_roll_.insert(pre_roll_.end(), frame.begin(), frame.end());
        if (pre_roll_.size() > kPreRollSamples) {
            pre_roll_.erase(pre_roll_.begin(),
                            pre_roll_.end() - static_cast<ptrdiff_t>(kPreRollSamples));
        }
        return;
    }

    if (utterance_.empty()) {
        utterance_.swap(pre_roll_);
        utterance_start_ = scored_samples_ - frame.size() - utterance_.size();
    }
    utterance_.insert(utterance_.end(), frame.begin(), frame.end());
    if (utterance_.size() >= kMaxUtteranceSamples) {
        end_utterance();
    }
}

void Pipeline::transcribe_pass() {
    samples_at_last_pass_ = utterance_.size();
    vector<Word> hypothesis;
    try {
        hypothesis = transcriber_.transcribe(utterance_);
    } catch (const TranscriberError& error) {
        // One failed pass loses a second of drafts, not the lecture: report it and carry on.
        cerr << "classcaption: " << error.what() << '\n';
        return;
    }
    const Stabilizer::Update update = stabilizer_.update(hypothesis);
    const vector<Word> finalized = in_lecture_time(update.finalized);
    const vector<Word> draft = in_lecture_time(update.draft);
    for (CaptionSink* sink : sinks_) {
        if (!finalized.empty()) {
            sink->on_final(finalized);
        }
        sink->on_draft(draft);
    }
}

void Pipeline::end_utterance() {
    if (utterance_.empty()) {
        return;
    }
    if (samples_at_last_pass_ < utterance_.size()) {
        transcribe_pass();
    }
    const vector<Word> rest = in_lecture_time(stabilizer_.finish_utterance());
    for (CaptionSink* sink : sinks_) {
        if (!rest.empty()) {
            sink->on_final(rest);
        }
        sink->on_draft({});
        sink->on_pause();
    }
    utterance_.clear();
    samples_at_last_pass_ = 0;
}

vector<Word> Pipeline::in_lecture_time(vector<Word> words) const {
    const chrono::milliseconds offset = samples_to_ms(utterance_start_);
    for (Word& word : words) {
        word.start += offset;
        word.end += offset;
    }
    return words;
}

} // namespace classcaption
