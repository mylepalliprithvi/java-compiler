#include <doctest/doctest.h>

#include <string>

#include "core/Version.hpp"

TEST_CASE("version string is non-empty") {
    CHECK(std::string(jc::version()).size() > 0);
}
