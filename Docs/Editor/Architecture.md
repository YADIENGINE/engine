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
IPC protokolü v5, odak kaybı / pause / input süresi aşımını normal release yerine cancellation olarak taşır.
Profiler mesajları generation/capture kimliği taşır; sınırlı parçalar tamamlanmadan kayıt yayımlanmaz.
Public snapshot'ın kimlikli touch verisi ve canvas dışındaki boş pozisyonları da aktarılır. Host, private pencere
örneklerini Input normalizer ile canvas'a dönüştürür; worker aynı public sözleşmeyi tüketir.
İptalden önce birikmiş eylemler silinir; sonraki yeni basışlar iptal bildiriminin ardından teslim edilir.
Host ve worker aynı protokol sürümüyle birlikte güncellenmelidir; proje tarifi sürümü değişmez.

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
- `EditorUiContext` font ve ölçekleme politikasının tek sahibidir: arayüzde Windows Segoe UI + Font Awesome,
  teknik değerlerde Consolas; sistem fontu bulunamazsa gömülü font. `EditorFontRole` atlas sırasından bağımsız
  Ui / Technical erişimi sağlar. Varsayılan ölçek 1.25'tir; kullanıcı yüzdesi × varsayılan ölçek × DPI hem
  fontlara hem taban stile uygulanır (%100 önceki %125 boyutundadır).
- `Ui/Style` nötr koyu yüzeyler, mavi vurgu, durum renkleri ve ortak ölçüleri; `Ui/Widgets/EditorWidgets`
  bölüm başlığı, durum rozeti, seçim grubu ve boş durum sunumunu paylaşır. Ortak bileşenler dar genişlikte
  metni kısaltır / sarar; bilgi tooltip ile korunur.
- `Ui/Widgets/InspectorWidgets` bütün önizlemelerin ortak Inspector sunumudur: sade açılır bölüm başlıkları,
  ölçülmüş etiket / değer / sıfırlama sütunları; dar panelde üstte etiket, altta tam genişlikli kontrol.
  Seçenek grupları her genişlikte etiketin altında tam panel genişliğini kullanır. Sütun sayısı metin ölçüsüne
  göre azalır; düğmeler tek satır kalır. Uygulama zamanı, monospace sayısal değerler ve salt okunur satırlar korunur.
  Arama ve Modified only yalnız özellik sunumunu filtreler; authored grup, uygulama zamanı ve görünürlük
  sözleşmesini değiştirmez. Eylemler açık `primary` metadata'sına göre yerleşir; ikincil eylemler yeterli
  genişlikte iki, dar panelde tek sütundur.
- `Application/EditorController` süreç, IPC ve önizleme komutlarını yönetir. Paneller yalnız `EditorSnapshot` ve
  `EditorUiActions` bilir; UI komutları bütün paneller çizildikten sonra uygulanır.
- `Application/EditorCommandState` kullanılabilirlik ve engel nedenlerini tek yerde çözer. Host'a ait
  `EditorCommand::Step` mevcut playback mesajını kullanır; public `Preview*` API'si ve worker protokolü değişmez.
- `Ui/Commands` komut kimliği / etiket / ikon / kısayol tablosunu menü, toolbar ve komut paletiyle paylaşır.
  Open / Stop / Cancel ana eylemi oturum durumundan türetilir. Kısayol ve paletin tükettiği girdi aynı karede
  worker'a gönderilmez; popup ve metin düzenleme sırasında önizleme kontrol kısayolları engellenir.
- `Ui/Shell` menü / global toolbar / durum çubuğunu; `Ui/Workspace` docking, panel kaydı, Develop / Inspect /
  Profile düzenleri ve geçici büyütmeyi yönetir. Panel kimlikleri görünür başlıktan bağımsızdır; görünürlük ve
  varsayılan dock konumu ortak `EditorPanelDefinition` tablosundadır. DockBuilder'ın internal API'si yalnız
  Workspace'te kullanılır. Hazır düzene geçiş son özel düzeni korur; kayıtlı kullanıcı düzeni açılışta ezilmez.
- `Ui/Panels` yerel araçları sahiplenir: View görünüm / zoom menüsü, Output tek Copy menüsü, Profiler gelişmiş
  ölçüm ayarları ve Export menüsü. Output / Profiler varsayılan olarak kapalıdır; alt durum çubuğu görünürlük,
  süreç durumu ve hata sayacını sunar.
- `Internals/Diagnostics` ve `Internals/Settings` ImGui / raylib bilmez. Tercihler
  `UserDataDirectory()/EngineEditor/preferences.json` altında atomik saklanır (yerleşim, son özel düzen,
  görünürlük, ölçek, favoriler, geçmiş, Output).
- Yeni panel kendi klasöründe değer bileşenidir; ortak panel kaydına eklenir. Yeni süreç komutu Application'da
  davranış / kullanılabilirlik, Ui/Commands'ta sunum bilgisi alır; menü, toolbar ve palet aynı sözleşmeyi kullanır.
  Henüz desteklenmeyen yetenekler için boş servis veya kullanılmayan public API eklenmez; bağımlılıklar
  [UI geliştirme listesinde](TODO.md) tutulur.

Profiler paneli Live / Benchmarks / Compare sunar. Headless modül ve isteğe bağlı runtime observer editörden
bağımsızdır. Worker ölçümde görüntü aktarımını durdurur; host 10 Hz'e iner. FPS/VSync/render politikası ölçüm
oturumuna aittir ve sonra geri yüklenir. Ayrıntılar: [Profiler](Profiler.md).


Proje adı ve build dizini editör tercihlerini ayırır. Worker options geçicidir.
Gerçek oyun adaptörlerinde ilerleme ve telemetry izolasyonu projeye aittir;
ayrıntılar [CreatingPreviews](CreatingPreviews.md).
