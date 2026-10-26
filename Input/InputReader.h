#pragma once
#include <functional>
#include <vector>
#include "Input/DeviceInput.h"

class Renderer;

// Cihazdan (klavye, fare, dokunmatik) her frame bir DeviceInput okur. Parmakları frame'ler arasında izler: yeni inen
// parmak 'taps'e girer, basılı tutulan 'held'de kalır. Ekran geçişlerinde de her frame okunmalı, yoksa geçiş
// sırasında inen parmak geçişten sonra yeni dokunuş sayılır.
class InputReader {
public:
    [[nodiscard]] DeviceInput Read(const Renderer& renderer);
    // Identity mapping samples window coordinates; external consumers can supply their own mapping.
    [[nodiscard]] DeviceInput Read();
    [[nodiscard]] DeviceInput Read(const std::function<std::optional<Vec2>(Vec2)>& mapPoint);
    // Cihazı örneklemeye devam eder; tüm basılı girdiler bırakılana kadar yalnız cancelled snapshot verir.
    void Cancel();

private:
    std::vector<int> _touchIds; // Önceki frame'deki parmaklar
    bool _suppressUntilNeutral = false;
};
