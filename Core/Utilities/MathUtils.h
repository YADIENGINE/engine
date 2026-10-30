#pragma once
#include <cmath>
#include <cstddef>

// View'ların ortak sayı yardımcıları (raylib'siz). Header-only.
inline constexpr float Pi = 3.14159265f;

// a'dan b'ye 't' oranında doğrusal geçiş ('t' kırpılmaz). raymath'in Lerp'iyle çakışmasın diye Mix.
constexpr float Mix(float a, float b, float t) { return a + (b - a) * t; }

// Ondalık kısım (0..1).
inline float Fract(float value) { return value - std::floor(value); }

// Tohumdan sabit sözde-rastgele sayı: aynı tohum her karede aynı sonucu verir (titremeyen süsler için).
constexpr unsigned Hash(unsigned value) {
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}
// Tohumdan 0..1 aralığında (bin adımlı) sabit sayı.
constexpr float Random01(unsigned seed) { return static_cast<float>(Hash(seed) % 1000U) / 1000.0f; }
// İki tamsayıdan 0..1 aralığında sabit gürültü (sin tabanlı; hücre / parçacık süsleri için). Hash'ten farklı dağılır.
inline float NoiseHash(int a, int b) {
    return Fract(std::sin(static_cast<float>(a) * 12.9898f + static_cast<float>(b) * 78.233f) * 43758.547f);
}
// 'value'yu 0..count-1 aralığına sarar (eksi değerler sondan döner); count > 0.
constexpr int WrapIndex(int value, int count) { return (value % count + count) % count; }
// 'value'yu 0..count-1 aralığına kırpar ve dizi indisine çevirir (seviye tabloları için); count > 0.
constexpr std::size_t ClampIndex(int value, int count) {
    return static_cast<std::size_t>(value < 0 ? 0 : (value > count - 1 ? count - 1 : value));
}
