#pragma once
#include <optional>
#include <string>
#include "Fx/CameraShot.h"
#include "Fx/FxQuality.h"
#include "Fx/FxStyle.h"
#include "Utilities/CameraView.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class Painter;

// Bir oyun alanının hazır efekt sahnesi: parçacık, uçan yazı, halka, şok dalgası, kamera (sarsıntı, vuruş, tekme,
// hızlanma, sinematik çekim), flaş / banner / toast, nabız, glitch ve ağır çekim. Konumlar canvas koordinatındadır;
// stiller kurulurken verilir (genelde CurrentTheme().fx). 'area' efektlerin oynadığı alan, 'viewArea' kenar
// efektlerinin (vinyet, flaş) kapladığı görüş alanı. Ömürler Update'in zamanıyla ilerler; sarsıntı, hız çizgileri ve
// glitch deseni çizimin saatinden (Painter::Time) türetilir.
class FxStage {
public:
    FxStage(const FloatRect& area, const FloatRect& viewArea, const FxStyle& style);
    // The player's effect choice (Game::Effects()) for particles, camera, flashes and glitches.
    void SetQuality(const FxQuality& quality);
    void Update(float deltaSeconds);
    void Clear();
    void Burst(Float2 at, Color4 color, int count, ParticleRecipe recipe = ParticleRecipe::Soft);
    void Trail(Float2 at, Color4 color);
    // 'facing': hareket yönü; kıvılcımlar arkaya saçılır.
    void Dash(Float2 at, Float2 facing, Color4 color);
    void Steam(Float2 at, Color4 color);
    void FloatText(Float2 at, const std::string& text, Color4 color);
    // Patlayarak giren, 'size' boyunda uçan yazı.
    void PopText(Float2 at, const std::string& text, Color4 color, float size);
    void Ring(Float2 at, Color4 color);
    void Toast(const std::string& text, Color4 color);
    void Flash(Color4 color);
    void Banner(const std::string& text);
    void Shake(float seconds);
    void Punch(float amount, std::optional<Float2> focus = std::nullopt);
    void Kick(Float2 direction);
    // Her frame: hızlanma yoğunluğu (0-1); kamera öne kayar, hız çizgileri 'color' renginde akar.
    void SetMotion(float intensity, Color4 color, Float2 facing);
    // Her frame: alan 'color'a çalar, kenarlar kalp atışı gibi nabız atar (yumuşak geçiş).
    void SetPulse(bool active, Color4 color);
    // 'at'tan yayılan çift halkalı şok dalgası; 'reach' > 0 ise o kadar birime, değilse bütün alana.
    void Shockwave(Float2 at, Color4 color, float reach = 0.0f);
    void Glitch(float seconds);
    void SlowMotion(float seconds, Color4 color);
    // Her frame: sinematik kamera çekimi; weight 0 iken normal kamera.
    void SetShot(const CameraShot& shot) { _shot = shot; }
    // Alanın kamerası: çekim + sarsıntı / vuruş / tekme / hızlanma; çizim: paint.Transform(camera.Transform()).
    [[nodiscard]] CameraView WorldCamera(float timeSeconds) const;
    // Dünya kamerası içinde, alanın nesnelerinin üstüne.
    void DrawWorldLayer(Painter& paint) const;
    // Kamerasız, görüş alanının üstüne.
    void DrawOverlay(Painter& paint) const;

private:
    FloatRect _area;
    FloatRect _viewArea;
    FxStyle _style;
    ParticleSystem2D _particles;
    FloatingText2D _texts;
    RingEffects2D _rings;
    CameraEffects2D _camera;
    OverlayEffects2D _overlays;
    PulseOverlay2D _pulse;
    ShockwaveEffects2D _shockwaves;
    GlitchEffects2D _glitch;
    Color4 _motionColor{};
    float _slowMotionAge = 0.0f;
    float _slowMotionDuration = 0.0f;
    Color4 _slowMotionColor{};
    CameraShot _shot;
};
