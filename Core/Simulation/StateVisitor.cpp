#include "Simulation/StateVisitor.h"

StateVisitor::~StateVisitor() = default;

void StateVisitor::Field(std::string_view name, Float2 value) {
    Group(name, [&] {
        Value("x", value.x);
        Value("y", value.y);
    });
}
void StateVisitor::Field(std::string_view name, Vec2 value) {
    Group(name, [&] {
        Value("x", static_cast<std::int64_t>(value.x));
        Value("y", static_cast<std::int64_t>(value.y));
    });
}
void StateVisitor::Field(std::string_view name, const Rng& value) {
    Group(name, [&] {
        Value("state", value.State());
        Value("increment", value.Increment());
    });
}
