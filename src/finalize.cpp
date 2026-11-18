#include "classcaption/finalize.h"

#include "classcaption/file_source.h"
#include "classcaption/pipeline.h"
#include "classcaption/session_files.h"
#include "classcaption/vtt_sink.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>

using namespace std;

namespace classcaption {
namespace {

// Longer than the longest utterance the pipeline allows, so each utterance gets exactly one pass:
// the one when it ends. With no drafts to show, more passes would only cost time.
constexpr int kOnePassPerUtterance = 60'000;

// Reports how far through the recording the final captions have reached.
class ProgressSink final : public CaptionSink {
  public:
    ProgressSink(chrono::milliseconds duration, const function<void(int)>& on_progress)
        : duration_(duration), on_progress_(on_progress) {}

    void on_final(span<const Word> words) override {
        if (!on_progress_ || words.empty() || duration_.count() <= 0) {
            return;
        }
        on_progress_(static_cast<int>(min<int64_t>(100, words.back().end * 100 / duration_)));
    }
    void on_draft(span<const Word> /*words*/) override {}
    void on_pause() override {}

  private:
    chrono::milliseconds duration_;
    const function<void(int)>& on_progress_;
};

} // namespace

filesystem::path finalize_recording(const Config& config, const filesystem::path& recording,
                                    const function<void(int)>& on_progress) {
    const filesystem::path captions = final_captions_for(recording);
    // These may have been corrected by hand, so never replace them silently.
    if (filesystem::exists(captions)) {
        throw FinalizeError(
            format("{} already exists. Delete it to redo the after-class pass", captions.string()));
    }

    // The same utterance-by-utterance path as live captions, with the larger model. Handing
    // Whisper the whole recording at once let it repeat one sentence dozens of times.
    Config final_config = config;
    final_config.live_model = config.final_model;
    final_config.backend = config.final_backend;
    final_config.step_ms = kOnePassPerUtterance;

    FileSource source(recording, FileSource::Pace::as_fast_as_possible);
    const auto duration =
        chrono::milliseconds(static_cast<int64_t>(source.sample_count()) * 1000 / kSampleRate);
    VttSink sink(captions);
    ProgressSink progress(duration, on_progress);
    Pipeline pipeline(final_config, {&sink, &progress});
    const atomic<bool> stop{false};
    pipeline.run(source, stop);
    return captions;
}

} // namespace classcaption
