#pragma once

#include "classcaption/caption_log.h"

#include <atomic>

namespace classcaption {

// Shows the captions in `log` until the window is closed or `stop` is set. Runs on the main
// thread, because macOS only allows windows there. `finished` tells it the audio has ended.
void show_caption_window(const CaptionLog& log, const std::atomic<bool>& stop,
                         const std::atomic<bool>& finished);

} // namespace classcaption
