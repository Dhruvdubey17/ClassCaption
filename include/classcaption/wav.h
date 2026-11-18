#pragma once

#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <vector>

namespace classcaption {

class WavError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Reads a 16 kHz mono 16-bit PCM WAV file as samples in [-1, 1].
std::vector<float> read_wav(const std::filesystem::path& path);

// Writes 16 kHz mono 16-bit PCM. The header is rewritten after every append, so the file on disk
// is always a valid recording of everything appended so far, even if the app is killed.
class WavWriter {
  public:
    explicit WavWriter(const std::filesystem::path& path);
    void append(std::span<const float> samples);

  private:
    void write_header();

    std::ofstream file_;
    std::uint32_t sample_count_ = 0;
};

} // namespace classcaption
