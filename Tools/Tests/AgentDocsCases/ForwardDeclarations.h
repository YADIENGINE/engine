#pragma once
class ForwardClass;
struct ForwardStruct;
template <class T> class ForwardTemplate;
class [[nodiscard]] AttributedForward;

// A defined class stays in the index.
class Defined {
public:
    struct NestedForward;
    struct Nested {
        int value = 0;
    };
};
