#include "classcaption/stabilizer.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

using namespace std;

namespace classcaption {
namespace {

// "Hello," and "hello" are the same word for agreement purposes.
string comparable(string_view word) {
    string out;
    for (const char c : word) {
        if (isalnum(static_cast<unsigned char>(c)) != 0) {
            out += static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
    }
    return out;
}

bool same_word(const Word& a, const Word& b) {
    return comparable(a.text) == comparable(b.text);
}

chrono::milliseconds midpoint(const Word& word) {
    return word.start + ((word.end - word.start) / 2);
}

} // namespace

vector<Word> Stabilizer::unfinalized(const vector<Word>& hypothesis) const {
    // Matched by time, not by position: a later pass may split or merge earlier words, which
    // shifts positions but not when the words were spoken.
    vector<Word> rest;
    ranges::copy_if(hypothesis, back_inserter(rest),
                    [this](const Word& word) { return midpoint(word) > finalized_until_; });
    return rest;
}

Stabilizer::Update Stabilizer::update(const vector<Word>& hypothesis) {
    const vector<Word> before = unfinalized(previous_);
    vector<Word> now = unfinalized(hypothesis);
    previous_ = hypothesis;

    const auto [agreed_end, ignored] = ranges::mismatch(now, before, same_word);
    Update result;
    result.finalized.assign(now.begin(), agreed_end);
    result.draft.assign(agreed_end, now.end());
    if (!result.finalized.empty()) {
        finalized_until_ = result.finalized.back().end;
    }
    return result;
}

vector<Word> Stabilizer::finish_utterance() {
    vector<Word> rest = unfinalized(previous_);
    previous_.clear();
    finalized_until_ = {};
    return rest;
}

} // namespace classcaption
