#include "classcaption/event_log_sink.h"
#include "fixtures.h"

#include <doctest.h>
#include <fstream>
#include <string>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

vector<string> lines_of(const filesystem::path& path) {
    ifstream file(path);
    vector<string> lines;
    for (string line; getline(file, line);) {
        lines.push_back(line);
    }
    return lines;
}

} // namespace

TEST_CASE("every draft and final word is logged with when it appeared") {
    const auto path = fixtures::temp_path("events.tsv");
    {
        EventLogSink sink(path);
        sink.mark_start();
        const vector<Word> draft = {{.text = "ask", .start = 3'000ms, .end = 3'400ms}};
        const vector<Word> final = {{.text = "Ask", .start = 3'000ms, .end = 3'380ms},
                                    {.text = "not!", .start = 3'380ms, .end = 4'100ms}};
        sink.on_draft(draft);
        sink.on_final(final);
        sink.on_pause();
    }
    const vector<string> lines = lines_of(path);
    REQUIRE(lines.size() == 5);
    CHECK(lines[0] == "wall_ms\tkind\tstart_ms\tend_ms\ttext");
    CHECK(lines[1].ends_with("\tdraft\t3000\t3400\task"));
    CHECK(lines[2].ends_with("\tfinal\t3000\t3380\tAsk"));
    CHECK(lines[3].ends_with("\tfinal\t3380\t4100\tnot!"));
    CHECK(lines[4].ends_with("\tpause\t\t\t"));
    const long wall_ms = stol(lines[1].substr(0, lines[1].find('\t')));
    CHECK(wall_ms >= 0);
    CHECK(wall_ms < 1'000);
}
