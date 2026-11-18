#include "classcaption/stabilizer.h"

#include <algorithm>
#include <cctype>
#include <span>
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

// Timing drift can bring back at most a few final words; this bounds how far back to look.
constexpr size_t kMaxRepeatWords = 5;

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

vector<Word> Stabilizer::without_repeat_of_final(vector<Word> words) const {
    // Try the longest overlap first, so "the cat the" never matches on just "the".
    for (size_t length = min({kMaxRepeatWords, recent_final_.size(), words.size()}); length > 0;
         --length) {
        const auto final_tail = span(recent_final_).last(length);
        if (ranges::equal(final_tail, span(words).first(length), same_word)) {
            words.erase(words.begin(), words.begin() + static_cast<ptrdiff_t>(length));
            return words;
        }
    }
    return words;
}

Stabilizer::Update Stabilizer::update(const vector<Word>& hypothesis) {
    // Matched by time first, then by text: Whisper's word times drift between passes, so a final
    // word can reappear just after the cutoff.
    const vector<Word> before = without_repeat_of_final(unfinalized(previous_));
    const vector<Word> now = without_repeat_of_final(unfinalized(hypothesis));
    previous_ = hypothesis;

    const auto [agreed_end, ignored] = ranges::mismatch(now, before, same_word);
    Update result;
    result.finalized.assign(now.begin(), agreed_end);
    result.draft.assign(agreed_end, now.end());
    if (!result.finalized.empty()) {
        finalized_until_ = max(finalized_until_, result.finalized.back().end);
        recent_final_.insert(recent_final_.end(), result.finalized.begin(), result.finalized.end());
        if (recent_final_.size() > kMaxRepeatWords) {
            recent_final_.erase(recent_final_.begin(),
                                recent_final_.end() - static_cast<ptrdiff_t>(kMaxRepeatWords));
        }
    }
    return result;
}

vector<Word> Stabilizer::finish_utterance() {
    vector<Word> rest = without_repeat_of_final(unfinalized(previous_));
    previous_.clear();
    finalized_until_ = {};
    recent_final_.clear();
    return rest;
}

} // namespace classcaption
