# Release paketi ve üçüncü taraf bildirimleri

`python Engine release` Release derler ve oyuncuya verilecek paketi kurar. İşi `Engine/Tools/commands/release.py`
yapar; ayarlar `Project.json` `release` bölümündedir. Her proje aynı düzeni üretir:

```text
Release/                     (git'e girmez; şablonun .gitignore'unda)
  MyGame/
    MyGame.exe
    Assets/                  package_assets hook'unun seçtikleri (hook yoksa Assets/ olduğu gibi)
    ThirdPartyNotices.txt    ÜRETİLİR: Part 1 Engine + Part 2 oyun
    <release.files>          oyunun ek metinleri (ör. Credits.txt)
  MyGame.zip                 aynı klasör, mağazaya/oyuncuya giden dosya
```

## Project.json

```json
"release": {
  "directory": "Release",
  "name": "MyGame",
  "notices": "Game/Distribution/GameNotices.txt",
  "files": ["Game/Distribution/AudioCredits.txt"]
}
```

| Alan | Varsayılan | Anlamı |
| --- | --- | --- |
| `directory` | `Release` | paket ve ZIP'in yazıldığı klasör |
| `name` | proje `name` | paket klasörünün ve ZIP'in adı |
| `notices` | yok | bildirimin oyun kısmı (Part 2); yoksa dosyada sadece Part 1 olur |
| `files` | `[]` | paketin köküne aynen kopyalanan ek dosyalar |

`ThirdPartyNotices.txt` adında bir dosya `files`'a yazılamaz; o ad üretilen bildirime aittir.

## Bildirim iki parçadır

**Part 1: Engine** otomatik eklenir, oyun hiçbir şey yazmaz. Kaynağı `Engine/ThirdParty/`:
`Notices.json` bileşen listesi (ad, sürüm, metin dosyası, kaynaktaki sürüm pin'i, lisans dosyası) ve
`Notices/*.txt` lisans metinleri; üreten kod `ThirdParty/notices.py`. Engine ile derlenen her oyuna giren kütüphaneler buradadır (raylib ve içinde gelenler, JSON, şifreleme).
Sadece editörde ya da testte kullanılan bağımlılıklar (ImGui, Catch2) oyuna girmediği için listelenmez.

**Part 2: oyun** projenin `release.notices` dosyasıdır ve aynen eklenir. Oyunun kendi getirdiği her üçüncü taraf
şey burada ya da buradan işaret edilen bir dosyada anılır.

## Oyun tarafı: `Game/Distribution/`

`Game/Distribution/` oyuncuya giden bildirim metinlerini tutar. `Game/Assets/`'in kardeşidir ama pakete kendiliğinden
girmez; sadece `release.notices` ve `release.files` ile girer. `python Engine new` `Game/Distribution/GameNotices.txt`'i
"henüz üçüncü taraf içerik yok" satırıyla kurar; ilk dış içerik eklendiğinde bu satır silinir.

| Dosya | İçerik |
| --- | --- |
| `GameNotices.txt` (`release.notices`) | oyunun kod kütüphaneleri, fontları, görselleri, shader'ları; uzun listeler için ayrı dosyaya yönlendirme |
| `AudioCredits.txt`, `ArtCredits.txt` ... (`release.files`) | çok girişli içerik envanteri: dosya → eser, yazar, kaynak, lisans |
| `README.md` | geliştirici notu: açık kaynak soruları, doğrulama kayıtları (pakete girmez) |

Bir asset'in lisansı asset'in yanında dağıtılmak zorundaysa (ör. font lisansı `Assets/Fonts/OFL.txt`) dosya
asset'in yanında kalır, `package_assets` onu da seçmelidir ve `GameNotices.txt` ona yol verir.

### Ne zaman güncellenir

Oyuna dışarıdan gelen bir şey eklenen, değiştirilen ya da silinen her değişiklik aynı committe bildirimini de
günceller. Bu kural insan ve yapay zekâ için aynıdır: asset ya da kütüphane ekleyen değişiklik bildirimsiz bitmiş
sayılmaz.

| Değişiklik | Yapılacak |
| --- | --- |
| İndirilen font, görsel, ses, müzik, shader, model | `GameNotices.txt`'e ya da ilgili credits dosyasına giriş; lisans dosyası gerekiyorsa pakete koy |
| Oyunun kendi `FetchContent`/vendor kütüphanesi | `GameNotices.txt`'e bileşen başlığı + lisans metninin tamamı |
| Var olan dış içeriği düzenlemek (kesmek, yeniden kodlamak) | girişine "edited for use in the game" notu |
| Dış içeriği silmek | girişini sil |
| Projede üretilmiş içerik (kendi çizimin, `AudioGeneration` sesleri) | bildirim gerekmez; credits dosyası varsa "projede üretildi" diye ayrı tutulabilir |
| Engine'in getirdiği bir şey | hiçbir şey; Part 1 Engine'indir |

Lisansı ya da kaynağı belirsiz içerik pakete girmeden önce `Game/Distribution/README.md`'de açık soru olarak yazılır.
Araç metinlerin varlığını denetler, oyun içeriğinin lisans uygunluğunu denetlemez.

### Giriş biçimi

Düz metin, satır başına en fazla ~100 karakter, her giriş başlık + çizgi:

```text
Inter font
----------
Source: https://rsms.me/inter/
License: SIL Open Font License 1.1, see Assets/Fonts/OFL.txt
Files:
  Assets/Fonts/Inter-Medium.ttf
```

Birden fazla lisans seçeneği olan bileşende seçilen seçenek başlıkta yazılır (`stb_image.h (MIT option)`).
Lisans metni atıf istiyorsa (MIT, BSD, zlib, CC BY) metnin ve telif satırlarının tamamı, sadece bağlantı değil.
Credits girişlerinde dosya yolları paketteki `Assets/`'e göredir.

## Denetimler

Paketleme bir şey bulursa mevcut `Release/<name>/` klasörünü silmeden durur ve hepsini listeler:

- `release.notices` ya da `release.files` dosyası, ya da `package_assets`'in seçtiği bir asset derlenmiş
  `Assets/`'te yok (yeni asset eklendi ama Release derlenmedi).
- Engine bileşeninin `Notices.json`'daki sürümü kaynağındaki pin'de yok (bağımlılık yükseltilmiş, bildirim
  güncellenmemiş) ya da metin dosyasının ilk satırı o sürümü anmıyor.
- Kayıtlı metin bileşenin `ThirdParty/` içindeki ana lisans dosyasını aynen içermiyor.

Engine'in kendi denetimleri derleyici istemez, Engine kökünde: `python -m unittest discover -s ThirdParty/Tests`
(gerçek pin'leri ve lisansları okur) ve `python -m unittest discover -s Tools/Tests -t .` (paketleme dahil).

## Engine'de nerede

Paketleme `Tools/commands/release.py`'dedir; sistem büyüyecekse (imza, sürüm damgası, mağaza depot'u, başka platform
paketi) bu modül `Tools/commands/release/` alt paketine dönüşür. Bağımlılıklar ve bildirimleri `ThirdParty/`'dedir
([ThirdParty/README.md](../ThirdParty/README.md)).

```text
Engine/Tools/commands/release.py   package(): denetim → paket klasörü → ThirdPartyNotices.txt → ZIP
Engine/Tools/Tests/test_release.py
Engine/ThirdParty/
  <bileşen>/        değiştirilmemiş kaynaklar
  Notices.json      Part 1 bileşen listesi: ad, sürüm, metin, sürüm pin'i, lisans dosyası
  Notices/*.txt     bileşen başına lisans metinleri
  notices.py        denetim + Part 1 metni
  Tests/            Python unittest
```

## Engine bakımı: bağımlılık eklemek ya da yükseltmek

Tarif `ThirdParty/README.md`'dedir. Oyuna giren bir bağımlılık eklendiğinde ya da sürümü değiştiğinde aynı committe
`Notices/<bileşen>.txt` ve `Notices.json` güncellenir. Editör ya da test bağımlılığı oyuna girmez; `Notices.json`'a
yazılmaz.
