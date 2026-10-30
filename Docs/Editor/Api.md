# Genel editör API'si

Bu belge projeden bağımsız sözleşmedir. İlk kurulum [GettingStarted](GettingStarted.md),
üretim çizimini kullanma ve içerik tasarımı [CreatingPreviews](CreatingPreviews.md) içindedir.

## Kayıt

Public başlıklar `Engine/Editor/Api/` altındadır; ImGui, Windows handle'ı ve IPC içermez. Proje, kendisine verilen
registry'ye açık kayıt yapar:

```cpp
void RegisterProjectPreviews(PreviewRegistry& registry) {
    PreviewDefinition definition;
    definition.id = "project.my-preview";
    definition.name = "My preview";
    definition.category = "Drawing";
    definition.parameters = {{"radius", "Radius", PreviewParameterKind::Float, 120.0, 8.0, 300.0}};
    definition.actions = {{"emit", "Emit"}};
    definition.create = [](const PreviewServices& services, const PreviewParameters& parameters) {
        return std::make_unique<MyPreview>(services, parameters);
    };
    registry.Add(std::move(definition));
}
```

`MyPreview`, `EditorPreview`'dan (bir `GameObject`) türer. Modelsiz çizimde yalnız `Draw() const` gerekir; `Start`,
`Update(float)`, `Stop`, `HandleInput`, `ApplyParameter`, `ExecuteAction`, `CaptureParameters`, `Inspect` gerektiğinde
override edilir. Kendi frame döngüsü yazılmaz; model ve gerçek View'ı içerik sahiplenir, editör için ikinci çizim
kopyası yazılmaz.

Kurallar:

- Kayıt yalnız metadata / fabrika ekler; pencere, asset, örnek kurulmaz. Kimlikler benzersizdir.
- Fabrika GL, metin ve ses hazırken engine thread'inde çağrılır; başlangıç değerleri tam harita olarak verilir,
  kurulumda `ApplyParameter` çağrılmasına güvenilmez.
- `PreviewServices` canvas boyutunu, metin / ses referanslarını, log ve uygulama komutu callback'lerini verir. Referanslar
  örnekten uzun yaşar; içerik durduktan sonra tutulmaz.
- `Update` ölçeklenmiş saniye alır. Görsel zaman `DrawTime()`'dır: `PreviewScene::Seconds()` her Reset'te 0'lanır ve
  `PreviewGame::DrawSeconds()` ile sunum saatine verilir; içerik saat bağlamaz.
- `Stop` ve destructor istisna atmaz. Fabrika / hook hataları worker'ı sonlandırabilir; editör hatayı ve çıkış
  kodunu gösterir.

`PreviewDefinition::benchmark` isteğe bağlı, tekrarlanabilir bir iş yükü sözleşmesidir. Kalıcı kimlik/sürüm,
iki varyantın adları, geçici render politikaları ve parametre farklarını taşır. Tanım yoksa önizleme yalnız
canlı ölçüme uygundur. `PreviewServices::profiler`, özel CPU scope/sayaç kaydı için ödünç modül referansıdır;
işlem yalnız `Modules/Profiler/Profiler.hpp` üzerinden yapılır. [Profiler rehberi](Profiler.md).

## Parametre ve eylem sözleşmesi

`PreviewValue`: `int64_t`, `double`, `bool`, normalize `PreviewColor` ya da seçenek metni (`std::string`).
`PreviewParameter` tür, varsayılan, aralık / seçenek, uygulama zamanı, `group` ve `description` taşır. Sayılar sonlu
ve aralık içinde, renk kanalları 0..1 olmalıdır. Değerler reddedilir, sessizce kırpılmaz.
`advanced = true` olan alanlardan oluşan gruplar ilk açılışta kapalıdır; arama / Modified filtresi eşleşmeleri açar.
`applyAction`, yalnız NextAction alanlarında tüketen eylemin kimliğini verebilir; kayıtlı bir eylem olmalıdır.
Inspector ortak uygulama zamanını grupta bir kez, farklı zamanları alanın `*` işaretinde / tooltip'inde gösterir.
Bu sunum bilgisi setter veya eylem yürütmez; değer değişikliği ile worker yanıtını bekleyen Pending ayrıdır.
Choice parametresinde `choiceButtons = true`, seçenekleri Inspector'da açılır liste yerine seçili değeri vurgulayan,
panel genişliğine göre bir veya iki sütuna yerleşen düğmeler olarak gösterir; varsayılanı `false`'tur.
`choiceButtonColumns` pozitifse en çok belirtilen sütun sayısını kullanır; alan daraldığında okunabilir düğmeler için
sütun sayısı azalır. Varsayılan `0`, bir veya iki sütun seçer.

`PreviewAction` kimlik, etiket, açıklama ve isteğe bağlı `primary` (varsayılan `false`) taşır. En fazla bir eylem
ana eylem olarak işaretlenebilir; listedeki konumu önemli değildir. Inspector onu tam genişlikte sabit alt alanda,
diğerlerini kaydırılabilir içerikte uyarlanabilir iki sütunlu grupta gösterir. Ana eylem belirtilmesi zorunlu değildir; eski kataloglarda alan yoksa
bütün eylemler ikincildir. Bu metadata eylem callback'lerini veya worker davranışını değiştirmez.

| Uygulama zamanı | Worker | İçerik |
| --- | --- | --- |
| `Immediate` | Setter'ı çağırır, başarıda kabul eder | Mevcut görünüşü güncelle |
| `NextAction` | Setter'ı çağırır, başarıda kabul eder | Sonraki tetikleme için sakla |
| `OnReset` | Doğrulayıp saklar; setter çağırmaz | Yeni fabrikada kullan |

Setter / eylem boş metinle başarı, açıklamayla ret bildirir; reddedilen setter içeriği değiştirmez. Editör yeniden
açılış için yalnız kabul edilen değeri saklar; yeni katalogda uyumsuz / silinmiş alanlar atılır ve açıklanır.
`CaptureParameters() const` eylemin veya ürün girdisinin değiştirdiği authored kontrolleri bildirir; runtime bütün
bildirimi doğrular, bilinmeyen / geçersiz alan hiçbir değeri değiştirmez. Preset diske yazımı, reflection ve otomatik
kaynak yazımı yoktur.

## İnceleme

`EditorPreview::Inspect()` salt okunur `PreviewInspection` döndürür: focus dikdörtgeni, etiket / değerler, tablo
sekmeleri, gizli / etkisiz kontrol ve eylem kimlikleri, `showImage`. Tarife karışmaz; değiştiğinde en çok 10 Hz
ayrı mesajla aktarılır. Genel UI proje kimliği bilmeden gösterir.
`PreviewReadout::kind` varsayılan `Text` için sola hizalı ve satır saran metin, `Number` için sağa hizalı teknik
font, `Status` için kısa rozet seçer. Sığmayan durum metni de satır sarar. Birimler biçimlenmiş değerin parçasıdır.

## Worker çalışma zamanı

`RunPreviewHost(argc, argv, RegisterProjectPreviews, settings)` isteğe bağlı bir Game fabrikası alır; varsayılan
`PreviewGame`'dir. `PreviewSession` IPC, örnek lifecycle'ı ve kare aktarımını Game'den ayırır; proje kendi gerçek
Game'inde aynı session'ı kurabilir (tüketicinin özel Game adaptörü). `PreviewSessionActions` quit / reset / activate ve
uygulama komutu callback'lerini bağlar. Game kancaları: `FrameSeconds` (host'un simülasyon süresi), `DrawSeconds`
(sunum saati), `SetScene(scene, true)` (aynı sahnenin Enter akışını yeniden başlatır). `fixedWindow` host ayarı
Options'ın OS pencere / capture boyutunu değiştirmesini engeller.

## Proje tarifi ve build

Proje CMake'i yapılandırmaya özel JSON tarifi üretir; başka tarif `EngineEditor.exe --project <yol>` ile seçilir:

```json
{
  "version": 1,
  "name": "My previews",
  "buildDirectory": "C:/MyProject/build",
  "worker": {"executable": "C:/MyProject/build/Debug/MyPreviewHost.exe", "arguments": [],
             "directory": "C:/MyProject/build/Debug", "environment": {}},
  "build": {"executable": "C:/Tools/CMake/bin/cmake.exe",
            "arguments": ["--build", "C:/MyProject/build", "--config", "Debug", "--target", "MyPreviewHost"],
            "directory": "C:/MyProject/build", "environment": {}}
}
```

Göreli yollar tarif dosyasına göredir; shell komut metni çalıştırılmaz. `worker.executable` build çıktısıdır;
editör worker'ları onun snapshot kopyalarından başlatır. Build & Refresh: çalışan önizleme açık kalırken yalnız
tarifteki hedefi derler → başarıda snapshot alır → yeni worker'ı aynı durumla eskisinin yanında başlatır → hazır
olunca yerine geçirir ([Architecture](Architecture.md#worker-akışı)). Build hatasında önizleme değişmez. Windows Job
Object yalnız editörün başlattığı süreç ağacını sahiplenir; 5 s içinde çıkmayan worker sonlandırılır. Aynı build
dizini editör oturumları arasında mutex ile korunur.

Genel editör Game olmadan derlenebilir:

```powershell
cmake -S Engine -B build/engine-editor -G "Ninja Multi-Config" -DENGINE_BUILD_RUNTIME=ON -DENGINE_BUILD_EDITOR=ON
cmake --build build/engine-editor --config Debug --target EngineEditor
```

`ENGINE_BUILD_EDITOR` ve `ENGINE_BUILD_EDITOR_TESTS` varsayılan OFF'tur. Raylib 5.5, rlImGui
118221c8a532501a3c2c236b62950877cbefc2b4 ve Dear ImGui v1.92.7-docking sabitlenmiştir.

## IPC ve görüntü aktarımı

- Kontrol: Windows named pipe (byte mode, `PIPE_NOWAIT`), 4 byte little-endian uzunluk + UTF-8 JSON. Mesaj en çok
  1 MiB; gönderim kuyruğu en çok 256 mesaj / 8 MiB; poll başına 64 KiB okuma / yazma bütçesi. Poll yalnız sahibi
  olan thread'de yapılır. Sürecin stdout / stderr çıktısı Output'a alınır; Full son 1000 satırı,
  Last Build Try son denemenin 10000 satır / 8 MiB tamponunu ve ilk hatasını tutar. Tam oturum / deneme
  logları build dizinindeki `editor-logs/` altındadır. Süreç çıkışında kalan çıktı 64 KiB poll sınırından
  bağımsız tamamen okunur; tampon sınırları dosyaya yazılan çıktıyı kesmez.
- Zarf: sürüm, oturum kimliği, içerik üretimi, istek kimliği, tür, veri. Reset yeni üretim, worker açılışı yeni oturum
  oluşturur. Başlangıç / ilk kare zaman aşımı 30 s.
- Görüntü: mutex korumalı iki paylaşımlı yuva, en çok 1920×1080 RGBA8. Canvas GPU'dan okunur, çevrilir, kopyalanır;
  editör kendi dokusuna yükler. Meşgul tampon beklenmez, eski kareler atlanabilir. Canvas boyutunu proje seçer
  (ölçüler GameSettings ile verilir). Hedef: worker 30 FPS, editör 60 FPS; View menüsündeki ölçüler readback / publish / transfer sürelerini
  gösterir (tam input→ekran gecikmesi değildir).
- Protokol veya `InputKey` yerleşimi değişince editör ve worker birlikte derlenir; sürüm / tuş sayısı uyuşmazlığı
  hata olarak bildirilir.

## Headless editör testleri

```powershell
cmake -S Engine -B build/editor-tests -G "Ninja Multi-Config" -DENGINE_BUILD_RUNTIME=OFF -DENGINE_BUILD_EDITOR_TESTS=ON
cmake --build build/editor-tests --config Debug --target EngineEditorTests
ctest --test-dir build/editor-tests -C Debug --output-on-failure
```

Viewport / DPI eşlemesi, input sahipliği, mesaj parçalanması, sürüm / oturum ayrımı, parametre doğrulama / geri
yükleme, kaynak kurmadan kayıt, worker çıkışından önce build yasağı ve paylaşımlı tamponu kapsar.
