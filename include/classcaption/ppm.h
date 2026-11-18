#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace classcaption {

// Encodes an RGBA frame read back from OpenGL (bottom row first) as a binary PPM image, the
// simplest format ffmpeg and image viewers open. Throws std::invalid_argument on a size mismatch.
std::string ppm_image(int width, int height, std::span<const std::uint8_t> rgba_bottom_up);

} // namespace classcaption
