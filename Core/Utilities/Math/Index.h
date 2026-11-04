#pragma once

// Wraps 'value' into 0..count-1 (negative values wrap from the end); count > 0.
constexpr int WrapIndex(int value, int count) { return (value % count + count) % count; }
