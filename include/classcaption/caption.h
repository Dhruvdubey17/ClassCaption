#pragma once

#include <chrono>
#include <string>

namespace classcaption {

// A finished line of captions. Times count from the start of the lecture.
struct Caption {
    std::chrono::milliseconds start{};
    std::chrono::milliseconds end{};
    std::string text;
};

} // namespace classcaption
