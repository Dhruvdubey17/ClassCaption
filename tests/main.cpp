#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

TEST_CASE("test runner is wired into ctest") {
    CHECK(1 + 1 == 2);
}
