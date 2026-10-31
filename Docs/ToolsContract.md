# Araç sözleşmesi

Engine'in bütün araçları tek komut satırındadır. Launcher (ve her türlü otomasyon: AI ajanı, CI) engine'le sadece bu
belgedeki komutlar üzerinden konuşur; launcher projenin kendi `Engine/`'indeki araçları çalıştırır, bu yüzden farklı
engine sürümlü projeler yan yana çalışır. Bu belgede olmayan her şey (modül adları, iç fonksiyonlar, çıktı metinleri)
serbestçe değişebilir.

## Çağırma

```powershell
python Engine [--project KÖK] <komut> [seçenekler]      # proje kökünden
python . [--project KÖK] <komut> [seçenekler]           # engine reposunun kökünden
```

`Engine` klasörü `__main__.py` taşıdığı için Python onu doğrudan çalıştırır. Python 3.10+, sadece standart kütüphane.
Araçlar yalıtılmış bir yorumlayıcıyla da çalışır (`python -I`, launcher'ın `._pth`'li gömülü Python'u): çalışma
klasörünün `sys.path`'te olmasına güvenmez ve CMake'e kendilerini çalıştıran yorumlayıcıyı verir
(`Python3_EXECUTABLE`), böylece derleme adımları da aynı Python'u kullanır.
Proje kökü: `--project` verilmişse o; yoksa engine'in üst klasörü (`Project.json` varsa), değilse engine'in kendisi.
`--project` komuttan önce yazılır.

## Komutlar

| Komut | Seçenekler | İş |
| --- | --- | --- |
| `build` | `--release`, `--clean` | Oyunu derler (Debug; `--release` ile Release). `--clean` önce build klasörünü siler |
| `run` | `--release` | Derler ve oyunu çalıştırır; oyun kapanınca döner |
| `release` | | Release derler, `Release/<ad>/` paket klasörünü ve ZIP'i kurar ([Release](Release.md)) |
| `test` | `--filter RE`, `--label RE`, `--list`, `--clean` | Headless testleri derler ve koşar; rapor `<tests.buildDirectory>/reports/<run-id>/results.json` |
| `editor` | `--release`, `--build-only`, `--clean` | Editörü ve projenin önizleme worker'ını derler, editörü açar |
| `refresh` | | Projenin açık editöründe Build & Refresh (sürüyorsa ona katılır); çıktısını akıtır, sonucuyla çıkar. Önizleme derleme boyunca açık kalır. Editör açık değilse 2 ([kontrol kapısı](Editor/Architecture.md#kontrol-kapısı)) |
| `new` | `--name AD`, `--dry-run` | Engine'in üst klasörünü yeni proje olarak kurar; `--name` yoksa adı sorar |
| `info` | | Engine ve proje bilgisi, JSON (aşağıda) |
| `check` | `--fix` | Internals include sınırı ([API boundaries](Contributing/ApiBoundaries.md)) ve tuning dosyalarının geçerliliği / kanonik biçimi ([Tuning](Tuning.md)); `--fix` geçerli tuning dosyalarını biçime sokar |

Ayarlar `Project.json`'dadır ([ProjectSetup](ProjectSetup.md)).

## `info` çıktısı

```json
{
  "engine": { "name": "...", "version": "0.1.0", "root": "C:\\...\\MyGame\\Engine" },
  "toolsContract": 1,
  "project": { "name": "MyGame", "root": "C:\\...\\MyGame" }
}
```

Proje kökünde `Project.json` yoksa `project` `null`'dır.

## Manifest

`EngineManifest.json` engine'in kimliğidir ve sözleşmenin parçasıdır: launcher proje listesini doldururken her
proje için Python başlatmaz, `Engine/EngineManifest.json`'u doğrudan okur ve `toolsContract`'ı denetler.

```json
{
  "name": "...", "version": "0.1.0", "toolsContract": 1,
  "repository": "https://.../engine.git",
  "notes": [ { "version": "0.1.0", "date": "2026-10-31", "changes": ["..."],
               "breaking": [ { "change": "...", "migrate": "..." } ] } ]
}
```

| Alan | Anlamı |
| --- | --- |
| `version` | `ANA.ARA.YAMA`. 0.x'te ARA, 1.0'dan sonra ANA artışı kırıcı değişikliktir: önceki sürümle derlenen bir proje elle değişiklik ister |
| `toolsContract` | bu belgenin sürümü |
| `repository` | engine reposunun adresi (developer modunda submodule kaynağı) |
| `notes` | birikimli güncelleme notları, en yeni üstte; ilki `version`'dır. `breaking` kırıcı değişiklikler ve nasıl uyulacağı (`migrate`); yoksa boş liste. Launcher güncellemede aradaki bütün notları gösterir |

Kurallar `Tools/manifest.py` `note_problems`'tadır; `Tools/Tests/test_manifest.py` gerçek manifest'i denetler, notsuz
sürüm artırılamaz. Launcher'ın kullanıcı kopyasında yanında üretilmiş `EngineCopy.json` (kaynak tag / commit, dosya
özetleri) durur; engine araçları onu okumaz.

## Ayrılmış ad

`<engine> launcher ...` launcher'ın kendi komutlarıdır; launcher geri kalan her argümanı projenin engine'ine aynen
devreder. Engine `launcher` adında komut eklemez (`test_cli.py` denetler).

## Çıkış kodları

| Kod | Anlamı |
| --- | --- |
| 0 | Başarı |
| 1 | Projenin işi başarısız: derleme hatası, başarısız test, release denetimi, oyun / editör hata koduyla kapandı, `check` ihlal ya da tuning sorunu buldu, `refresh` başarısız ya da iptal edildi |
| 2 | Kullanım ya da altyapı: yanlış argüman, eksik araç (MSVC, CMake, Ninja), bozuk `Project.json`, test raporu kurulamadı |
| 130 | Ctrl+C ile kesildi |

Hata mesajı stderr'e `error: ...` olarak yazılır.

## Sözleşmeyi değiştirmek

- Yeni komut, yeni seçenek ya da `info`'ya yeni alan eklemek sözleşmeyi bozmaz.
- Yeni manifest alanı eklemek bozmaz.
- Komut / seçenek silmek ya da anlamını değiştirmek, çıkış kodlarını, `info` ya da manifest alanlarını değiştirmek
  sözleşmeyi bozar: `EngineManifest.json` `toolsContract` bir artar ve bu belge aynı committe güncellenir.
- Sözleşme testi: `Tools/Tests/test_cli.py` (komut ve seçenek listesi, `info`, çıkış kodları).
