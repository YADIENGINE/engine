#pragma once
#include <algorithm>
#include <span>
#include <vector>
#include "Utilities/Float2.h"

// Toplanan şeylerin (puan, para, egg ...) toplandığı yerden HUD sayacına uçuşları. Uçuş yolda iken miktarı sayaca
// henüz yansımaz: sayaç 'gerçek değer - Pending(kind)' gösterir (AnimatedCounter ile). Çizim ve hedef konumu oyundadır.
// 'Kind' sayacı seçer (ör. oyunun HUD ikon enum'u). Header-only, raylib'siz.
template <class Kind> struct CollectFlight {
    Kind kind;
    Float2 from;
    int amount;
    float age = 0.0f; // Eksi: henüz kalkmadı (gecikme)
};

template <class Kind> class CollectFlights {
public:
    explicit CollectFlights(float flightSeconds)
        : _flightSeconds(flightSeconds) {}

    // 'delay' saniye sonra 'from'dan kalkar.
    void Emit(Kind kind, Float2 from, int amount, float delay) { _flights.push_back({kind, from, amount, -delay}); }
    void Update(float deltaSeconds) {
        for (auto& flight : _flights)
            flight.age += deltaSeconds;
        std::erase_if(_flights, [&](const CollectFlight<Kind>& flight) { return flight.age >= _flightSeconds; });
    }
    // Yolda olan (sayaca henüz yansımamış) toplam miktar.
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
