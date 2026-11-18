#include "classcaption/vtt_sink.h"

#include <string>

using namespace std;

namespace classcaption {
namespace {

// About two lines of a typical video player at a readable font size.
constexpr size_t kMaxCueWords = 14;

bool ends_sentence(const Word& word) {
    return !word.text.empty() && string_view(".?!").find(word.text.back()) != string_view::npos;
}

} // namespace

VttSink::VttSink(const filesystem::path& path) : writer_(path) {}

void VttSink::on_final(span<const Word> words) {
    for (const Word& word : words) {
        pending_.push_back(word);
        if (ends_sentence(word) || pending_.size() >= kMaxCueWords) {
            write_cue();
        }
    }
}

void VttSink::on_pause() {
    write_cue();
}

void VttSink::write_cue() {
    if (pending_.empty()) {
        return;
    }
    Caption caption{.start = pending_.front().start, .end = pending_.back().end, .text = ""};
    for (const Word& word : pending_) {
        caption.text += (caption.text.empty() ? "" : " ") + word.text;
    }
    writer_.write(caption);
    pending_.clear();
}

} // namespace classcaption
