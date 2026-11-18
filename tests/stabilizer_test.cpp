#include "classcaption/stabilizer.h"

#include <doctest.h>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace classcaption;

namespace {

// Builds a hypothesis with each word lasting 300 ms, starting at 0.
vector<Word> words(const string& text) {
    vector<Word> out;
    istringstream in(text);
    chrono::milliseconds start{0};
    for (string w; in >> w; start += 300ms) {
        out.push_back({.text = w, .start = start, .end = start + 300ms});
    }
    return out;
}

string text_of(const vector<Word>& ws) {
    string out;
    for (const Word& w : ws) {
        out += (out.empty() ? "" : " ") + w.text;
    }
    return out;
}

} // namespace

TEST_CASE("the first pass is all draft") {
    Stabilizer stabilizer;
    const auto update = stabilizer.update(words("today we cover"));
    CHECK(update.finalized.empty());
    CHECK(text_of(update.draft) == "today we cover");
}

TEST_CASE("words two passes agree on become final, the rest stays draft") {
    Stabilizer stabilizer;
    stabilizer.update(words("today we cover"));
    const auto update = stabilizer.update(words("today we discuss eigen"));
    CHECK(text_of(update.finalized) == "today we");
    CHECK(text_of(update.draft) == "discuss eigen");
}

TEST_CASE("agreement ignores case and punctuation and keeps the newer spelling") {
    Stabilizer stabilizer;
    stabilizer.update(words("hello everyone so"));
    const auto update = stabilizer.update(words("Hello, everyone. So"));
    CHECK(text_of(update.finalized) == "Hello, everyone. So");
}

TEST_CASE("final words never change and are never repeated") {
    Stabilizer stabilizer;
    stabilizer.update(words("today we cover"));
    stabilizer.update(words("today we discuss"));
    // A later pass rewrites the first two words. They are already final, so they are ignored and
    // only the words after them are compared.
    CHECK(text_of(stabilizer.update(words("Today, we'd discuss matrices")).finalized) == "discuss");
    const auto update = stabilizer.update(words("Today, we'd discuss matrices today"));
    CHECK(text_of(update.finalized) == "matrices");
    CHECK(text_of(update.draft) == "today");
}

TEST_CASE("the same hypothesis twice does not finalize words twice") {
    Stabilizer stabilizer;
    stabilizer.update(words("a b c"));
    CHECK(text_of(stabilizer.update(words("a b c")).finalized) == "a b c");
    CHECK(stabilizer.update(words("a b c")).finalized.empty());
}

TEST_CASE("a pause finalizes the rest of the latest pass and starts a fresh utterance") {
    Stabilizer stabilizer;
    stabilizer.update(words("today we cover"));
    stabilizer.update(words("today we discuss eigen values"));
    CHECK(text_of(stabilizer.finish_utterance()) == "discuss eigen values");

    // The next utterance starts over: its first pass is draft again.
    const auto update = stabilizer.update(words("next topic"));
    CHECK(update.finalized.empty());
    CHECK(text_of(update.draft) == "next topic");
}

TEST_CASE("finishing an utterance with nothing pending gives nothing") {
    Stabilizer stabilizer;
    CHECK(stabilizer.finish_utterance().empty());
}
