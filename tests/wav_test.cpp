#include "classcaption/wav.h"
#include "fixtures.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <doctest.h>
#include <fstream>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

// A minimal 16-bit PCM header with no samples, for formats WavWriter never produces.
void write_header(const filesystem::path& path, uint16_t channels, uint32_t sample_rate) {
    const auto put = [](ofstream& out, auto value) {
        array<char, sizeof(value)> bytes{};
        memcpy(bytes.data(), &value, sizeof(value));
        out.write(bytes.data(), bytes.size());
    };
    const auto put32 = [&put](ofstream& out, uint32_t value) { put(out, value); };
    const auto put16 = [&put](ofstream& out, uint16_t value) { put(out, value); };
    ofstream out(path, ios::binary);
    out << "RIFF";
    put32(out, 36);
    out << "WAVEfmt ";
    put32(out, 16);
    put16(out, 1);
    put16(out, channels);
    put32(out, sample_rate);
    put32(out, sample_rate * channels * 2);
    put16(out, static_cast<uint16_t>(channels * 2));
    put16(out, 16);
    out << "data";
    put32(out, 0);
}

} // namespace

TEST_CASE("reads the 11 second fixture clip as 16 kHz mono samples") {
    const vector<float> samples = read_wav(fixtures::kJfkWav);
    CHECK(samples.size() == 176'000);
    CHECK(ranges::all_of(samples, [](float s) { return s >= -1.0F && s <= 1.0F; }));
    CHECK(ranges::any_of(samples, [](float s) { return s != 0.0F; }));
}

TEST_CASE("audio written in pieces reads back as one recording") {
    const auto path = fixtures::temp_path("round_trip.wav");
    const vector<float> first = {0.0F, 0.5F, -0.5F};
    const vector<float> second = {1.0F, -1.0F};
    {
        WavWriter writer(path);
        writer.append(first);
        writer.append(second);
    }
    const vector<float> samples = read_wav(path);
    REQUIRE(samples.size() == 5);
    const vector<float> expected = {0.0F, 0.5F, -0.5F, 1.0F, -1.0F};
    for (size_t i = 0; i < expected.size(); ++i) {
        CHECK(samples[i] == doctest::Approx(expected[i]).epsilon(1.0 / 32'768));
    }
}

TEST_CASE("a recording is readable after every append, so a crash keeps what was saved") {
    const auto path = fixtures::temp_path("crash_safe.wav");
    WavWriter writer(path);
    writer.append(vector<float>{0.25F, 0.25F});
    CHECK(read_wav(path).size() == 2);
    writer.append(vector<float>{0.25F});
    CHECK(read_wav(path).size() == 3);
}

TEST_CASE("files that are not 16 kHz mono 16-bit WAV are rejected with a clear message") {
    CHECK_THROWS_WITH_AS(read_wav(fixtures::temp_path("missing.wav")),
                         doctest::Contains("cannot open"), WavError);

    const auto stereo = fixtures::temp_path("stereo.wav");
    write_header(stereo, 2, 16'000);
    CHECK_THROWS_WITH_AS(read_wav(stereo), doctest::Contains("must be mono"), WavError);

    const auto cd_rate = fixtures::temp_path("cd_rate.wav");
    write_header(cd_rate, 1, 44'100);
    CHECK_THROWS_WITH_AS(read_wav(cd_rate), doctest::Contains("must be 16000 Hz"), WavError);

    const auto text = fixtures::temp_path("text.wav");
    ofstream(text) << "hello";
    CHECK_THROWS_WITH_AS(read_wav(text), doctest::Contains("not a WAV file"), WavError);
}
