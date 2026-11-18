#pragma once

#include "classcaption/config.h"

#include <filesystem>
#include <functional>
#include <stdexcept>

namespace classcaption {

class FinalizeError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// The after-class pass: captions a saved recording utterance by utterance with config.final_model,
// which is slower than the live model but more accurate, and writes .final.vtt beside it.
// Reports progress as a percentage. Returns the path written.
std::filesystem::path finalize_recording(const Config& config,
                                         const std::filesystem::path& recording,
                                         const std::function<void(int)>& on_progress);

} // namespace classcaption
