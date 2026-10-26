#pragma once
#include <span>
#include <vector>
#include "Utilities/Float2.h"
#include "Utilities/Rect.h"

// Bir dikdörtgenin (cam, ekran) kırılma deseni: her tohum noktasının kırığı dikdörtgende ona en yakın noktalardır
// (Voronoi). Kırıklar dışbükeydir ve dikdörtgeni boşluksuz kaplar; iki kırığın ortak kenarı bir çatlaktır, dikdörtgenin
// kenarları çatlak değildir. Spread çatlakların hangi anda, hangi uçtan açıldığını verir. Saf geometri; çizim
// kullanan tarafındır.
struct ShatterShard {
    std::vector<Float2> outline; // Dışbükey, köşeler sırayla; tohum dikdörtgenin dışındaysa ya da tekrarsa boş
    Float2 seed;
    Float2 center; // Ağırlık merkezi
    float area = 0.0f;
    float crackedAt = 0.0f; // Kenarındaki ilk çatlağın açılmaya başladığı an (Spread)
    float freedAt = 0.0f;   // Bütün çatlaklarının açıldığı an: kırık serbest kalır (Spread)
};

struct ShatterCrack {
    Float2 from; // Spread'den sonra: çatlağın açılmaya başladığı uç
    Float2 to;
    float length = 0.0f;
    int left = 0; // Ayırdığı iki kırığın indeksi
    int right = 0;
    float start = 0.0f; // 'from'dan açılmaya başladığı an (Spread)
    float end = 0.0f;   // 'to'ya vardığı an
};

// Çatlak dalgasının kaynağı: 'time' anında 'point'ten başlar.
struct CrackSource {
    Float2 point;
    float time = 0.0f;
};

class ShatterPattern {
public:
    // Kırık indeksleri tohum indeksleriyle aynıdır.
    ShatterPattern(const Rect& bounds, std::span<const Float2> seeds);

    // Kaynaklardan 'speed' (birim/s, > 0) hızla yayılan dalga: her çatlak dalganın önce değdiği ucundan açılmaya
    // başlar ve aynı hızla öbür ucuna ilerler. Kaynak yoksa hepsi 0 anında açıktır.
    void Spread(std::span<const CrackSource> sources, float speed);

    [[nodiscard]] const std::vector<ShatterShard>& Shards() const { return _shards; }
    [[nodiscard]] const std::vector<ShatterCrack>& Cracks() const { return _cracks; }
    // 'point'in düştüğü kırık (en yakın tohumunki); hiç kırık yoksa -1.
    [[nodiscard]] int ShardAt(Float2 point) const;

private:
    std::vector<ShatterShard> _shards;
    std::vector<ShatterCrack> _cracks;
};
