#include "classcaption/ring_buffer.h"

#include <algorithm>
#include <doctest.h>
#include <numeric>
#include <thread>
#include <vector>

using namespace std;
using namespace classcaption;

TEST_CASE("an empty buffer gives nothing back") {
    RingBuffer buffer(4);
    vector<float> out(4);
    CHECK(buffer.pop(out) == 0);
    CHECK(buffer.size() == 0);
}

TEST_CASE("samples come out in the order they went in") {
    RingBuffer buffer(8);
    const vector<float> in = {1, 2, 3};
    CHECK(buffer.push(in) == 3);
    CHECK(buffer.size() == 3);

    vector<float> out(8);
    REQUIRE(buffer.pop(out) == 3);
    CHECK(vector<float>(out.begin(), out.begin() + 3) == in);
}

TEST_CASE("reads and writes wrap around the end of the storage") {
    RingBuffer buffer(4);
    vector<float> out(4);
    buffer.push(vector<float>{1, 2, 3});
    REQUIRE(buffer.pop(span(out).first(2)) == 2);
    REQUIRE(buffer.push(vector<float>{4, 5, 6}) == 3);

    REQUIRE(buffer.pop(out) == 4);
    CHECK(out == vector<float>{3, 4, 5, 6});
}

TEST_CASE("a full buffer drops what does not fit instead of blocking") {
    RingBuffer buffer(4);
    CHECK(buffer.push(vector<float>{1, 2, 3, 4, 5, 6}) == 4);

    vector<float> out(8);
    REQUIRE(buffer.pop(out) == 4);
    CHECK(vector<float>(out.begin(), out.begin() + 4) == vector<float>{1, 2, 3, 4});
}

TEST_CASE("one producer and one consumer thread pass every sample in order") {
    constexpr int kSampleCount = 200'000;
    RingBuffer buffer(1024);

    thread producer([&buffer] {
        int next = 0;
        while (next < kSampleCount) {
            // Chunk sizes cycle through 1..299 so writes land at every offset and wrap often.
            const int chunk_size = min((next % 299) + 1, kSampleCount - next);
            vector<float> chunk(static_cast<size_t>(chunk_size));
            iota(chunk.begin(), chunk.end(), static_cast<float>(next));
            size_t written = 0;
            while (written < chunk.size()) {
                written += buffer.push(span(chunk).subspan(written));
            }
            next += chunk_size;
        }
    });

    vector<float> received;
    received.reserve(kSampleCount);
    vector<float> out(257);
    while (received.size() < kSampleCount) {
        const size_t count = buffer.pop(out);
        received.insert(received.end(), out.begin(), out.begin() + static_cast<ptrdiff_t>(count));
    }
    producer.join();

    bool in_order = true;
    for (size_t i = 0; i < received.size(); ++i) {
        in_order = in_order && received[i] == static_cast<float>(i);
    }
    CHECK(in_order);
}
