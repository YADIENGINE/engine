#pragma once
#include <raylib.h>

// Sinematik kamera çekimi: oyun alanı 'focus'a (canvas noktası) kayar, 'zoom' katına yakınlaşır, 'rotation' derece
// döner. 'weight' 0..1: normal kameradan bu çekime geçişin oranı; 0 iken çekim etkisizdir.
struct CameraShot {
    Vector2 focus{};
    float zoom = 1.0f;
    float rotation = 0.0f;
    float weight = 0.0f;
    // true: yakın çekimde görünen alan arenada kalır (odak içeri çekilir); false: arena kenarındaki bir şeyi bütünüyle
    // kadraja almak için odak arenanın dışına taşabilir.
    bool keepInArena = true;
};

// Kadraj: 'safeArea' (ör. sinema şeritlerinin arası) 'viewport' ile kesişir, kenarlardan 'padding' kadar içeride
// kalınır; zoom [minZoom, maxZoom] aralığında.
struct CameraFraming {
    Rectangle viewport;
    Rectangle safeArea;
    float padding = 0.0f;
    float minZoom = 1.0f;
    float maxZoom = 1.0f;
};

// 'bounds'u güvenli alanın ortasına oturtan çekim (weight 1); boş 'bounds' etkisiz çekim döndürür.
[[nodiscard]] CameraShot FrameCameraShot(Rectangle bounds, const CameraFraming& framing);
// İki çekim arası: odak, zoom, dönüş ve ağırlık 't' (0..1) oranında.
[[nodiscard]] CameraShot BlendShots(const CameraShot& from, const CameraShot& to, float t);
// Produces the cinematic base camera; apply camera feedback afterwards.
[[nodiscard]] Camera2D ShotCamera(const CameraShot& shot, Rectangle viewport, Rectangle arena);
