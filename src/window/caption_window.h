#pragma once

#include "classcaption/caption_log.h"

#include <atomic>
#include <filesystem>

namespace classcaption {

// Shows the captions in `log` until the window is closed or `stop` is set. Runs on the main
// thread, because macOS only allows windows there. `finished` tells it the audio has ended. With
// a non-empty `frames_dir`, it saves frames there and closes once the audio has ended.
void show_caption_window(const CaptionLog& log, const std::atomic<bool>& stop,
                         const std::atomic<bool>& finished,
                         const std::filesystem::path& frames_dir);

} // namespace classcaption
