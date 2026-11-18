#include "classcaption/file_source.h"
#include "classcaption/ring_buffer.h"
#include "classcaption/wav.h"
#include "fixtures.h"

#include <chrono>
#include <doctest.h>
#include <thread>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

vector<float> drain_until_finished(AudioSource& source, RingBuffer& buffer) {
    vector<float> received;
    vector<float> chunk(4096);
    while (!source.finished() || buffer.size() > 0) {
        const size_t count = buffer.pop(chunk);
        received.insert(received.end(), chunk.begin(),
                        chunk.begin() + static_cast<ptrdiff_t>(count));
        this_thread::sleep_for(1ms);
    }
    return received;
}

} // namespace

TEST_CASE("a file source delivers every sample of the file, even through a small buffer") {
    RingBuffer buffer(1000);
    FileSource source(fixtures::kJfkWav, FileSource::Pace::as_fast_as_possible);
    source.start(buffer);
    const vector<float> received = drain_until_finished(source, buffer);
    CHECK(received == read_wav(fixtures::kJfkWav));
}

TEST_CASE("a paced file source takes about as long as the audio lasts") {
    const auto path = fixtures::temp_path("half_second.wav");
    WavWriter(path).append(vector<float>(kSampleRate / 2, 0.1F));

    RingBuffer buffer(kSampleRate);
    FileSource source(path, FileSource::Pace::real_time);
    const auto started = chrono::steady_clock::now();
    source.start(buffer);
    const vector<float> received = drain_until_finished(source, buffer);
    const auto elapsed = chrono::steady_clock::now() - started;

    CHECK(received.size() == kSampleRate / 2);
    CHECK(elapsed >= 450ms);
    CHECK(elapsed < 1500ms);
}

TEST_CASE("stopping a file source early ends delivery") {
    RingBuffer buffer(static_cast<size_t>(kSampleRate) * 20);
    FileSource source(fixtures::kJfkWav, FileSource::Pace::real_time);
    source.start(buffer);
    this_thread::sleep_for(100ms);
    source.stop();
    const size_t delivered = buffer.size();
    this_thread::sleep_for(100ms);
    CHECK(buffer.size() == delivered);
    CHECK(delivered < 176'000);
}
