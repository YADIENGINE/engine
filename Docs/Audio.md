# Audio

Engine ses cihazını ve kaynakları sahiplenir; oyun neyin ne zaman çalacağını belirler.
Public API [AudioSystem.h](../Audio/AudioSystem.h), katalog sözleşmesi
[AudioCatalogDefinition.h](../Audio/AudioCatalogDefinition.h) içindedir.

## Katalog kurmak

```cpp
#include "Runtime/GameSettings.h"

namespace {
constexpr SoundId Click{0};
constexpr MusicId MenuMusic{0};
}

GameSettings Settings() {
    GameSettings settings;
    settings.applicationId = "MyProject";
    settings.audio.effects = {{Click, "Audio/click.wav"}};
    settings.audio.music = {{MenuMusic, "Audio/menu.ogg", MusicVolumeGroup::Menu}};
    return settings;
}
```

ID'ler proje tarafından atanır; SoundId ve MusicId ayrı yuvalardır.
Dosyalar asset köküne göre yoldur. Runtime katalogdaki yolları asset manifestine ekler;
ayrıca aynı yolları `settings.assets` içine yazmanız gerekmez.
Dosyaları paketlemek tüketicinin sorumluluğudur: [Assets](AssetsAndPlatforms.md).

## Yükleme ve sahiplik

`Game` ses sistemini kurar ve `BeginLoading` çağırır. Arka plan yüklemesi tamamlanıp
`FinishLoading()` başarılı olana kadar playback veya Load/Unload API'si kullanmayın.
Volume değerleri yükleme sırasında saklanıp devralmada uygulanabilir.
Dosya/cihaz yükleme hataları exception olarak bildirilir.
`Game::Start()` ses hazır olmadan çalışabilir; ilk normal sahne update/input akışı yükleme sonrasındadır.

Kendi servisiniz varsa `FinishLoading()` false iken bekleme ekranını sürdürün;
başarıda kaynaklar ana thread'e devralınır. AudioSystem process çapındaki cihazı sahiplenir;
ikinci bir cihaz veya paralel raylib audio erişimi kurmayın.
Yükleme thread'i bir altyapı ayrıntısıdır; tüketici API'si genel thread-safe servis değildir.

## Playback

- `Audio().PlayEffect(Click)`: aynı efekt için varsayılan dört voice üst üste çalabilir.
  Hepsi doluysa sıradaki voice baştan başlar. `LoadEffect` maxConcurrent ile sınırı değiştirir.
- `PlayEffectIfIdle`: zaten çalıyorsa yeni isteği atlar; kuyruk oluşturmaz.
- `PlayMusic`: mevcut parçayı durdurur, yeni parçayı normal pitch ile başlatır; varsayılan loop açıktır.
- `SetMusicPitch`: hız ve perde birlikte değişir. `CurrentMusic` duraklatılmış parçayı da bildirir.
- `PauseMusic` / `ResumeMusic` duraklatır; `StopMusic` current seçimini temizler.
- `AudioSystem::Update()` stream'i her frame doldurur; Game bunu zaten çağırır.

`LoadEffect/LoadMusic` dolu yuvayı önce boşaltır. Dinamik önizleme yalnız kendi yüklediği
kaynakları boşaltmalıdır. Bütün kaynaklar cihaz kapanmadan bırakılır; Game cihazı pencereden önce kapatır.

## Volume ve kalıcılık

Master, efekt, müzik grubu ve efektin kendi volume değeri ayrı katmanlardır.
`MusicVolumeGroup::Menu` menü müziğine ayrı kullanıcı seviyesi sağlar.
`settings.audioVolume.steps` yüzde adımlarını belirler; `AdjustEffectVolume` /
`AdjustMusicVolume` bu adımlar üzerinden değiştirir. `SetMuted` geçici mute içindir.
Engine `audio.*` seçeneklerini yükler ve kayıt noktalarında saklar;
oyun bunlar için ikinci bir settings anahtarı oluşturmaz.
Android odak kaybı müziği duraklatır: [runtime](Runtime.md).
