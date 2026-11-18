#include "classcaption/config.h"
#include "classcaption/transcriber.h"
#include "classcaption/wav.h"
#include "fixtures.h"

#include <algorithm>
#include <cctype>
#include <doctest.h>
#include <string>

using namespace std;
using namespace classcaption;

namespace {

Config cpu_config() {
    Config config;
    config.backend = Backend::cpu;
    return config;
}

// Lowercase letters and single spaces only, so the check ignores punctuation and casing.
string normalized_text(const vector<Word>& words) {
    string text;
    for (const Word& word : words) {
        text += ' ';
        for (const char c : word.text) {
            if (isalpha(static_cast<unsigned char>(c)) != 0) {
                text += static_cast<char>(tolower(static_cast<unsigned char>(c)));
            }
        }
    }
    return text;
}

} // namespace

TEST_CASE("transcribes the fixture clip with word timings") {
    Transcriber transcriber(cpu_config(), fixtures::kTinyModel);
    const vector<Word> words = transcriber.transcribe(read_wav(fixtures::kJfkWav));

    CHECK(normalized_text(words).find("ask not what your country can do for you") != string::npos);
    REQUIRE_FALSE(words.empty());
    CHECK(words.front().start >= 0ms);
    CHECK(words.back().end <= 11'000ms);
    CHECK(ranges::all_of(words, [](const Word& w) { return w.start <= w.end; }));
    CHECK(ranges::is_sorted(words, {}, &Word::start));
    CHECK(ranges::none_of(words, [](const Word& w) { return w.text.starts_with(' '); }));
}

TEST_CASE("silence gives no words") {
    Transcriber transcriber(cpu_config(), fixtures::kTinyModel);
    const vector<float> silence(static_cast<size_t>(kSampleRate) * 2, 0.0F);
    CHECK(transcriber.transcribe(silence).empty());
}

TEST_CASE("a missing model fails at startup with the path in the message") {
    CHECK_THROWS_WITH_AS(Transcriber(cpu_config(), "models/nope.bin"),
                         doctest::Contains("models/nope.bin"), TranscriberError);
}

TEST_CASE("the Neural Engine backend refuses to start without its Core ML encoder") {
    Config config;
    config.backend = Backend::coreml;
    // tiny.en has no encoder in models/, so this must fail instead of silently using the CPU.
    CHECK_THROWS_WITH_AS(Transcriber(config, fixtures::kTinyModel), doctest::Contains("Core ML"),
                         TranscriberError);
}

// Needs the live model from `scripts/fetch_models.sh all`, so CI skips it. Run by hand with
//   build/classcaption_tests -tc="the live model*" --no-skip
TEST_CASE("the live model transcribes the fixture clip on every backend" * doctest::skip()) {
    const filesystem::path live_model = fixtures::kSourceDir / Config{}.live_model;
    for (const Backend backend : {Backend::coreml, Backend::metal, Backend::cpu}) {
        CAPTURE(static_cast<int>(backend));
        Config config;
        config.backend = backend;
        Transcriber transcriber(config, live_model);
        const vector<Word> words = transcriber.transcribe(read_wav(fixtures::kJfkWav));
        CHECK(normalized_text(words).find("ask not what your country can do for you") !=
              string::npos);
    }
}

TEST_CASE("skipping silence keeps word times on the recording's timeline") {
    // Long silences are where Whisper can loop on one phrase, so whole recordings skip them.
    Config config = cpu_config();
    config.vad_model = fixtures::kVadModel;
    Transcriber transcriber(config, fixtures::kTinyModel, Silence::skip);

    vector<float> audio(static_cast<size_t>(kSampleRate) * 20, 0.0F);
    const vector<float> speech = read_wav(fixtures::kJfkWav);
    audio.insert(audio.end(), speech.begin(), speech.end());
    const vector<Word> words = transcriber.transcribe(audio);

    CHECK(normalized_text(words).find("ask not what your country can do for you") != string::npos);
    REQUIRE_FALSE(words.empty());
    CHECK(words.front().start >= 19'500ms);
    CHECK(words.back().end <= 31'500ms);
}

TEST_CASE("skipping silence needs the VAD model") {
    Config config = cpu_config();
    config.vad_model = "models/nope-vad.bin";
    CHECK_THROWS_WITH_AS(Transcriber(config, fixtures::kTinyModel, Silence::skip),
                         doctest::Contains("models/nope-vad.bin"), TranscriberError);
}
