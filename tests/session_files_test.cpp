#include "classcaption/session_files.h"
#include "fixtures.h"

#include <ctime>
#include <doctest.h>
#include <fstream>

using namespace std;
using namespace classcaption;

namespace {

tm nov_17_2026_at_0905() {
    tm time{};
    time.tm_year = 2026 - 1900;
    time.tm_mon = 10;
    time.tm_mday = 17;
    time.tm_hour = 9;
    time.tm_min = 5;
    return time;
}

} // namespace

TEST_CASE("session files are named after the local start time") {
    const auto dir = fixtures::temp_path("sessions");
    const SessionFiles files = session_files(dir, nov_17_2026_at_0905());
    CHECK(files.captions == dir / "lecture-2026-11-17-0905.vtt");
    CHECK(files.audio == dir / "lecture-2026-11-17-0905.wav");
    CHECK(files.final_captions == dir / "lecture-2026-11-17-0905.final.vtt");
}

TEST_CASE("a second session in the same minute never overwrites the first") {
    const auto dir = fixtures::temp_path("sessions_clash");
    filesystem::create_directories(dir);
    ofstream(dir / "lecture-2026-11-17-0905.vtt") << "WEBVTT\n";
    ofstream(dir / "lecture-2026-11-17-0905-2.wav") << "";

    const SessionFiles files = session_files(dir, nov_17_2026_at_0905());
    CHECK(files.captions == dir / "lecture-2026-11-17-0905-3.vtt");
    CHECK(files.audio == dir / "lecture-2026-11-17-0905-3.wav");
}

TEST_CASE("final captions sit next to the recording they come from") {
    CHECK(final_captions_for("lectures/lecture-2026-11-17-0905.wav") ==
          "lectures/lecture-2026-11-17-0905.final.vtt");
}
