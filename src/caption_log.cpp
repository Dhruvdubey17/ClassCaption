#include "classcaption/caption_log.h"

using namespace std;

namespace classcaption {
namespace {

// Within this many pixels of the bottom counts as at the bottom; scroll positions are fractional.
constexpr float kBottomSlack = 4.0F;

string joined(span<const Word> words) {
    string text;
    for (const Word& word : words) {
        if (!text.empty()) {
            text += ' ';
        }
        text += word.text;
    }
    return text;
}

} // namespace

void CaptionLog::on_final(span<const Word> words) {
    const string text = joined(words);
    const scoped_lock lock(mutex_);
    if (!line_open_) {
        lines_.emplace_back();
        line_open_ = true;
    }
    string& line = lines_.back();
    line += (line.empty() ? "" : " ");
    line += text;
}

void CaptionLog::on_draft(span<const Word> words) {
    string text = joined(words);
    const scoped_lock lock(mutex_);
    draft_ = std::move(text);
}

void CaptionLog::on_pause() {
    const scoped_lock lock(mutex_);
    line_open_ = false;
}

CaptionLog::Snapshot CaptionLog::snapshot() const {
    const scoped_lock lock(mutex_);
    return {.lines = lines_, .draft = draft_};
}

bool keep_following(float scroll_y, float scroll_max) {
    return scroll_y >= scroll_max - kBottomSlack;
}

} // namespace classcaption
