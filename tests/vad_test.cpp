#include "classcaption/audio_source.h"
#include "classcaption/speech_gate.h"
#include "classcaption/vad.h"
#include "classcaption/wav.h"
#include "fixtures.h"

#include <algorithm>
#include <doctest.h>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

Config test_config() {
    Config config;
    config.vad_model = fixtures::kVadModel;
    return config;
}

// Sends audio through the VAD in 10 ms pieces, the way the microphone delivers it.
vector<float> probabilities_in_pieces(Vad& vad, span<const float> audio) {
    vector<float> probabilities;
    constexpr size_t kPiece = 160;
    for (size_t offset = 0; offset < audio.size(); offset += kPiece) {
        const auto piece = audio.subspan(offset, min(kPiece, audio.size() - offset));
        const vector<float> more = vad.process(piece);
        probabilities.insert(probabilities.end(), more.begin(), more.end());
    }
    return probabilities;
}

} // namespace

TEST_CASE("one probability comes back per whole frame, however the audio is split") {
    Vad vad(test_config());
    const vector<float> audio = read_wav(fixtures::kJfkWav);
    const vector<float> probabilities = probabilities_in_pieces(vad, audio);
    CHECK(probabilities.size() == audio.size() / kVadFrameSamples);
    CHECK(ranges::all_of(probabilities, [](float p) { return p >= 0.0F && p <= 1.0F; }));
}

TEST_CASE("the fixture speech opens the gate early and keeps it open for most of the clip") {
    const Config config = test_config();
    Vad vad(config);
    SpeechGate gate(config.vad_threshold, 15);
    const vector<float> probabilities = probabilities_in_pieces(vad, read_wav(fixtures::kJfkWav));

    const auto first_open =
        ranges::find_if(probabilities, [&gate](float p) { return gate.update(p); });
    REQUIRE(first_open != probabilities.end());
    const auto first_open_ms = (first_open - probabilities.begin()) * kVadFrameMs;
    CHECK(first_open_ms < 1'000);

    SpeechGate fresh_gate(config.vad_threshold, 15);
    const auto open_frames =
        ranges::count_if(probabilities, [&fresh_gate](float p) { return fresh_gate.update(p); });
    CHECK(open_frames > static_cast<ptrdiff_t>(probabilities.size()) / 2);
}

TEST_CASE("silence never opens the gate") {
    const Config config = test_config();
    Vad vad(config);
    SpeechGate gate(config.vad_threshold, 15);
    const vector<float> silence(static_cast<size_t>(kSampleRate) * 3, 0.0F);
    const vector<float> probabilities = vad.process(silence);
    CHECK(ranges::none_of(probabilities, [&gate](float p) { return gate.update(p); }));
}

TEST_CASE("a missing VAD model fails at startup with the path in the message") {
    Config config;
    config.vad_model = "models/nope-vad.bin";
    CHECK_THROWS_WITH_AS(Vad{config}, doctest::Contains("models/nope-vad.bin"), VadError);
}
