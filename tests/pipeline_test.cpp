#include "classcaption/file_source.h"
#include "classcaption/pipeline.h"
#include "classcaption/wav.h"
#include "fixtures.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <doctest.h>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

Config test_config() {
    Config config;
    config.backend = Backend::cpu;
    config.live_model = fixtures::kTinyModel;
    config.vad_model = fixtures::kVadModel;
    return config;
}

class MemorySink final : public CaptionSink {
  public:
    void on_final(span<const Word> words) override {
        finals.insert(finals.end(), words.begin(), words.end());
    }
    void on_draft(span<const Word> words) override { drafts_seen += words.empty() ? 0 : 1; }
    void on_pause() override { ++pauses; }

    [[nodiscard]] string normalized_text() const {
        string text;
        for (const Word& word : finals) {
            text += ' ';
            for (const char c : word.text) {
                if (isalpha(static_cast<unsigned char>(c)) != 0) {
                    text += static_cast<char>(tolower(static_cast<unsigned char>(c)));
                }
            }
        }
        return text;
    }

    vector<Word> finals;
    int drafts_seen = 0;
    int pauses = 0;
};

size_t count_of(const string& text, const string& phrase) {
    size_t count = 0;
    for (auto at = text.find(phrase); at != string::npos; at = text.find(phrase, at + 1)) {
        ++count;
    }
    return count;
}

} // namespace

TEST_CASE("a recording becomes final captions, each word once, in lecture time") {
    MemorySink sink;
    Pipeline pipeline(test_config(), {&sink});
    FileSource source(fixtures::kJfkWav, FileSource::Pace::as_fast_as_possible);
    const atomic<bool> stop{false};
    pipeline.run(source, stop);

    const string text = sink.normalized_text();
    CHECK(count_of(text, "ask not what your country can do for you") == 1);
    CHECK(count_of(text, "fellow americans") == 1);
    CHECK(sink.pauses >= 1);
    CHECK(sink.drafts_seen >= 1);
    REQUIRE_FALSE(sink.finals.empty());
    CHECK(sink.finals.back().end <= 11'500ms);
    CHECK(ranges::is_sorted(sink.finals, {}, &Word::start));
}

TEST_CASE("the recording on disk holds every sample that was captioned") {
    const auto path = fixtures::temp_path("pipeline_recording.wav");
    MemorySink sink;
    Pipeline pipeline(test_config(), {&sink});
    FileSource source(fixtures::kJfkWav, FileSource::Pace::as_fast_as_possible);
    const atomic<bool> stop{false};
    {
        WavWriter recording(path);
        pipeline.run(source, stop, &recording);
    }
    CHECK(read_wav(path).size() == read_wav(fixtures::kJfkWav).size());
}

TEST_CASE("setting stop ends a live run promptly") {
    MemorySink sink;
    Pipeline pipeline(test_config(), {&sink});
    FileSource source(fixtures::kJfkWav, FileSource::Pace::real_time);
    atomic<bool> stop{false};

    const auto started = chrono::steady_clock::now();
    thread stopper([&stop] {
        this_thread::sleep_for(300ms);
        stop = true;
    });
    pipeline.run(source, stop);
    stopper.join();
    CHECK(chrono::steady_clock::now() - started < 5s);
}
