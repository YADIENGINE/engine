#pragma once

// One visitor made of several lambdas, for std::visit over a variant such as a simulation's events:
//     std::visit(Overloaded{[&](const Hit& hit) { ... }, [&](const Death& death) { ... }}, happened.event);
// A missing alternative does not compile, so a new event type shows every place that must handle it.
template <class... Functions> struct Overloaded : Functions... {
    using Functions::operator()...;
};
template <class... Functions> Overloaded(Functions...) -> Overloaded<Functions...>;
