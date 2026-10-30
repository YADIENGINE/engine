# Audio

Engine ses cihazını ve kaynakları sahiplenir; oyun neyin ne zaman çalacağını belirler.
Tek public giriş [AudioSystem.hpp](../Core/Audio/AudioSystem.hpp): servis, kimlikler, katalog ve ayarlar bu
header'dadır. Header raylib veya yükleme thread'i içermez. `Audio/Internals/` desteklenen API değildir;
oyun ve editör yalnız public header'ı kullanır. Ses, Engine runtime servisidir; ayrı bir opt-in modül değildir.

## Katalog kurmak

```cpp
#include "Runtime/GameSettings.h"
#include "Audio/AudioSystem.hpp"

namespace {
constexpr SoundId Click{0};
constexpr MusicId MenuMusic{0};
}

GameSettings Settings() {
    GameSettings settings;
    settings.applicationId = "MyProject";
    settings.audio.effects = {{Click, "Audio/click.wav",
                               {.maxConcurrent = 2, .volume = 0.8f, .overflow = EffectOverflow::Skip}}};
    settings.audio.music = {{MenuMusic, "Audio/menu.ogg", MusicVolumeGroup::Menu}};
    return settings;
}
```

ID'ler proje tarafından atanır; SoundId ve MusicId ayrı kimlik alanlarıdır.
Seyrek ID'ler desteklenir; en büyük ID kadar boş yuva ayrılmaz.
Yeni bir oyun kendi ID sabitlerini ve kataloğunu oluşturup `GameSettings::audio` alanına verir.
İkinci bir ses servisi, yükleme döngüsü veya platforma özel çalma kodu kurması gerekmez.
Dosyalar asset köküne göre yoldur. Runtime katalogdaki yolları asset manifestine ekler;
ayrıca aynı yolları `settings.assets` içine yazmanız gerekmez.
Dosyaları paketlemek tüketicinin sorumluluğudur: [Assets](AssetsAndPlatforms.md).

`AudioSystem::ValidateCatalog(catalog)` cihaz açmadan ve dosyalara erişmeden sözleşmeyi denetler.
Boş katalog geçerlidir; aynı türde yinelenen ID, boş/absolute/`..` içeren yol, sıfır voice,
geçersiz volume veya enum değeri `std::invalid_argument` fırlatır.
SoundId ve MusicId aynı sayısal değeri kullanabilir. Dosyanın varlığı ve biçimi yüklenirken denetlenir.
`Game`, asset hazırlamadan önce; `BeginLoading` de worker başlatmadan önce kataloğu doğrular.

## Yükleme ve sahiplik

`Game` ses sistemini kurar ve `BeginLoading` çağırır. Arka plan yüklemesi tamamlanıp
`FinishLoading()` başarılı olana kadar efekt çalma/durdurma veya Load/Unload API'si kullanmayın.
Volume değerleri yükleme sırasında saklanıp devralmada uygulanabilir.
Dosya/cihaz yükleme hataları exception olarak bildirilir.
`Game::Start()` ses hazır olmadan çalışabilir; ilk normal sahne update/input akışı yükleme sonrasındadır.

Kendi servisiniz varsa `FinishLoading()` false iken bekleme ekranını sürdürün;
başarıda kaynaklar ana thread'e devralınır:

| Durum | Davranış |
| --- | --- |
| Yükleme başlamadı | `FinishLoading` ve playback/resource değişiklikleri `std::logic_error` fırlatır. |
| Yükleniyor | `FinishLoading` false; playback/resource değişiklikleri `std::logic_error` fırlatır. |
| Hazır | `FinishLoading` sonraki çağrılarda da true; çalma ve dinamik kaynak işlemleri kullanılabilir. |
| Yükleme başarısız | Kısmi kaynaklar bırakılır; `FinishLoading` ve playback/resource değişiklikleri aynı hatayı yeniden fırlatır. |

Bir instance tek yükleme girişimi yapar; ikinci `BeginLoading` `std::logic_error` fırlatır.
Katalog doğrulaması başarısızsa yükleme başlamamıştır; katalog düzeltilip tekrar denenebilir.
Worker hatasından sonra yeniden denemek için servis yok edilip yeniden kurulur.

AudioSystem kurulurken process çapındaki cihazı rezerve eder; ikinci canlı instance `std::logic_error` fırlatır.
Servisi kuran thread'den kullanın; public API genel thread-safe servis değildir. Doğrudan raylib audio çağrıları
ve paralel cihaz sahipliği desteklenmez. Yükleme worker'ı yalnız private kaynakları kullanır; ana thread
join sonrasında devralır. Destructor yüklemeyi iptal etmeyi ister ve worker'ı bekler.
Kaynaklar RAII ile yönetilir; alias'lar örnekten önce, bütün kaynaklar cihazdan önce bırakılır.
`Game::Audio()` ve editörün verdiği referanslar servis sahibinin ömrünü aşmamalıdır.

## Playback

- `Audio().PlayEffect(Click)`: önce boş bir voice seçer. `EffectSettings` varsayılanı dört eşzamanlı
  kopya, volume 1 ve `EffectOverflow::RestartNext`'tir. Bütün kopyalar doluysa sıradaki baştan başlar;
  `EffectOverflow::Skip` yeni isteği atlar. Katalogdaki `EffectAsset::settings` veya dinamik
  `LoadEffect(id, file, settings)` ile efekt başına belirlenir.
- `PlayEffectIfIdle`: zaten çalıyorsa yeni isteği atlar; kuyruk oluşturmaz.
- `StopEffect`: ilgili ID'nin bütün kopyalarını durdurur.
- `PlayMusic`: mevcut parçayı durdurur, yeni parçayı normal pitch ile başlatır; varsayılan loop açıktır.
- `PlayEffect`, `PlayEffectIfIdle` ve `SetMusicPitch` pitch'i sonlu ve pozitif olmalıdır;
  aksi durumda `std::invalid_argument` fırlatılır. Hız ve perde birlikte değişir.
- `CurrentMusic` duraklatılmış veya doğal olarak bitmiş parçayı da bildirir; `IsMusicPlaying` gerçek çalma durumudur.
- `PauseMusic` / `ResumeMusic` duraklatır; `StopMusic` current seçimini temizler.
- `AudioSystem::Update()` stream'i her frame doldurur; Game bunu zaten çağırır.

`HasEffect` / `HasMusic` hazır olmadan false döner. `CurrentMusic` boş, `IsMusicPlaying` false olur;
seçili parça yokken `StopMusic`, `PauseMusic`, `ResumeMusic` ve `Update` güvenli no-op'tur.
Diğer playback/resource işlemleri önce sistemin hazır olmasını gerektirir.
Hazır sistemde yüklenmemiş ID ile işlem `std::out_of_range` fırlatır.

`LoadEffect/LoadMusic` yeni kaynağı tamamen yükledikten sonra eskisini değiştirir;
başarısız yükleme eski kaynağı ve çalan müziği korur. Başarılı efekt değişimi verilen `EffectSettings`'i uygular.
Çalan müzik ID'si başarıyla değiştirilirse çalma durur; yeni stream kendiliğinden başlamaz.
Dinamik önizleme yalnız kendi yüklediği kaynakları boşaltmalıdır. Game cihazı pencereden önce kapatır.

## Volume ve kalıcılık

Master, efekt, müzik grubu ve efektin kendi volume değeri ayrı katmanlardır.
Volume setter'ları sonlu `0..1` aralığı dışındaki değerlerde `std::invalid_argument` fırlatır.
Efekt çıkışı master × genel efekt seviyesi × efektin kendi seviyesidir.
`MusicVolumeGroup::Menu` menü müziğine ayrı kullanıcı seviyesi sağlar.
`settings.audioVolume.steps` yüzde adımlarını belirler; `AdjustEffectVolume` /
`AdjustMusicVolume` bu adımlar üzerinden değiştirir ve seviye değiştiyse true döner.
Adım sayısı 100'ü bölen pozitif bir tamsayı olmalıdır; aksi durumda constructor `std::invalid_argument` fırlatır.
`EffectVolumePercent` / `MusicVolumePercent` menüde gösterilecek yüzdeyi döndürür.
`SetMuted` geçici mute içindir; saklanan seviyeleri değiştirmez. Master ve mute kalıcı tercih değildir.
Engine `audio.*` seçeneklerini yükler ve kayıt noktalarında saklar;
oyun bunlar için ikinci bir settings anahtarı oluşturmaz. Eksik/geçersiz kayıtlı seviyeler ayrı ayrı %100'e döner.

## Bağımlılıklar ve doğrulama

Backend raylib 5.5'tir ve `Engine` hedefinde kalır. Katalog doğrulaması ve private volume politikası
`EngineHeadless`'da headless derlenir; public header tek başına raylib istemez.
Engine ses altyapısı hiçbir oyun kimliğini, asset yolunu veya çalma koreografisini bilmez.
Headless ses testleri `[audio]` etiketi altındadır; gerçek çalma ve stream
geliştiricinin manuel kontrolüdür.
