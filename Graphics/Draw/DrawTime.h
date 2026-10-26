#pragma once

// Sunum saati (saniye): View'ların döngüsel süsleri (nabız, parıltı, dönme) buradan okur; View saat tutmaz ve
// aktarmaz. Engine her kareyi çizmeden önce Game::DrawSeconds()'tan yazar: oyunda gerçek zaman, editör worker'ında
// host'un duraklatılabilir saati. Draw / Render / Fx bunu okumaz, zamanı parametre alır.
[[nodiscard]] float DrawTime();
// Sadece Engine Runtime (Game) yazar.
void SetDrawTime(double seconds);
