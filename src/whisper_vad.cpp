#include "classcaption/vad.h"
#include "whisper_log.h"

#include <filesystem>
#include <format>
#include <whisper.h>

using namespace std;

namespace classcaption {
namespace {

struct VadContextDeleter {
    void operator()(whisper_vad_context* context) const { whisper_vad_free(context); }
};

} // namespace

struct Vad::Engine {
    unique_ptr<whisper_vad_context, VadContextDeleter> context;
    // Audio waiting to fill a whole frame.
    vector<float> pending;
};

Vad::Vad(const Config& config) : engine_(make_unique<Engine>()) {
    if (!filesystem::exists(config.vad_model)) {
        throw VadError(format("VAD model not found at {}. Run scripts/fetch_models.sh",
                              config.vad_model.string()));
    }
    whisper_log_set(print_whisper_errors_only, nullptr);

    whisper_vad_context_params params = whisper_vad_default_context_params();
    // The Silero model is under 1 MB, too small to be worth a GPU or a second thread.
    params.n_threads = 1;
    params.use_gpu = false;
    engine_->context.reset(
        whisper_vad_init_from_file_with_params(config.vad_model.c_str(), params));
    if (!engine_->context) {
        throw VadError(format("could not load VAD model {}", config.vad_model.string()));
    }
}

Vad::~Vad() = default;

vector<float> Vad::process(span<const float> audio) {
    vector<float>& pending = engine_->pending;
    pending.insert(pending.end(), audio.begin(), audio.end());
    // whisper.cpp zero-pads a partial frame, which would score half a frame of silence.
    const size_t whole = pending.size() - (pending.size() % kVadFrameSamples);
    if (whole == 0) {
        return {};
    }

    whisper_vad_context* const context = engine_->context.get();
    if (!whisper_vad_detect_speech_no_reset(context, pending.data(), static_cast<int>(whole))) {
        throw VadError("Silero VAD failed on a block of audio");
    }
    pending.erase(pending.begin(), pending.begin() + static_cast<ptrdiff_t>(whole));

    const float* const probabilities = whisper_vad_probs(context);
    return {probabilities, probabilities + whisper_vad_n_probs(context)};
}

} // namespace classcaption
