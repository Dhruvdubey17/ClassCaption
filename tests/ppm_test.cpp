#include "classcaption/ppm.h"

#include <cstdint>
#include <doctest.h>
#include <string>
#include <vector>

using namespace std;
using namespace classcaption;

TEST_CASE("a frame read from OpenGL becomes a top-down RGB image") {
    // OpenGL hands rows back bottom row first, as RGBA.
    const vector<uint8_t> bottom_up = {
        1, 2, 3, 255, 4,  5,  6,  255, // bottom row
        7, 8, 9, 255, 10, 11, 12, 255, // top row
    };
    const string image = ppm_image(2, 2, bottom_up);
    const string header = "P6\n2 2\n255\n";
    REQUIRE(image.size() == header.size() + 12);
    CHECK(image.starts_with(header));
    const vector<uint8_t> pixels(image.begin() + static_cast<ptrdiff_t>(header.size()),
                                 image.end());
    CHECK(pixels == vector<uint8_t>{7, 8, 9, 10, 11, 12, 1, 2, 3, 4, 5, 6});
}

TEST_CASE("a frame with the wrong number of bytes is rejected") {
    CHECK_THROWS_AS(ppm_image(2, 2, vector<uint8_t>(15)), invalid_argument);
}
