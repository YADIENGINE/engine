#pragma once
#include <cstdint>
#include <span>

void FillSecureRandom(std::span<std::uint8_t> bytes);
