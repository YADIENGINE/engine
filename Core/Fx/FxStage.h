#pragma once
#include <raylib.h>
#include <optional>
#include <string>
#include "Fx/CameraShot.h"
#include "Fx/FxStyle.h"
#include "Utilities/Rect.h"

class TextRenderer;

// Bir oyun alanının hazır efekt sahnesi: parçacık, uçan yazı, halka, şok dalgası, kamera (sarsıntı, vuruş, tekme,
// hızlanma, sinematik çekim), flaş / banner / toast, nabız, glitch ve ağır çekim. Konumlar canvas koordinatındadır;
// stiller kurulurken verilir (genelde CurrentTheme().fx). 'area' efektlerin oynadığı alan, 'viewArea' kenar
// efektlerinin (vinyet, flaş) kapladığı görüş alanı. Ömürler Update'in zamanıyla ilerler; sarsıntı, hız çizgileri ve
// glitch deseni çağıranın verdiği 'timeSeconds' saatinden türetilir.
class FxStage {
public:
    FxStage(const Rect& area, const Rect& viewArea, const FxStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    void Burst(Vector2 at, Color color, int count, ParticleRecipe recipe = ParticleRecipe::Soft);
    void Trail(Vector2 at, Color color);
    // 'facing': hareket yönü; kıvılcımlar arkaya saçılır.
    void Dash(Vector2 at, Vector2 facing, Color color);
    void Steam(Vector2 at, Color color);
    void FloatText(Vector2 at, const std::string& text, Color color);
    // Patlayarak giren, 'fontSize' boyutunda uçan yazı.
    void PopText(Vector2 at, const std::string& text, Color color, int fontSize);
    void Ring(Vector2 at, Color color);
    void Toast(const std::string& text, Color color);
    void Flash(Color color);
    void Banner(const std::string& text);
    void Shake(float seconds);
    void Punch(float amount, std::optional<Vector2> focus = std::nullopt);
    void Kick(Vector2 direction);
    // Her frame: hızlanma yoğunluğu (0-1); kamera öne kayar, hız çizgileri 'color' renginde akar.
    void SetMotion(float intensity, Color color, Vector2 facing);
    // Her frame: alan 'color'a çalar, kenarlar kalp atışı gibi nabız atar (yumuşak geçiş).
    void SetPulse(bool active, Color color);
    // 'at'tan yayılan çift halkalı şok dalgası; 'reach' > 0 ise o kadar birime, değilse bütün alana.
    void Shockwave(Vector2 at, Color color, float reach = 0.0f);
    void Glitch(float seconds);
    void SlowMotion(float seconds, Color color);
    // Her frame: sinematik kamera çekimi; weight 0 iken normal kamera.
    void SetShot(const CameraShot& shot) { _shot = shot; }
    [[nodiscard]] Camera2D WorldCamera(float timeSeconds) const;
    // Dünya kamerası içinde, alanın nesnelerinin üstüne.
    void DrawWorldLayer(const TextRenderer& text) const;
    // Kamerasız, görüş alanının üstüne.
    void DrawOverlay(const TextRenderer& text, float timeSeconds) const;

private:
    Rect _area;
    Rect _viewArea;
    FxStyle _style;
    ParticleSystem2D _particles;
    FloatingText2D _texts;
    RingEffects2D _rings;
    CameraEffects2D _camera;
    OverlayEffects2D _overlays;
    PulseOverlay2D _pulse;
    ShockwaveEffects2D _shockwaves;
    GlitchEffects2D _glitch;
    Color _motionColor{};
    float _slowMotionAge = 0.0f;
    float _slowMotionDuration = 0.0f;
    Color _slowMotionColor{};
    CameraShot _shot;
};
