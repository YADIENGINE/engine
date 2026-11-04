#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include "Utilities/FloatRect.h"

enum class UiMove { Up, Down, Left, Right };

// Which of 'candidates' a cursor at 'from' moves to. It prefers what lies straight ahead, nearest first. With 'wrap',
// when nothing is ahead it continues from the far side, in the same row or column when there is one.
[[nodiscard]] std::optional<std::size_t> FindUiFocusTarget(std::span<const FloatRect> candidates, const FloatRect& from,
                                                           UiMove move, bool wrap);
// The candidate nearest to 'from': where a cursor goes when its element disappears.
[[nodiscard]] std::optional<std::size_t> FindNearestUiFocus(std::span<const FloatRect> candidates,
                                                            const FloatRect& from);
