# v1 iş sırası

Durum: kabul edildi (2026-10-31). Kararlar: [README](README.md). Her aşama tek başına işe yarar bir sonuç verir;
sonraki aşama öncekinin üstüne kurulur.

## Bağımlılıklar

```
Aşama 0 Hazırlık ──┬─► Aşama 1 Editör döngüsü ──┐
                   ├─► Aşama 2 Tuning ───────────┼─► Aşama 3 Launcher MVP ─► Aşama 4 Dağıtım ─► Aşama 6 Debug
                   │                             │
                   │         Aşama 2 ────────────┴─► Aşama 5 Görsel asset'ler (ayrı oturum, paralel olabilir)
```

## Aşama 0: Hazırlık (sadece engine)

- ~~Android'in kaldırılması (D13)~~ yapıldı.
- ~~Engine adı ve sürümü tek kaynakta: `EngineManifest.json`~~ yapıldı.
- ~~Bağımlılıkların engine'e alınması~~ yapıldı: `ThirdParty/` (raylib, nlohmann_json, Monocypher, imgui, rlImGui,
  Catch2), `FetchContent` kalktı; lisans bildirimi ve denetimi de `ThirdParty/`'ye taşındı (D8).
- ~~Engine klasör düzeni~~ yapıldı: oyuna giren C++ `Core/` (include kökü), editör `Editor/`, Python araçları
  `Tools/`, bağımlılıklar `ThirdParty/`; headless hedef `EngineHeadless` (eski `EngineCore`).
- ~~Araç sözleşmesi~~ yapıldı (D7, D15): tek giriş noktası `python Engine <komut>` (`__main__.py` → `Tools/cli.py`);
  `build`, `run`, `release`, `test`, `editor`, `new`, `info`, `check`; çıkış kodları 0 / 1 / 2;
  `EngineManifest.json` `toolsContract: 1`; belge `Docs/ToolsContract.md`, sözleşme testi `Tools/Tests/test_cli.py`.
  Projelerdeki `p.py` / `editor.py` ve `EngineSetup.py` kalktı (`new` komutu).

**Sonuç:** çevrimdışı derlenen, launcher'a hazır engine.

## Aşama 1: Editör döngüsü

Uygulandı (2026-10-31). Ayrıntı: [Editor architecture](../Editor/Architecture.md).

- ~~Yanına derle, sonra değiştir~~ (D9): worker'lar build'in snapshot kopyalarından çalışır; Build & Refresh
  sırasında önizleme açık kalır, yeni build eskisinin yanında aynı durumla (önizleme, parametreler, pause, hız)
  başlar ve hazır olunca yerine geçer; hata ya da iptalde eski önizleme sürer. Kamera zaten host'tadır. Sunum saati
  bilerek aktarılmaz: sahne durumu Update ile ilerlediği için saatten yeniden kurulamaz.
- ~~Editörün kontrol kapısı~~ (D6'nın editör tarafı): proje başına named pipe, build dizininde duyurulur, satır başına
  JSON. İlk istemcisi `python Engine refresh`; launcher ve ajan API'si (D5) yeni komutları aynı kapıya ekler.
- Bulgu: oyun (`build/`) ve editör (`build/<editör>`) ayrı klasörlerde derlendiği için `python Engine build` editörle
  hiç çakışmıyordu; çakışma yalnız editörün kendi worker build'indeydi ve snapshot'larla bitti.

**Sonuç:** kod değişince `python Engine refresh` (ya da Build & Refresh) önizlemeyi kesmeden yeniler.

## Aşama 2: Tuning

Uygulandı (2026-10-31). Kullanım: [Tuning](../Tuning.md); kararlar: [Tuning tasarımı](Tuning.md).

- ~~`*.tuning.json` biçimi, header üretimi, canlı okuma + dosya izleme, Release'de `constexpr`~~ (D2): üretici
  `Tools/tuning/`, CMake `engine_add_tuning`, çalışma zamanı `Core/Tuning/`, `check --fix`.
- ~~Editör~~: ayrı Tuning paneli (Enter ile giriş, diske atomik yama), worker'ın canlı durumu protokol v8 `tuning`
  mesajıyla; tipler D11'e uygun (uzamsal değerler sayı).
- Göç yok: tuning yeni kod içindir; var olan projeler `constexpr` sabitlerinde kalır. Doğrulama engine testleri
  (Python ve C++ ortak örnekler) ve `new` ile kurulan deneme projesiyle; proje şablonu `engine_add_tuning`'i çağırır.

**Sonuç:** değer ayarı derlemesiz ve kalıcı; editörde yapılan ayar git'te görünür.

## Aşama 3: Launcher MVP (yeni repo)

Uygulandı (2026-10-31, launcher `c571dd3` "Launcher UI", push'lu). Çekirdek + `<engine>.com` + `<engine>.exe`
ekranları derlendi; `LauncherCoreTests` ve ekran görüntüsü üreten `LauncherUiSmoke` (ölçek 75–200 %, taşma denetimi)
çalıştı. Ayrıntı launcher `Docs/Architecture.md`, `Docs/CoreApi.md`, `Docs/UiScreens.md`. Aşama 4'e geçenler:
var olan oyunun geçişi (kurulu launcher ister) ve "ses araçlarını kur" (D7).

- Launcher reposu; engine içe aktarma betiği (D16) ve ilk `Engines/` kopyası.
- `<engine>` CLI: `build`, `test`, `run`, `release`, `editor`; işi projenin `Engine/Tools`'una gömülü Python ile devreder
  (D6, D7); editör açıksa Aşama 1'deki kanalı kullanır.
- Launcher UI: Projeler, Yeni proje, Sistem kontrolü (MSVC kurulumu), Ayarlar (D15).
- Kullanıcı modu: kopya + manifest + salt okunur denetimi; developer modu: submodule işlemleri (D4).
- Şablon: `p.py` / `editor.py` kalkar, AI dosyası yok, boş proje (D14).
- Var olan oyunun geçişi (2026-10-31): oyun listeye "Var olanı ekle" ile submodule projesi olarak girer; oyunun
  belgeleri `python Engine <komut>` yerine `<engine> <komut>` yazar. Belgeler launcher kurulup (`<engine>` PATH'te)
  kabul testi geçince değişir: `<engine> build`, `<engine> test`, editör açıkken `<engine> refresh` ve
  `python Engine` aynı sonucu verir. `python Engine` sözleşme olarak kalır (launcher onu çağırır).
- Oturumlar: (1) UI dışı çekirdek: repo, içe aktarma, CLI, gömülü araçlar, sistem kontrolü, ayarlar, proje
  işlemleri, modlar; (2) ekranlar, çekirdeğin public API'si üstüne.
- (1) yazıldı (2026-10-31), derleme / test onayı bekliyor: launcher reposu `YADIEN` (`Engine/` submodule,
  `Engines/Engine0.1` = `v0.1.0`), `LauncherCore` + `<engine>.com` + testler. UI session'ının girişi launcher
  reposunda `Docs/CoreApi.md`. Engine tarafı: manifest `repository` + not kuralları, `export-ignore`, yalıtılmış
  Python ile çalışan araçlar (`Python3_EXECUTABLE`, tuning üreticisi `-m`'siz).
- UI hazırlığı (bitti): engine'de ortak araç arayüzü `ToolUi/` (`EngineToolUi`, editör
  de kullanır) ve editörün "Back to project list"i (`ENGINE_LAUNCHER`); launcher'da `Job<T>`, Aç / Run akışları,
  proje satırı modeli, Scan, PATH, klasör seçme penceresi ve `Apps/Ui` iskeleti (tek model + boş ekran
  fonksiyonları). Kararlar D15 "UI hazırlığı". Kalan: ekranların görsel tasarımı (launcher `Docs/UiScreens.md`).

**Sonuç:** projeyi launcher'dan kurmak, açmak ve derlemek; Python / CMake kurmadan.

## Aşama 4: Dağıtım

Başladı (2026-10-31). Açık sorular [README](README.md#açık-sorular)'de, tek tek kapanır.

- ~~Açık kaynağa geçiş (D19)~~ yapıldı (2026-10-31): `LICENSE` (MIT) + İngilizce README'ler, developer modu
  şifresiz, repolar `YADIENGINE/engine` ve `YADIENGINE/yadien` olarak public, kullanıcıya dönük belgeler
  İngilizce. Yayın elle (`dev.py package` + Releases'a yükleme).
- ~~Velopack deneme paketi~~ yapıldı (2026-10-31, bulgular D15).
- ~~Tek exe kurulum (D15)~~ yapıldı (2026-10-31; kuruldu, kullanıldı, kaldırıldı; ikon ve açılış görseli koddan;
  ayrıntı launcher `Docs/Packaging.md`): (1) launcher entegrasyonu: `velopack_libc`, `VelopackApp::Run()`, kurulum /
  kaldırma hook'larında PATH ve günlük, `Settings.json` `current/`'in dışına (`LauncherLayout`); (2) paket betiği
  `dev.py package`: Release build + budanmış CMake (`Toolchain.lock.json` `exclude`) + `Engines/` + gömülü
  araçların lisans metinleri → `vpk pack`, imza adımının yeri boş; (3) Ayarlar'daki Licenses bölümü paketteki
  bütün lisansları gösterir.
- Engine güncellemesi (D16): notların gösterimi ve onaylı güncelleme Aşama 3'te yapıldı (`PlanEngineUpdate` /
  `ApplyEngineUpdate`, Engine update paneli). **TODO (ertelendi 2026-11-01):** iki gerçek sürümle uçtan uca deneme;
  0.1 pakete girmediği için 0.2.0'dan sonraki ilk engine sürümüyle (0.2.x ya da 0.3) yapılır.
- ~~"Ses araçlarını kur" (D7)~~ yapıldı (2026-10-31): Ayarlar'da düğme + `launcher audio-tools`, betik için
  `launcher python`.
- ~~Kabul testi~~ geçti sayıldı (2026-11-01; kurulum / kullanım / kaldırma elle denendi, çıkan hata sonra ele alınır).
  Var olan oyunun geçişi: oyunun belgeleri `<engine> <komut>`'a geçer.
- ~~İlk yayın~~ yapıldı (2026-11-01): engine `v0.2.0`, launcher `v0.1.0` GitHub'da Pre-release (notu "Early
  preview. For testing."; sadece ekip içi deneme, duyurulmaz). Paket kuruldu, CLI ile denendi (yeni proje derlendi),
  pencere elle kontrol edildi, kaldırıldı. Sonraki yayın `build/Package/Releases/`'i saklar (delta paketi).
  Karar ve akış (kayıt için): engine'in bugünkü
  `main`'i **0.2.0** olarak tag'lenir (Painter kırıcı; MIT `LICENSE`, İngilizce belgeler, yeni adres) ve launcher'a
  `Engine0.2` olarak alınır; `Engine0.1` (0.1.1: lisanssız, Türkçe belgeli, eski adresli) pakete girmez. Şart:
  `main` kullanıcıya verilecek durumda (Painter göçü bitti, oyun derlenip çalışıyor). GitHub'da **Pre-release**
  + "erken önizleme" notu (yalnız Windows, imzasız, 0.x kırabilir, ilk açılışta MSVC kurulur); launcher README'sinin
  indirme bağlantısı `/releases` (`/releases/latest` ön sürümleri atlar).
  Akış: (1) engine manifest'ine 0.2.0 notu (kırıcı: Painter; MIT, İngilizce belgeler) → testler → `v0.2.0` tag'i;
  (2) launcher'da `import_engine.py v0.2.0`, `Engines/Engine0.1` silinir, README bağlantısı; (3) `dev.py package`,
  kur / dene / kaldır; (4) launcher `v0.1.0` tag'i + `gh release create --prerelease` (Setup + güncelleme dosyaları,
  notlar: içerir engine 0.2.0); (5) oyunun pointer'ları.

**Sonuç:** indirilip çalıştırılabilen ürün.

## Aşama 5: Görsel asset'ler (ayrı oturum)

- [Asset library](AssetLibrary.md) (D22): Assets paneli, grafik kümeleri, küçük resimler, ses önizleme. Prefab ertelendi.
- Pişirme önbelleği (D10, [Visual assets](VisualAssets.md)) ertelendi; önce ölçüm gerekir.

## Aşama 6: Debug (v1'in sonu)

- `.vscode` şablonu, "Debugger'ı bekle", metin çökme raporu (D12).

## v1 dışı

- Ajan API'si (D5), sürüklenebilir uzamsal tuning (D11), flipbook / disk önbelleği (D10), CI/CD (GitHub Actions: derleme + test, tag'den yayın) ve SignPath imzası
  (D19; ertelendi), llvm-mingw (D8).
- **Paylaşılan bağımlılık önbelleği** (derleme süresi dert olursa): kaynaklar `ThirdParty/`'de kalır; launcher ilk
  derlenen bağımlılık kütüphanelerini (raylib, imgui, Catch2 ...) projeler arasında paylaşılan bir önbellekte
  tutar. Anahtar: engine sürümü + bağımlılık + yapılandırma (Debug / Release, runtime) + derleyici sürümü. Yeni
  proje ya da `--clean` bağımlılıkları yeniden derlemez. Hazır DLL / binary dağıtmanın yerine geçer: ABI ve runtime
  uyumu her makinede yerel derlemeyle sağlanır, Release yine tek exe kalır, kaynak debugger ve AI için okunur.
