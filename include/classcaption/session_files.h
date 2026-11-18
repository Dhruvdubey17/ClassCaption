#pragma once

#include <ctime>
#include <filesystem>

namespace classcaption {

// The files one captioning session writes, all sharing a name based on when it started.
struct SessionFiles {
    std::filesystem::path captions;       // live captions, written as the lecture goes
    std::filesystem::path audio;          // the recording, for the after-class pass
    std::filesystem::path final_captions; // written by `classcaption finalize`
};

// Picks names in `dir` from the local start time, adding -2, -3, ... if a session from the same
// minute already exists, so no earlier lecture is ever overwritten.
SessionFiles session_files(const std::filesystem::path& dir, const std::tm& local_start);

std::filesystem::path final_captions_for(const std::filesystem::path& audio);

} // namespace classcaption
