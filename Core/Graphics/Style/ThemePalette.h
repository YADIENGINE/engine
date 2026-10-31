#pragma once
#include "Utilities/Color4.h"

// Temanın rol renkleri: arayüzün her yerinde aynı anlamla kullanılır. Oyuna özgü vurgu renkleri (nesne, kademe,
// kategori renkleri) oyunun kendi paletindedir.
struct ThemePalette {
    Color4 background{0, 0, 0, 255};         // Ekran zemini
    Color4 surface{0, 0, 0, 255};            // Panel, kart, önizleme gövdesi
    Color4 surfaceFrame{130, 130, 130, 255}; // Seçili olmayan / kilitli panel çerçevesi
    Color4 text{255, 255, 255, 255};         // Ana yazı
    Color4 textSoft{200, 200, 200, 255};     // İkincil yazı: koyu yüzeyde okunaklı
    Color4 textDim{130, 130, 130, 255};      // Sönük yazı, ipucu
    Color4 overlay{0, 0, 0, 0};              // Ekranı karartan perde
    Color4 accent{255, 255, 255, 255};       // Birincil vurgu (onay, ilerleme)
    Color4 warning{230, 41, 55, 255};        // Tehlike, uyarı
};
