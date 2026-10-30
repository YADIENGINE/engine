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
- **Araç sözleşmesi:** launcher'ın çağıracağı `Engine/Tools` giriş noktaları ve çıktıları (komutlar, seçenekler,
  çıkış kodları, JSON sonuç) belgelenir ve sabitlenir (D7, D15). Bugünkü `build_cli.py` / `editor_cli.py` bunun
  temelidir.

**Sonuç:** çevrimdışı derlenen, launcher'a hazır engine. Snake etkilenmez.

## Aşama 1: Editör döngüsü

- **Yanına derle, sonra değiştir** (D9): eski worker derleme boyunca çalışır; başarılı derlemede yenisine geçilir,
  durum geri gelir (seçim, parametreler, sunum saati, pause, kamera); hatada eski worker sürer.
- **Editör derlemenin sahibi** (D6'nın editör tarafı): editör dışarıdan derleme / test komutu kabul eden yerel bir
  kanal açar. İlk istemcisi bugünkü `p.py`'dir; launcher gelince `<engine>` aynı kanalı kullanır.

**Sonuç:** Snake'te günlük iş hemen hızlanır: AI `p.py --build` dediğinde açık editör derler ve önizleme kesilmeden
yenilenir; `LNK1104` çakışmaları biter.

## Aşama 2: Tuning

- `*.tuning.json` biçimi, derleme sırasında header üretimi, geliştirmede canlı okuma + dosya izleme, Release'de
  `constexpr` (D2).
- Editör: tuning değerlerini Inspector'da gösterme ve JSON'a geri yazma; desteklenen tipler (D11: uzamsal değerler
  sayı olarak).
- Snake'te pilot: birkaç View sabiti ve bir `Config` dosyası. Snake'in tamamının göçü ayrı iş.

**Sonuç:** değer ayarı derlemesiz ve kalıcı; editörde yapılan ayar git'te görünür.

## Aşama 3: Launcher MVP (yeni repo)

- Launcher reposu; engine içe aktarma betiği (D16) ve ilk `Engines/` kopyası.
- `<engine>` CLI: `build`, `test`, `run`, `release`, `editor`; işi projenin `Engine/Tools`'una gömülü Python ile devreder
  (D6, D7); editör açıksa Aşama 1'deki kanalı kullanır.
- Launcher UI: Projeler, Yeni proje, Sistem kontrolü (MSVC kurulumu), Ayarlar (D15).
- Kullanıcı modu: kopya + manifest + salt okunur denetimi; developer modu: submodule işlemleri (D4).
- Şablon: `p.py` / `editor.py` kalkar, AI dosyası yok, boş proje (D14). Snake `<engine>`'ye geçer.

**Sonuç:** projeyi launcher'dan kurmak, açmak ve derlemek; Python / CMake kurmadan.

## Aşama 4: Dağıtım

- Tek exe kurulum: `%LOCALAPPDATA%\<Engine>`, masaüstü kısayolu, PATH, Uygulamalar kaydı, üzerine kurulan
  güncelleme (D15).
- Engine güncellemesi: manifest notlarının gösterimi ve onaylı güncelleme (D16).

**Sonuç:** indirilip çalıştırılabilen ürün.

## Aşama 5: Görsel asset'ler (ayrı oturum)

- Katalog, galeri, pişirme önbelleği; grafik kodunun göçü ve düzenlenmesiyle birlikte (D10). Tuning'e (Aşama 2)
  dayanır; Aşama 3–4 ile paralel yürüyebilir.

## Aşama 6: Debug (v1'in sonu)

- `.vscode` şablonu, "Debugger'ı bekle", metin çökme raporu (D12).

## v1 dışı

- Ajan API'si (D5), sürüklenebilir uzamsal tuning (D11), flipbook / disk önbelleği (D10), otomatik güncelleme ve kod
  imzalama (D15), llvm-mingw (D8).
- **Paylaşılan bağımlılık önbelleği** (derleme süresi dert olursa): kaynaklar `ThirdParty/`'de kalır; launcher ilk
  derlenen bağımlılık kütüphanelerini (raylib, imgui, Catch2 ...) projeler arasında paylaşılan bir önbellekte
  tutar. Anahtar: engine sürümü + bağımlılık + yapılandırma (Debug / Release, runtime) + derleyici sürümü. Yeni
  proje ya da `--clean` bağımlılıkları yeniden derlemez. Hazır DLL / binary dağıtmanın yerine geçer: ABI ve runtime
  uyumu her makinede yerel derlemeyle sağlanır, Release yine tek exe kalır, kaynak debugger ve AI için okunur.
