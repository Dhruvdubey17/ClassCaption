#include "classcaption/finalize.h"
#include "classcaption/session_files.h"
#include "classcaption/transcriber.h"
#include "classcaption/wav.h"
#include "fixtures.h"

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

TEST_CASE("finalize skips silence, so it needs the VAD model") {
    const auto recording = fixtures::temp_path("lecture-2026-11-17-1100.wav");
    WavWriter(recording).append(read_wav(fixtures::kJfkWav));
    filesystem::remove(final_captions_for(recording));
    Config config = test_config();
    config.vad_model = "models/nope-vad.bin";
    CHECK_THROWS_WITH_AS(finalize_recording(config, recording, {}),
                         doctest::Contains("models/nope-vad.bin"), TranscriberError);
}
