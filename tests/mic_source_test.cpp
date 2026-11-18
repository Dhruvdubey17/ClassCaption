#include "classcaption/mic_source.h"
#include "classcaption/ring_buffer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <doctest.h>
#include <thread>
#include <vector>

using namespace std;
using namespace classcaption;

// Needs a real microphone and permission, so CI skips it. Run by hand while talking:
//   build/classcaption_tests -tc="the microphone*" --no-skip
TEST_CASE("the microphone records sound" * doctest::skip()) {
    RingBuffer buffer(static_cast<size_t>(kSampleRate) * 4);
    MicSource mic;
    mic.start(buffer);
    this_thread::sleep_for(2s);
    mic.stop();

    vector<float> samples(buffer.size());
    buffer.pop(samples);
    CHECK(samples.size() > kSampleRate);
    CHECK(ranges::any_of(samples, [](float s) { return abs(s) > 0.01F; }));
    CHECK_FALSE(mic.finished());
}
