#include "classcaption/vtt_writer.h"
#include "fixtures.h"

#include <doctest.h>
#include <fstream>
#include <sstream>

using namespace std;
using namespace classcaption;

namespace {

string read_file(const filesystem::path& path) {
    ifstream file(path);
    stringstream text;
    text << file.rdbuf();
    return text.str();
}

} // namespace

TEST_CASE("timestamps use the WebVTT hours:minutes:seconds.milliseconds form") {
    CHECK(vtt_timestamp(0ms) == "00:00:00.000");
    CHECK(vtt_timestamp(3'723'004ms) == "01:02:03.004");
    CHECK(vtt_timestamp(100h) == "100:00:00.000");
}

TEST_CASE("cues are numbered and written in order after the WEBVTT header") {
    const auto path = fixtures::temp_path("numbered.vtt");
    {
        VttWriter writer(path);
        writer.write({.start = 0ms, .end = 2'500ms, .text = "Good morning."});
        writer.write({.start = 2'500ms, .end = 4'000ms, .text = "Today: eigenvalues."});
    }
    CHECK(read_file(path) == "WEBVTT\n"
                             "\n"
                             "1\n"
                             "00:00:00.000 --> 00:00:02.500\n"
                             "Good morning.\n"
                             "\n"
                             "2\n"
                             "00:00:02.500 --> 00:00:04.000\n"
                             "Today: eigenvalues.\n"
                             "\n");
}

TEST_CASE("caption text cannot break the file format") {
    const auto path = fixtures::temp_path("escaped.vtt");
    {
        VttWriter writer(path);
        writer.write({.start = 0ms, .end = 1'000ms, .text = "if a < b && b > c\n\nthen --> done"});
    }
    CHECK(read_file(path).find("if a &lt; b &amp;&amp; b &gt; c then --&gt; done\n") !=
          string::npos);
}

TEST_CASE("each cue is on disk as soon as it is written") {
    const auto path = fixtures::temp_path("flushed.vtt");
    VttWriter writer(path);
    writer.write({.start = 0ms, .end = 1'000ms, .text = "first"});
    CHECK(read_file(path).find("first") != string::npos);
}
