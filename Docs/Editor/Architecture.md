# Editor architecture

| Katman | Hedef / kaynak | Sorumluluk |
| --- | --- | --- |
| Public API | EngineEditorApi / Engine/Editor/Api | Metadata, parametre, inspection; ImGui/IPC bilmez |
| Support | EngineEditorSupport / Internals | Proje tarifi, protokol, süreç, viewport, tercihler |
| Worker runtime | EnginePreviewRuntime / Preview | Game tabanlı preview ve session |
| Arayüz | EngineEditorUi / Ui | Shell, docking, paneller ve widget'lar |
| Uygulama | EngineEditor / Host, Application | Windows süreçleri ve UI lifecycle |
| Proje worker'ı | tüketicinin hedefi | Oyun View/model ve açık katalog kaydı |

Engine → Game bağımlılığı yoktur. Normal oyuna Editor API veya ImGui link verilmez.
`ENGINE_BUILD_EDITOR` Windows ve runtime gerektirir; `ENGINE_BUILD_EDITOR_TESTS`
runtime kapalıyken headless protokol/state testlerini açabilir.
Bağımlılık sürümleri ve IPC sınırları [API](Api.md) içindedir.

## Worker akışı

Editör önce worker'ı catalog modunda başlatır; registry yalnız metadata verir,
GL/font/ses kurulmaz. Önizleme açıldığında RunPreviewHost servisleri ve PreviewGame'i
kurar; Game::Run tek döngüdür. PreviewScene içerik instance'ını ObjectList ile yönetir.
Reset instance'ı yeniler; Stop worker sürecini kapatır.
Build & Refresh gerçek süreç çıkışını bekler ve yalnız tarifteki build komutunu çalıştırır.

## Sunum saati ve playback

- Görsel zaman Engine sunum saatidir: `DrawTime()` (`Graphics/Draw/DrawTime.h`). `Game` her kareyi çizmeden önce
  sanal `Game::DrawSeconds()`'ı okuyup yazar. Oyunda gerçek zaman; worker'da `PreviewGame`
  override'ı önizleme sahnesinin Reset'te 0'lanan ölçekli saatidir; özel Game adaptörü akış saatini sağlar.
- View'lar saat tutmaz, aktarmaz, `GetTime()` çağırmaz. Kendi zamanını oynatan çizim (portre, giriş) zamanı
  `float time` parametresiyle verir.
- Pause Update'i ve sunum saatini dondurur; çizim, iletişim, parametreler ve eylemler sürer. Hız (0.1×..4×) ve
  Step (1/60 s) `Game::FrameSeconds` üzerinden hem gerçek zamanlı menülere hem oyun zamanına uygulanır.

## Kontroller ve inceleme

- Ayarlar `PreviewParameter`, eylemler `PreviewAction`; Inspector grupları / açıklamaları içerik tanımında author edilir.
- `CaptureParameters` eylemin veya ürün girdisinin değiştirdiği authored değerleri bildirir; `MergePreviewCapture`
  bütün bildirimi doğrulayıp atomik uygular, `OnReset` tarifleri staged kalır.
- `PreviewInspection` salt okunur değerleri, tabloları, focus alanını ve kontrol görünürlüğünü taşır; tarife karışmaz,
  en çok 10 Hz gönderilir.
- Fit / Focus / 100% / pan / zoom aynı canvas dönüşümünü input'ta da kullanır; panel dışı noktalar reddedilir.



## UI katmanları (EngineEditor)

- Host Game / ImGui yaşam döngüsünü, GPU görüntü köprüsünü ve input yönlendirmesini sahiplenir; ImGui bağlamı ve
  görüntü dokusu RAII ile GL bağlamı yaşarken temizlenir.
- `EditorUiContext` font ve ölçekleme politikasının tek sahibidir (gömülü ProggyForever + Font Awesome; Inspector
  etiketlerinde Windows Segoe UI, bulunamazsa gömülü font; kullanıcı yüzdesi × DPI).
- `Ui/Widgets/InspectorWidgets` bütün önizlemelerin ortak Inspector sunumudur: sade açılır bölüm başlıkları,
  %39 / %61 özellik sütunları, kontrol altında Default / uygulama zamanı, monospace sayısal değerler ve salt okunur
  satırlar. Boyutlar font / DPI ile ölçeklenir; bu stil kapsamı diğer panelleri değiştirmez. Eylemler açık `primary`
  metadata'sına göre yerleşir; ikincil eylemler yeterli genişlikte iki, dar panelde tek sütundur.
- `Application/EditorController` süreç, IPC ve önizleme komutlarını yönetir. Paneller yalnız `EditorSnapshot` ve
  `EditorUiActions` bilir; UI komutları bütün paneller çizildikten sonra uygulanır.
- `Ui/Shell` menü / toolbar / durum çubuğu, `Ui/Workspace` docking ve geçici büyütme, `Ui/Panels` paneller,
  `Ui/Style` tema / ikon, `Ui/Widgets` ortak kontroller. DockBuilder'ın internal API'si yalnız Workspace'te.
- `Internals/Diagnostics` ve `Internals/Settings` ImGui / raylib bilmez. Tercihler
  `UserDataDirectory()/EngineEditor/preferences.json` altında atomik saklanır (yerleşim, favoriler, geçmiş, Output).
- Yeni panel kendi klasöründe değer bileşenidir; Workspace görünürlük / docking bağlantısını kurar. Yeni süreç
  komutunun kullanılabilirliği Application'da tanımlanır; menü ve toolbar aynı sözleşmeyi kullanır.


Proje adı ve build dizini editör tercihlerini ayırır. Worker options geçicidir.
Gerçek oyun adaptörlerinde ilerleme ve telemetry izolasyonu projeye aittir;
ayrıntılar [CreatingPreviews](CreatingPreviews.md).
