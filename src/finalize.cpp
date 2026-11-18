#include "classcaption/finalize.h"

#include "classcaption/session_files.h"
#include "classcaption/transcriber.h"
#include "classcaption/vtt_sink.h"
#include "classcaption/wav.h"

#include <format>

using namespace std;

namespace classcaption {

filesystem::path finalize_recording(const Config& config, const filesystem::path& recording,
                                    const function<void(int)>& on_progress) {
    const filesystem::path captions = final_captions_for(recording);
    // These may have been corrected by hand, so never replace them silently.
    if (filesystem::exists(captions)) {
        throw FinalizeError(
            format("{} already exists. Delete it to redo the after-class pass", captions.string()));
    }

    Config final_config = config;
    final_config.backend = config.final_backend;
    Transcriber transcriber(final_config, config.final_model);
    const vector<Word> words = transcriber.transcribe(read_wav(recording), on_progress);

    VttSink sink(captions);
    sink.on_final(words);
    sink.on_pause();
    return captions;
}

} // namespace classcaption
