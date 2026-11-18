#include "classcaption/config.h"

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <format>
#include <memory>
#include <string>

using namespace std;

namespace classcaption {
namespace {

string_view trim(string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

Backend parse_backend(string_view value) {
    if (value == "coreml") {
        return Backend::coreml;
    }
    if (value == "metal") {
        return Backend::metal;
    }
    if (value == "cpu") {
        return Backend::cpu;
    }
    throw ConfigError("backend must be coreml, metal or cpu");
}

int parse_positive_int(string_view key, string_view value) {
    int number = 0;
    const char* const first = to_address(value.begin());
    const char* const end = to_address(value.end());
    const auto [stop, error] = from_chars(first, end, number);
    if (error != errc{} || stop != end) {
        throw ConfigError(format("{} must be a whole number", key));
    }
    if (number <= 0) {
        throw ConfigError(format("{} must be above 0", key));
    }
    return number;
}

float parse_fraction(string_view key, string_view value) {
    const string text(value);
    char* stop = nullptr;
    errno = 0;
    const float number = strtof(text.c_str(), &stop);
    const bool parsed_whole_text = !text.empty() && stop == text.c_str() + text.size();
    if (!parsed_whole_text || errno != 0 || number < 0.0F || number > 1.0F) {
        throw ConfigError(format("{} must be between 0 and 1", key));
    }
    return number;
}

bool parse_bool(string_view key, string_view value) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    throw ConfigError(format("{} must be true or false", key));
}

void apply_setting(Config& config, string_view key, string_view value) {
    if (key == "backend") {
        config.backend = parse_backend(value);
    } else if (key == "live_model") {
        config.live_model = value;
    } else if (key == "final_model") {
        config.final_model = value;
    } else if (key == "vad_model") {
        config.vad_model = value;
    } else if (key == "language") {
        config.language = value;
    } else if (key == "vad_threshold") {
        config.vad_threshold = parse_fraction(key, value);
    } else if (key == "step_ms") {
        config.step_ms = parse_positive_int(key, value);
    } else if (key == "output_dir") {
        config.output_dir = value;
    } else if (key == "save_audio") {
        config.save_audio = parse_bool(key, value);
    } else {
        throw ConfigError(format("unknown setting '{}'", key));
    }
}

void apply_line(Config& config, string_view line) {
    const auto equals = line.find('=');
    if (equals == string_view::npos) {
        throw ConfigError("expected key = value");
    }
    apply_setting(config, trim(line.substr(0, equals)), trim(line.substr(equals + 1)));
}

} // namespace

Config parse_config(string_view text) {
    Config config;
    int line_number = 0;
    while (!text.empty()) {
        ++line_number;
        const auto newline = text.find('\n');
        const string_view line = trim(text.substr(0, newline));
        text = newline == string_view::npos ? string_view{} : text.substr(newline + 1);

        if (line.empty() || line.starts_with('#')) {
            continue;
        }
        try {
            apply_line(config, line);
        } catch (const ConfigError& error) {
            throw ConfigError(format("line {}: {}", line_number, error.what()));
        }
    }
    return config;
}

Config apply_args(Config config, span<const string> args) {
    for (size_t i = 0; i < args.size(); ++i) {
        const string_view arg = args[i];
        if (!arg.starts_with("--")) {
            throw ConfigError(format("unexpected argument '{}'", arg));
        }
        const string_view flag = arg.substr(2);
        const auto equals = flag.find('=');
        if (equals != string_view::npos) {
            apply_setting(config, flag.substr(0, equals), flag.substr(equals + 1));
            continue;
        }
        if (i + 1 == args.size()) {
            throw ConfigError(format("{} needs a value", arg));
        }
        apply_setting(config, flag, args[++i]);
    }
    return config;
}

} // namespace classcaption
