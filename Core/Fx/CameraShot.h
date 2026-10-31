#pragma once
#include "Utilities/CameraView.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

// Cinematic camera shot: the field moves to 'focus' (a canvas point), zooms by 'zoom', and rotates by 'rotation'
// radians. 'weight' 0..1 blends from the normal camera to this shot; at 0 the shot has no effect.
struct CameraShot {
    Float2 focus{};
    float zoom = 1.0f;
    float rotation = 0.0f; // Radians, clockwise
    float weight = 0.0f;
    // true: yakın çekimde görünen alan arenada kalır (odak içeri çekilir); false: arena kenarındaki bir şeyi bütünüyle
    // kadraja almak için odak arenanın dışına taşabilir.
    bool keepInArena = true;
};

// Kadraj: 'safeArea' (ör. sinema şeritlerinin arası) 'viewport' ile kesişir, kenarlardan 'padding' kadar içeride
// kalınır; zoom [minZoom, maxZoom] aralığında.
struct CameraFraming {
    FloatRect viewport;
    FloatRect safeArea;
    float padding = 0.0f;
    float minZoom = 1.0f;
    float maxZoom = 1.0f;
};

// 'bounds'u güvenli alanın ortasına oturtan çekim (weight 1); boş 'bounds' etkisiz çekim döndürür.
[[nodiscard]] CameraShot FrameCameraShot(const FloatRect& bounds, const CameraFraming& framing);
// İki çekim arası: odak, zoom, dönüş ve ağırlık 't' (0..1) oranında.
[[nodiscard]] CameraShot BlendShots(const CameraShot& from, const CameraShot& to, float t);
// Sinematik temel kamera; kamera efektleri (CameraEffects2D) bunun üstüne uygulanır.
[[nodiscard]] CameraView ShotCamera(const CameraShot& shot, const FloatRect& viewport, const FloatRect& arena);
