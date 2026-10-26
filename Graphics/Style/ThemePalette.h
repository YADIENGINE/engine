#pragma once
#include <raylib.h>

// Temanın rol renkleri: arayüzün her yerinde aynı anlamla kullanılır. Oyuna özgü vurgu renkleri (nesne, kademe,
// kategori renkleri) oyunun kendi paletindedir.
struct ThemePalette {
    Color background = BLACK;   // Ekran zemini
    Color surface = BLACK;      // Panel, kart, önizleme gövdesi
    Color surfaceFrame = GRAY;  // Seçili olmayan / kilitli panel çerçevesi
    Color text = WHITE;         // Ana yazı
    Color textSoft = LIGHTGRAY; // İkincil yazı: koyu yüzeyde okunaklı
    Color textDim = GRAY;       // Sönük yazı, ipucu
    Color overlay = BLANK;      // Ekranı karartan perde
    Color accent = WHITE;       // Birincil vurgu (onay, ilerleme)
    Color warning = RED;        // Tehlike, uyarı
};
