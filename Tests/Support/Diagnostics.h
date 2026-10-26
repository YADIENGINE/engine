#pragma once
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>
#include <string>
#include "Utilities/Vec2.h"

template <> struct Catch::StringMaker<Vec2> {
    static std::string convert(Vec2 value) {
        return "(" + std::to_string(value.x) + ", " + std::to_string(value.y) + ")";
    }
};
