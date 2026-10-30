#pragma once
#include <raylib.h>

// Ardışık parıltıları tek additive blend kapsamında, verildikleri sırayla çizer; kuyruk veya tahsis yoktur.
// RenderExecutionOptions Individual seçerse aynı parıltıları tekil DrawGlow yoluyla verir.
// Alpha blending etkinken yerel kapsamda kurulur. Kapsam boyunca yalnız Draw çağrılır; başka çizim, blend,
// kamera, scissor veya render target değişimi yapılmaz. İlk parıltıda açılır, yıkımda alpha blending'e döner.
class GlowBatch {
public:
    GlowBatch();
    ~GlowBatch();

    void Draw(Vector2 center, float radius, Color color);

private:
    bool _active = false;
    bool _individual = false;

public:
    GlowBatch(const GlowBatch&) = delete;
    GlowBatch& operator=(const GlowBatch&) = delete;
    GlowBatch(GlowBatch&&) = delete;
    GlowBatch& operator=(GlowBatch&&) = delete;
};
