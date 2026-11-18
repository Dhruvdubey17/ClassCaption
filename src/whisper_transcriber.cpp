#include "classcaption/transcriber.h"
#include "whisper_log.h"

#include <algorithm>
#include <cstdio>
#include <format>
#include <fstream>
#include <string_view>
#include <thread>
#include <whisper.h>

using namespace std;

namespace classcaption {
namespace {

constexpr unsigned kMaxThreads = 4;
// whisper.cpp reports token times in centiseconds.
constexpr chrono::milliseconds kWhisperTick{10};

struct ContextDeleter {
    void operator()(whisper_context* context) const { whisper_free(context); }
};
using ContextPtr = unique_ptr<whisper_context, ContextDeleter>;

// whisper.cpp logs every load step. Keep that text only so a failed load can explain itself.
void collect_log(ggml_log_level /*level*/, const char* text, void* log) {
    *static_cast<string*>(log) += text;
}

#ifdef CLASSCAPTION_COREML

// The same rule whisper.cpp uses: drop the extension and any -qX_Y suffix, add -encoder.mlmodelc.
filesystem::path coreml_encoder_path(const filesystem::path& model) {
    string stem = model.stem().string();
    const auto dash = stem.rfind('-');
    const string_view suffix = dash == string::npos ? "" : string_view(stem).substr(dash);
    if (suffix.size() == 5 && suffix[1] == 'q' && suffix[3] == '_') {
        stem.resize(dash);
    }
    return model.parent_path() / (stem + "-encoder.mlmodelc");
}

void require_coreml_encoder(const filesystem::path& model) {
    const auto encoder = coreml_encoder_path(model);
    if (!filesystem::exists(encoder)) {
        throw TranscriberError(format("Core ML encoder not found at {}. Run "
                                      "scripts/fetch_models.sh all, or set backend = metal",
                                      encoder.string()));
    }
}

#else

void require_coreml_encoder(const filesystem::path& model) {
    throw TranscriberError(
        format("this build has no Core ML support, so {} cannot use backend = coreml. Rebuild with "
               "-DCLASSCAPTION_COREML=ON, or set backend = metal",
               model.string()));
}

#endif

ContextPtr load_with_coreml(const filesystem::path& model, const whisper_context_params& params) {
    return ContextPtr(whisper_init_from_file_with_params(model.c_str(), params));
}

// Loading through a stream leaves whisper.cpp without a model path, so it never finds a Core ML
// encoder and the whole model runs on Metal or the CPU, as configured.
ContextPtr load_without_coreml(const filesystem::path& model,
                               const whisper_context_params& params) {
    ifstream file(model, ios::binary);
    whisper_model_loader loader{};
    loader.context = &file;
    loader.read = [](void* stream, void* output, size_t size) -> size_t {
        auto& in = *static_cast<ifstream*>(stream);
        in.read(static_cast<char*>(output), static_cast<streamsize>(size));
        return static_cast<size_t>(in.gcount());
    };
    loader.eof = [](void* stream) { return static_cast<ifstream*>(stream)->eof(); };
    loader.close = [](void* /*stream*/) {};
    return ContextPtr(whisper_init_with_params(&loader, params));
}

ContextPtr load_model(const Config& config, const filesystem::path& model) {
    if (!filesystem::exists(model)) {
        throw TranscriberError(
            format("model not found at {}. Run scripts/fetch_models.sh", model.string()));
    }
    if (config.backend == Backend::coreml) {
        require_coreml_encoder(model);
    }

    whisper_context_params params = whisper_context_default_params();
    params.use_gpu = config.backend != Backend::cpu;

    // shortcut: the log hook is process-wide, so two models loading at once would mix their
    // logs. Only one loads at a time today; pass a per-load buffer if that changes.
    string log;
    whisper_log_set(collect_log, &log);
    ContextPtr context = config.backend == Backend::coreml ? load_with_coreml(model, params)
                                                           : load_without_coreml(model, params);
    whisper_log_set(print_whisper_errors_only, nullptr);

    if (!context) {
        throw TranscriberError(format("could not load {}:\n{}", model.string(), log));
    }
    return context;
}

whisper_full_params decoding_params(const string& language) {
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = static_cast<int>(min(kMaxThreads, max(1U, thread::hardware_concurrency())));
    params.language = language.c_str();
    params.no_context = true;
    params.token_timestamps = true;
    params.suppress_nst = true;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.print_special = false;
    return params;
}

chrono::milliseconds to_millis(int64_t whisper_ticks) {
    return whisper_ticks * kWhisperTick;
}

} // namespace

struct Transcriber::Engine {
    ContextPtr context;
    string language;
    whisper_full_params params{};
};

Transcriber::Transcriber(const Config& config, const filesystem::path& model)
    : engine_(make_unique<Engine>()) {
    engine_->context = load_model(config, model);
    engine_->language = config.language;
    // params holds a pointer into language, so it is built after language has its final home.
    engine_->params = decoding_params(engine_->language);
}

Transcriber::~Transcriber() = default;

vector<Word> Transcriber::transcribe(span<const float> audio) {
    whisper_context* const context = engine_->context.get();
    if (whisper_full(context, engine_->params, audio.data(), static_cast<int>(audio.size())) != 0) {
        throw TranscriberError("whisper.cpp could not transcribe the audio");
    }

    // Tokens are pieces of words. A token that starts with a space begins a new word.
    vector<Word> words;
    const whisper_token first_special = whisper_token_eot(context);
    for (int segment = 0; segment < whisper_full_n_segments(context); ++segment) {
        for (int token = 0; token < whisper_full_n_tokens(context, segment); ++token) {
            const whisper_token_data data = whisper_full_get_token_data(context, segment, token);
            if (data.id >= first_special) {
                continue;
            }
            const string_view piece = whisper_full_get_token_text(context, segment, token);
            if (words.empty() || piece.starts_with(' ')) {
                words.push_back(
                    {.text = "", .start = to_millis(data.t0), .end = to_millis(data.t1)});
            }
            Word& word = words.back();
            word.text += piece.starts_with(' ') ? piece.substr(1) : piece;
            word.end = max(word.end, to_millis(data.t1));
        }
    }
    erase_if(words, [](const Word& word) { return word.text.empty(); });
    return words;
}

} // namespace classcaption
