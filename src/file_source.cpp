#include "classcaption/file_source.h"

#include "classcaption/ring_buffer.h"
#include "classcaption/wav.h"

#include <algorithm>
#include <chrono>
#include <span>

using namespace std;

namespace classcaption {
namespace {

// 10 ms chunks, the same size a microphone callback typically delivers.
constexpr size_t kChunkSamples = kSampleRate / 100;
constexpr auto kChunkDuration = 10ms;

} // namespace

FileSource::FileSource(const filesystem::path& path, Pace pace)
    : samples_(read_wav(path)), pace_(pace) {}

FileSource::~FileSource() {
    stop();
}

void FileSource::start(RingBuffer& out) {
    stop();
    stop_requested_ = false;
    finished_ = false;
    thread_ = thread([this, &out] { deliver(out); });
}

void FileSource::stop() {
    stop_requested_ = true;
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool FileSource::finished() const {
    return finished_;
}

void FileSource::deliver(RingBuffer& out) {
    auto next_chunk_at = chrono::steady_clock::now();
    const span<const float> all(samples_);
    for (size_t sent = 0; sent < all.size() && !stop_requested_;) {
        if (pace_ == Pace::real_time) {
            this_thread::sleep_until(next_chunk_at);
            next_chunk_at += kChunkDuration;
        }
        const auto piece = all.subspan(sent, min(kChunkSamples, all.size() - sent));
        // Unlike the microphone, a file must not lose audio, so wait for the consumer to make room.
        for (size_t written = 0; written < piece.size() && !stop_requested_;) {
            written += out.push(piece.subspan(written));
            if (written < piece.size()) {
                this_thread::sleep_for(1ms);
            }
        }
        sent += piece.size();
    }
    finished_ = true;
}

} // namespace classcaption
