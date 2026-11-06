#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

TEST_CASE("Probe.Fatal") {
    volatile int* invalid = nullptr;
    *invalid = 17;
}
TEST_CASE("Probe.Expected") {
    CHECK_THROWS_AS(throw std::runtime_error("expected"), std::runtime_error);
    CHECK(1 == 1);
}
