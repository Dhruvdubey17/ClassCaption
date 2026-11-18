#include "classcaption/caption_log.h"

#include <doctest.h>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

vector<Word> words(const vector<string>& texts) {
    vector<Word> out;
    out.reserve(texts.size());
    for (const string& text : texts) {
        out.push_back({.text = text, .start = {}, .end = {}});
    }
    return out;
}

} // namespace

TEST_CASE("final words build up the current line and a pause starts the next") {
    CaptionLog log;
    log.on_final(words({"Good", "morning."}));
    log.on_final(words({"Let's", "begin."}));
    log.on_pause();
    log.on_final(words({"First,", "vectors."}));
    CHECK(log.snapshot().lines == vector<string>{"Good morning. Let's begin.", "First, vectors."});
}

TEST_CASE("the draft is replaced on every pass and cleared by an empty one") {
    CaptionLog log;
    log.on_draft(words({"what", "is"}));
    CHECK(log.snapshot().draft == "what is");
    log.on_draft(words({"what", "is", "a", "basis"}));
    CHECK(log.snapshot().draft == "what is a basis");
    log.on_draft({});
    CHECK(log.snapshot().draft.empty());
}

TEST_CASE("pauses with nothing said do not leave empty lines") {
    CaptionLog log;
    log.on_pause();
    log.on_pause();
    log.on_final(words({"hello"}));
    log.on_pause();
    log.on_pause();
    CHECK(log.snapshot().lines == vector<string>{"hello"});
}

TEST_CASE("the window can read while the pipeline writes") {
    CaptionLog log;
    thread writer([&log] {
        for (int i = 0; i < 2'000; ++i) {
            log.on_final(words({"word"}));
            log.on_draft(words({"draft"}));
            if (i % 10 == 9) {
                log.on_pause();
            }
        }
    });
    size_t lines_seen = 0;
    for (int i = 0; i < 2'000; ++i) {
        lines_seen = max(lines_seen, log.snapshot().lines.size());
    }
    writer.join();
    CHECK(log.snapshot().lines.size() == 200);
    CHECK(lines_seen <= 200);
}

TEST_CASE("the view follows new captions only while the reader is at the bottom") {
    CHECK(keep_following(500.0F, 500.0F));
    CHECK(keep_following(498.0F, 500.0F));
    CHECK_FALSE(keep_following(300.0F, 500.0F));
    CHECK(keep_following(0.0F, 0.0F));
}
