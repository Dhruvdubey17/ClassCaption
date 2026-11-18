#include "classcaption/wav.h"

#include "classcaption/audio_source.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <iterator>
#include <string_view>

using namespace std;

namespace classcaption {
namespace {

constexpr uint16_t kPcmFormat = 1;
constexpr uint16_t kBitsPerSample = 16;
constexpr uint32_t kBytesPerSample = kBitsPerSample / 8;
constexpr float kInt16Scale = 32'768.0F;
constexpr uint32_t kHeaderSize = 44;

// WAV is little-endian, as is every Mac this runs on, so fields copy straight out of the bytes.
template <typename T> T read_field(span<const char> bytes, size_t offset) {
    if (offset + sizeof(T) > bytes.size()) {
        throw WavError("file ends in the middle of a header");
    }
    T value{};
    memcpy(&value, bytes.subspan(offset, sizeof(T)).data(), sizeof(T));
    return value;
}

string_view read_tag(span<const char> bytes, size_t offset) {
    if (offset + 4 > bytes.size()) {
        return {};
    }
    return {bytes.subspan(offset, 4).data(), 4};
}

void check_format(span<const char> fmt) {
    if (read_field<uint16_t>(fmt, 0) != kPcmFormat ||
        read_field<uint16_t>(fmt, 14) != kBitsPerSample) {
        throw WavError("audio must be 16-bit PCM");
    }
    if (read_field<uint16_t>(fmt, 2) != 1) {
        throw WavError("audio must be mono");
    }
    if (read_field<uint32_t>(fmt, 4) != kSampleRate) {
        throw WavError("audio must be 16000 Hz");
    }
}

vector<float> decode_samples(span<const char> data) {
    vector<float> samples(data.size() / kBytesPerSample);
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] =
            static_cast<float>(read_field<int16_t>(data, i * kBytesPerSample)) / kInt16Scale;
    }
    return samples;
}

template <typename T> void write_field(ofstream& file, T value) {
    array<char, sizeof(T)> bytes{};
    memcpy(bytes.data(), &value, sizeof(T));
    file.write(bytes.data(), bytes.size());
}

int16_t to_int16(float sample) {
    return static_cast<int16_t>(lround(clamp(sample, -1.0F, 1.0F) * (kInt16Scale - 1)));
}

} // namespace

vector<float> read_wav(const filesystem::path& path) {
    ifstream file(path, ios::binary);
    if (!file) {
        throw WavError(format("cannot open {}", path.string()));
    }
    const vector<char> bytes{istreambuf_iterator<char>(file), istreambuf_iterator<char>()};
    if (read_tag(bytes, 0) != "RIFF" || read_tag(bytes, 8) != "WAVE") {
        throw WavError(format("{} is not a WAV file", path.string()));
    }

    bool format_checked = false;
    // Chunks after the 12-byte RIFF header are tag, size, body, padded to an even length.
    for (size_t offset = 12; offset + 8 <= bytes.size();) {
        const string_view tag = read_tag(bytes, offset);
        const auto size = read_field<uint32_t>(bytes, offset + 4);
        const size_t body = offset + 8;
        const size_t available = min<size_t>(size, bytes.size() - body);
        const span<const char> chunk = span(bytes).subspan(body, available);

        if (tag == "fmt ") {
            check_format(chunk);
            format_checked = true;
        } else if (tag == "data") {
            if (!format_checked) {
                throw WavError(format("{} has audio data before its format", path.string()));
            }
            return decode_samples(chunk);
        }
        offset = body + size + (size % 2);
    }
    throw WavError(format("{} has no audio data", path.string()));
}

WavWriter::WavWriter(const filesystem::path& path) : file_(path, ios::binary | ios::trunc) {
    if (!file_) {
        throw WavError(format("cannot create {}", path.string()));
    }
    write_header();
}

void WavWriter::append(span<const float> samples) {
    file_.seekp(0, ios::end);
    for (const float sample : samples) {
        write_field(file_, to_int16(sample));
    }
    sample_count_ += static_cast<uint32_t>(samples.size());
    write_header();
}

void WavWriter::write_header() {
    const uint32_t data_size = sample_count_ * kBytesPerSample;
    file_.seekp(0);
    file_.write("RIFF", 4);
    write_field(file_, kHeaderSize - 8 + data_size);
    file_.write("WAVEfmt ", 8);
    write_field<uint32_t>(file_, 16);
    write_field(file_, kPcmFormat);
    write_field<uint16_t>(file_, 1);
    write_field<uint32_t>(file_, kSampleRate);
    write_field(file_, kSampleRate * kBytesPerSample);
    write_field(file_, static_cast<uint16_t>(kBytesPerSample));
    write_field(file_, kBitsPerSample);
    file_.write("data", 4);
    write_field(file_, data_size);
    file_.flush();
    if (!file_) {
        throw WavError("could not write audio to disk");
    }
}

} // namespace classcaption
