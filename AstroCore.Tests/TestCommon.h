#pragma once

// AstroCore's headers expect Windows.h to have been included (SYSTEMTIME, DEFINE_ENUM_FLAG_OPERATORS).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using Catch::Approx;
