#pragma once
#include <raylib.h>
#include <functional>

// Düzgün yarı saydam gölge: 'shapes' gölgenin parçalarını verilen renkle çizer; üst üste binen parçalar iki kez
// koyulaşmaz. Hedefin alfa kanalını maske olarak kullanır, bu yüzden 'shapes' iki kez çağrılır (önce maske, sonra
// karartma); parçaların dışındaki piksellere dokunulmaz. 'darkness' 0 (gölge yok) .. 1 (simsiyah).
void DrawMergedShadow(float darkness, const std::function<void(Color)>& shapes);
