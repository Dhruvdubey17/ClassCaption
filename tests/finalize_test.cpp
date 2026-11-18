#include "classcaption/audio_source.h"
#include "classcaption/finalize.h"
#include "classcaption/session_files.h"
#include "classcaption/vad.h"
#include "classcaption/wav.h"
#include "fixtures.h"

#include <cctype>
#include <doctest.h>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

Config test_config() {
    Config config;
    config.final_backend = Backend::cpu;
    config.final_model = fixtures::kTinyModel;
    config.vad_model = fixtures::kVadModel;
    return config;
}

} // namespace

TEST_CASE("finalize writes clean captions next to the recording") {
    const auto recording = fixtures::temp_path("lecture-2026-11-17-0905.wav");
    WavWriter(recording).append(read_wav(fixtures::kJfkWav));
    filesystem::remove(final_captions_for(recording)); // left over from an earlier test run

    vector<int> progress;
    const auto captions =
        finalize_recording(test_config(), recording, [&progress](int p) { progress.push_back(p); });

    CHECK(captions == final_captions_for(recording));
    ifstream file(captions);
    stringstream text;
    text << file.rdbuf();
    CHECK(text.str().starts_with("WEBVTT\n"));
    CHECK(text.str().find("what you can do for your country") != string::npos);
    REQUIRE_FALSE(progress.empty());
    CHECK(progress.back() <= 100);
}

TEST_CASE("finalize refuses to overwrite existing final captions") {
    const auto recording = fixtures::temp_path("lecture-2026-11-17-1000.wav");
    WavWriter(recording).append(read_wav(fixtures::kJfkWav));
    ofstream(final_captions_for(recording)) << "WEBVTT\n\nhand-corrected\n";

    CHECK_THROWS_WITH_AS(finalize_recording(test_config(), recording, {}),
                         doctest::Contains("already exists"), FinalizeError);
}

TEST_CASE("finalize cuts the recording at pauses, so it needs the VAD model") {
    const auto recording = fixtures::temp_path("lecture-2026-11-17-1100.wav");
    WavWriter(recording).append(read_wav(fixtures::kJfkWav));
    filesystem::remove(final_captions_for(recording));
    Config config = test_config();
    config.vad_model = "models/nope-vad.bin";
    CHECK_THROWS_WITH_AS(finalize_recording(config, recording, {}),
                         doctest::Contains("models/nope-vad.bin"), VadError);
}

TEST_CASE("a long recording with silences comes out with each sentence once, at the right time") {
    const auto recording = fixtures::temp_path("lecture-2026-11-17-1200.wav");
    filesystem::remove(final_captions_for(recording));
    const vector<float> speech = read_wav(fixtures::kJfkWav);
    const vector<float> silence(static_cast<size_t>(kSampleRate) * 20, 0.0F);
    {
        WavWriter writer(recording);
        writer.append(speech);
        writer.append(silence);
        writer.append(speech);
        writer.append(silence);
        writer.append(speech);
    }
    const auto captions = finalize_recording(test_config(), recording, {});

    ifstream file(captions);
    string text;
    string third_start;
    for (string line; getline(file, line);) {
        if (line.find("-->") != string::npos) {
            if (line >= "00:01:02" && third_start.empty()) {
                third_start = line.substr(0, 12);
            }
            continue;
        }
        for (char& c : line) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
        text += line + ' ';
    }
    size_t count = 0;
    for (auto at = text.find("what you can do for your country"); at != string::npos;
         at = text.find("what you can do for your country", at + 1)) {
        ++count;
    }
    CHECK(count == 3);
    CHECK(third_start >= "00:01:02.000");
    CHECK(third_start < "00:01:04.000");
}
