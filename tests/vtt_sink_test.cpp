#include "classcaption/vtt_sink.h"
#include "fixtures.h"

#include <doctest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

vector<Word> timed(const vector<string>& texts, chrono::milliseconds start) {
    vector<Word> out;
    for (const string& text : texts) {
        out.push_back({.text = text, .start = start, .end = start + 250ms});
        start += 250ms;
    }
    return out;
}

string cue_lines(const filesystem::path& path) {
    ifstream file(path);
    string line;
    string cues;
    while (getline(file, line)) {
        if (line.find("-->") != string::npos) {
            string text;
            getline(file, text);
            cues.append(line).append(" | ").append(text).append("\n");
        }
    }
    return cues;
}

} // namespace

TEST_CASE("a sentence end closes a cue") {
    const auto path = fixtures::temp_path("sentences.vtt");
    {
        VttSink sink(path);
        sink.on_final(timed({"Good", "morning.", "Today", "we"}, 0ms));
        sink.on_final(timed({"start."}, 1'000ms));
    }
    CHECK(cue_lines(path) == "00:00:00.000 --> 00:00:00.500 | Good morning.\n"
                             "00:00:00.500 --> 00:00:01.250 | Today we start.\n");
}

TEST_CASE("a pause closes a cue even mid-sentence") {
    const auto path = fixtures::temp_path("pause.vtt");
    {
        VttSink sink(path);
        sink.on_final(timed({"so", "the", "answer", "is"}, 0ms));
        sink.on_pause();
        sink.on_pause();
    }
    CHECK(cue_lines(path) == "00:00:00.000 --> 00:00:01.000 | so the answer is\n");
}

TEST_CASE("long run-on speech is split so a cue fits on screen") {
    const auto path = fixtures::temp_path("long.vtt");
    {
        VttSink sink(path);
        sink.on_final(timed(vector<string>(20, "word"), 0ms));
        sink.on_pause();
    }
    const string cues = cue_lines(path);
    CHECK(cues.find("00:00:00.000 --> 00:00:03.500 | word word word word word word word word word "
                    "word word word word word\n") != string::npos);
    CHECK(cues.find("00:00:03.500 --> 00:00:05.000 | word word word word word word\n") !=
          string::npos);
}

TEST_CASE("drafts never reach the file") {
    const auto path = fixtures::temp_path("drafts.vtt");
    {
        VttSink sink(path);
        sink.on_draft(timed({"maybe", "this"}, 0ms));
        sink.on_pause();
    }
    CHECK(cue_lines(path).empty());
}
