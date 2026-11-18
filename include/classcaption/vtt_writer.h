#pragma once

#include "classcaption/caption.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace classcaption {

std::string vtt_timestamp(std::chrono::milliseconds time);

// Writes captions as a WebVTT file that video players and text editors can open. Every cue is
// flushed as it is written, so a crash mid-lecture keeps everything captioned so far.
class VttWriter {
  public:
    explicit VttWriter(const std::filesystem::path& path);
    void write(const Caption& caption);

  private:
    std::ofstream file_;
    int cue_count_ = 0;
};

} // namespace classcaption
