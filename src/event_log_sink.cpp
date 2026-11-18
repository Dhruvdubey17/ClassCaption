#include "classcaption/event_log_sink.h"

#include <format>
#include <stdexcept>

using namespace std;

namespace classcaption {

EventLogSink::EventLogSink(const filesystem::path& path) : file_(path, ios::trunc) {
    if (!file_) {
        throw runtime_error(format("cannot create {}", path.string()));
    }
    file_ << "wall_ms\tkind\tstart_ms\tend_ms\ttext\n";
}

void EventLogSink::mark_start() {
    start_ = chrono::steady_clock::now();
}

void EventLogSink::on_final(span<const Word> words) {
    log("final", words);
}

void EventLogSink::on_draft(span<const Word> words) {
    log("draft", words);
}

void EventLogSink::on_pause() {
    const auto wall =
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start_);
    file_ << wall.count() << "\tpause\t\t\t\n" << flush;
}

void EventLogSink::log(string_view kind, span<const Word> words) {
    const auto wall =
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start_);
    for (const Word& word : words) {
        file_ << wall.count() << '\t' << kind << '\t' << word.start.count() << '\t'
              << word.end.count() << '\t' << word.text << '\n';
    }
    file_ << flush;
}

} // namespace classcaption
