#pragma once
#include <algorithm>
#include <span>
#include <vector>
#include "Utilities/Float2.h"

// Flights of collected items (score, currency, eggs ...) from their collection point to the HUD counter. In-flight
// amounts are not reflected yet: the counter shows 'actual value - Pending(kind)' (with AnimatedCounter). The game
// supplies drawing and destination positions. 'Kind' selects the counter (e.g. a HUD icon enum). Header-only, no
// raylib.
template <class Kind> struct CollectFlight {
    Kind kind;
    Float2 from;
    int amount;
    float age = 0.0f; // Negative: not launched yet (delay)
};

template <class Kind> class CollectFlights {
public:
    explicit CollectFlights(float flightSeconds)
        : _flightSeconds(flightSeconds) {}

    // Launches from 'from' after 'delay' seconds.
    void Emit(Kind kind, Float2 from, int amount, float delay) { _flights.push_back({kind, from, amount, -delay}); }
    void Update(float deltaSeconds) {
        for (auto& flight : _flights)
            flight.age += deltaSeconds;
        std::erase_if(_flights, [&](const CollectFlight<Kind>& flight) { return flight.age >= _flightSeconds; });
    }
    // Total amount in flight (not yet reflected in the counter).
    [[nodiscard]] int Pending(Kind kind) const {
        int amount = 0;
        for (const auto& flight : _flights)
            if (flight.kind == kind) amount += flight.amount;
        return amount;
    }
    [[nodiscard]] std::span<const CollectFlight<Kind>> Active() const { return _flights; }
    [[nodiscard]] float FlightSeconds() const { return _flightSeconds; }

private:
    float _flightSeconds;
    std::vector<CollectFlight<Kind>> _flights;
};
