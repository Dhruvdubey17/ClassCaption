#include "classcaption/config.h"
#include "classcaption/file_source.h"
#include "classcaption/mic_source.h"
#include "classcaption/pipeline.h"
#include "classcaption/session_files.h"
#include "classcaption/vtt_sink.h"

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

Config load_config(int argc, char** argv) {
    const vector<string> args(argv + 1, argv + argc);
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

int caption(const Config& config) {
    filesystem::create_directories(config.output_dir);
    const SessionFiles files = session_files(config.output_dir, local_now());

    TerminalSink terminal;
    VttSink captions(files.captions);
    Pipeline pipeline(config, {&terminal, &captions});
    const unique_ptr<AudioSource> source = open_source(config);
    optional<WavWriter> recording;
    if (config.save_audio) {
        recording.emplace(files.audio);
    }

    cerr << "Captioning " << (config.input.empty() ? "the microphone" : config.input.string())
         << ". Captions go to " << files.captions.string() << ". Press Ctrl+C to stop.\n";
    signal(SIGINT, [](int /*signal*/) { stop_requested = true; });
    pipeline.run(*source, stop_requested, recording ? &*recording : nullptr);
    cerr << "Saved " << files.captions.string() << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return caption(load_config(argc, argv));
    } catch (const exception& error) {
        cerr << "classcaption: " << error.what() << '\n';
        return 1;
    }
}
