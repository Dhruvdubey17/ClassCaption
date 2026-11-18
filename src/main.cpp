#include "classcaption/caption_log.h"
#include "classcaption/config.h"
#include "classcaption/file_source.h"
#include "classcaption/finalize.h"
#include "classcaption/mic_source.h"
#include "classcaption/pipeline.h"
#include "classcaption/session_files.h"
#include "classcaption/vtt_sink.h"
#include "window/caption_window.h"

#include <atomic>
#include <csignal>
#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

// A signal handler can only reach state with static storage, so this one flag is global.
atomic<bool> stop_requested{false}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static_assert(atomic<bool>::is_always_lock_free, "the Ctrl+C handler needs a lock-free flag");

constexpr string_view kDefaultConfigFile = "config.ini";

// Prints final captions as running text, one line per utterance. Drafts are left to the window.
class TerminalSink final : public CaptionSink {
  public:
    void on_final(span<const Word> words) override {
        for (const Word& word : words) {
            cout << word.text << ' ';
        }
        cout << flush;
    }
    void on_draft(span<const Word> /*words*/) override {}
    void on_pause() override { cout << '\n' << flush; }
};

string read_text_file(const filesystem::path& path) {
    ifstream file(path);
    if (!file) {
        throw ConfigError(format("cannot read config file {}", path.string()));
    }
    stringstream text;
    text << file.rdbuf();
    return text.str();
}

Config load_config(span<const string> args) {
    const auto [config_file, rest] = take_config_flag(args);
    Config config;
    if (config_file) {
        config = parse_config(read_text_file(*config_file));
    } else if (filesystem::exists(kDefaultConfigFile)) {
        config = parse_config(read_text_file(kDefaultConfigFile));
    }
    return apply_args(config, rest);
}

tm local_now() {
    const time_t now = time(nullptr);
    tm local{};
    localtime_r(&now, &local);
    return local;
}

unique_ptr<AudioSource> open_source(const Config& config) {
    if (config.input.empty()) {
        return make_unique<MicSource>();
    }
    return make_unique<FileSource>(config.input, FileSource::Pace::real_time);
}

// Runs the pipeline on a worker thread while the window owns the main thread. Closing the window
// ends the session, and an error on the worker is rethrown here once it has stopped.
void run_with_window(Pipeline& pipeline, AudioSource& source, WavWriter* recording,
                     const CaptionLog& log) {
    atomic<bool> finished{false};
    exception_ptr failure;
    thread worker([&] {
        try {
            pipeline.run(source, stop_requested, recording);
        } catch (...) {
            failure = current_exception();
        }
        finished = true;
    });
    show_caption_window(log, stop_requested, finished);
    stop_requested = true;
    worker.join();
    if (failure) {
        rethrow_exception(failure);
    }
}

int caption(const Config& config) {
    filesystem::create_directories(config.output_dir);
    const SessionFiles files = session_files(config.output_dir, local_now());

    TerminalSink terminal;
    VttSink captions(files.captions);
    CaptionLog window_log;
    vector<CaptionSink*> sinks = {&terminal, &captions};
    if (config.show_window) {
        sinks.push_back(&window_log);
    }
    Pipeline pipeline(config, sinks);
    const unique_ptr<AudioSource> source = open_source(config);
    optional<WavWriter> recording;
    if (config.save_audio) {
        recording.emplace(files.audio);
    }
    WavWriter* const recording_or_null = recording ? &*recording : nullptr;

    cerr << "Captioning " << (config.input.empty() ? "the microphone" : config.input.string())
         << ". Captions go to " << files.captions.string() << ". Press Ctrl+C to stop.\n";
    signal(SIGINT, [](int /*signal*/) { stop_requested = true; });
    if (config.show_window) {
        run_with_window(pipeline, *source, recording_or_null, window_log);
    } else {
        pipeline.run(*source, stop_requested, recording_or_null);
    }
    cerr << "Saved " << files.captions.string() << '\n';
    return 0;
}

int finalize(const Config& config, const filesystem::path& recording) {
    cerr << "After-class pass on " << recording.string() << " with " << config.final_model.string()
         << ". This takes a while for a full lecture.\n";
    const filesystem::path captions = finalize_recording(
        config, recording, [](int percent) { cerr << "\r" << percent << "% " << flush; });
    cerr << "\nSaved " << captions.string() << '\n';
    return 0;
}

constexpr string_view kUsage = "usage: classcaption [--setting value ...]\n"
                               "       classcaption finalize <recording.wav> [--setting value ...]";

} // namespace

int main(int argc, char** argv) {
    try {
        const vector<string> args(argv + 1, argv + argc);
        if (!args.empty() && (args[0] == "--help" || args[0] == "-h")) {
            cout << kUsage << '\n';
            return 0;
        }
        if (!args.empty() && args[0] == "finalize") {
            if (args.size() < 2) {
                throw ConfigError(string(kUsage));
            }
            return finalize(load_config(span(args).subspan(2)), args[1]);
        }
        return caption(load_config(args));
    } catch (const exception& error) {
        cerr << "classcaption: " << error.what() << '\n';
        return 1;
    }
}
