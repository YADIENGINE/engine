#pragma once
#include <raylib.h>
#include "Graphics/Icons/Icons.hpp"

// Eski yol (raylib); Icon Graphics/Icons/Icons.hpp'de, Painter karşılığı (IconStyle ile) da orada. Bu başlık göç
// bitince silinir.

// IconStyle'ın raylib renkli eşi: taşınmamış View'lar raylib renkleriyle çağırabilsin diye.
struct LegacyIconStyle {
    Color color;          // Şeklin rengi
    Color detail = BLANK; // İçindeki ayrıntı: hayalet gözü, tuğla çatlağı, yumurta parlaması
    float line = 2.0f;    // Çizgi ikonlarının kalınlığı
};

// 'size' yarı genişlik: ikon merkezden her yöne yaklaşık bu kadar uzanır.
void DrawIcon(Icon icon, Vector2 center, float size, const LegacyIconStyle& style);
