#pragma once

#include <cstdio>
#include <whisper.h>

namespace classcaption {

// whisper.cpp logs every load step and every VAD call. Only errors are worth a user's attention.
inline void print_whisper_errors_only(ggml_log_level level, const char* text, void* /*unused*/) {
    if (level == GGML_LOG_LEVEL_ERROR) {
        std::fputs(text, stderr);
    }
}

} // namespace classcaption
