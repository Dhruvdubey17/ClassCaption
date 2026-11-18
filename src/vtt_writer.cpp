#include "classcaption/vtt_writer.h"

#include <format>
#include <stdexcept>

using namespace std;

namespace classcaption {
namespace {

// A blank line ends a cue and "-->" marks a timing line, so text must contain neither.
string escape_cue_text(string_view text) {
    string escaped;
    escaped.reserve(text.size());
    for (const char c : text) {
        switch (c) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '\n':
        case '\r':
            if (!escaped.empty() && escaped.back() != ' ') {
                escaped += ' ';
            }
            break;
        default:
            escaped += c;
        }
    }
    return escaped;
}

} // namespace

string vtt_timestamp(chrono::milliseconds time) {
    const auto hours = chrono::duration_cast<chrono::hours>(time);
    const auto minutes = chrono::duration_cast<chrono::minutes>(time - hours);
    const auto seconds = chrono::duration_cast<chrono::seconds>(time - hours - minutes);
    const auto millis = time - hours - minutes - seconds;
    return format("{:02}:{:02}:{:02}.{:03}", hours.count(), minutes.count(), seconds.count(),
                  millis.count());
}

VttWriter::VttWriter(const filesystem::path& path) : file_(path, ios::trunc) {
    if (!file_) {
        throw runtime_error(format("cannot create {}", path.string()));
    }
    file_ << "WEBVTT\n\n" << flush;
}

void VttWriter::write(const Caption& caption) {
    ++cue_count_;
    file_ << cue_count_ << '\n'
          << vtt_timestamp(caption.start) << " --> " << vtt_timestamp(caption.end) << '\n'
          << escape_cue_text(caption.text) << "\n\n"
          << flush;
}

} // namespace classcaption
