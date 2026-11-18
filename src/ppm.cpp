#include "classcaption/ppm.h"

#include <format>
#include <stdexcept>

using namespace std;

namespace classcaption {

string ppm_image(int width, int height, span<const uint8_t> rgba_bottom_up) {
    const auto row_bytes = static_cast<size_t>(width) * 4;
    if (width <= 0 || height <= 0 ||
        rgba_bottom_up.size() != row_bytes * static_cast<size_t>(height)) {
        throw invalid_argument(format("expected {}x{} RGBA pixels, got {} bytes", width, height,
                                      rgba_bottom_up.size()));
    }
    string image = format("P6\n{} {}\n255\n", width, height);
    image.reserve(image.size() + (row_bytes / 4 * 3 * static_cast<size_t>(height)));
    for (int row = height - 1; row >= 0; --row) {
        const auto pixels = rgba_bottom_up.subspan(static_cast<size_t>(row) * row_bytes, row_bytes);
        for (size_t i = 0; i < pixels.size(); i += 4) {
            image += static_cast<char>(pixels[i]);
            image += static_cast<char>(pixels[i + 1]);
            image += static_cast<char>(pixels[i + 2]);
        }
    }
    return image;
}

} // namespace classcaption
