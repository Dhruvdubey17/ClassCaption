#include "classcaption/session_files.h"

#include <format>
#include <string>

using namespace std;

namespace classcaption {
namespace {

SessionFiles files_for(const filesystem::path& dir, const string& stem) {
    return {.captions = dir / (stem + ".vtt"),
            .audio = dir / (stem + ".wav"),
            .final_captions = dir / (stem + ".final.vtt")};
}

bool any_exists(const SessionFiles& files) {
    return filesystem::exists(files.captions) || filesystem::exists(files.audio) ||
           filesystem::exists(files.final_captions);
}

} // namespace

SessionFiles session_files(const filesystem::path& dir, const tm& local_start) {
    const string stem = format("lecture-{:04}-{:02}-{:02}-{:02}{:02}", local_start.tm_year + 1900,
                               local_start.tm_mon + 1, local_start.tm_mday, local_start.tm_hour,
                               local_start.tm_min);
    SessionFiles files = files_for(dir, stem);
    for (int copy = 2; any_exists(files); ++copy) {
        files = files_for(dir, format("{}-{}", stem, copy));
    }
    return files;
}

filesystem::path final_captions_for(const filesystem::path& audio) {
    return filesystem::path(audio).replace_extension(".final.vtt");
}

} // namespace classcaption
