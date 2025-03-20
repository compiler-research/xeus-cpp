#include "test_header.hpp"
#include <doctest/doctest.h>
#include <string>

TEST_CASE("Test non-standard include paths")
{
    CHECK(test_ns::test_value == 42);
}