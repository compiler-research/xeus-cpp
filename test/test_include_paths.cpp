#include <doctest/doctest.h>
#include <string>
#include "test_header.hpp"

TEST_CASE("Test non-standard include paths")
{
    CHECK(test_ns::test_value == 42);
}
