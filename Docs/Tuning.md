# Tuning dosyaları

Elle ince ayar yapılan sayılar (yarıçap, hız, renk, süre, olasılık, fiyat) C++ sabiti yerine class'ın yanındaki
`X.tuning.json` dosyasında durabilir. Değer editörde ya da dosyada değişince çalışan oyun onu derlemeden alır;
Release'de aynı değerler `constexpr` olarak derlenir. JSON tek kaynaktır: git'te görünür, elle, editörle ya da AI
ile düzenlenir. Sistem opt-in'dir; tasarım kararları engine reposunda `Docs/Design/Tuning.md`.

Yapısal sabitler (dizi boyutu, yuva sayısı, enum'a bağlı sayılar) tuning'e girmez, kodda kalır. Var olan
`constexpr` sabitleri taşımak gerekmez; iki yöntem yan yana yaşar.

## Kurulum

Hedef, tuning dosyalarını hangi klasörün altında aradığını bir kez bildirir:

```cmake
engine_add_tuning(MyGame ROOT Game)
```

`ROOT` altındaki her `*.tuning.json` derleme sırasında `<build>/Tuning/MyGame/<ROOT'a göre yol>.tuning.h` üretir; o
klasör hedefin include dizinidir. Yeni dosya CMake'e yazılmaz, bir sonraki build'de bulunur. Üretilen header repoya
girmez. Aynı hedefte aynı adlı iki tuning dosyası hatadır (struct'lar global). `python Engine new`'in kurduğu
projede oyun hedefi bunu zaten çağırır. Üretici Python 3.10+ ister (`python Engine` ile aynı).

## Dosya

`Game/Effects/Glow.tuning.json`:

```json
{
  "radius": { "value": 12.0, "min": 0, "max": 40 },
  "pulses": { "type": "int", "value": 3, "min": 1 },
  "tint": { "type": "color", "value": "#FF4080" },
  "offset": { "type": "vec2", "value": [0, -4] },
  "delay": { "type": "range", "value": { "min": 0.2, "max": 0.6 }, "min": 0 },
  "steps": { "type": "float[]", "value": [0, 0.5, 1] },
  "odds": {
    "type": "table",
    "columns": {
      "fromLevel": { "type": "int", "min": 1 },
      "chances": { "type": "float[]", "min": 0, "max": 100 }
    },
    "rows": [
      { "fromLevel": 1, "chances": [70, 30] },
      { "fromLevel": 5, "chances": [40, 60] }
    ]
  }
}
```

| `type` | Değer | C++ |
| --- | --- | --- |
| (yok, sayı) / `float` | `12.0` | `float` |
| `int` | `3` | `int` |
| (yok, true/false) / `bool` | `true` | `bool` |
| `color` | `"#RRGGBB"` / `"#RRGGBBAA"` | `Color4` (`Utilities/Color4.h`; çizimde `ToColor`) |
| `vec2` | `[x, y]` | `Float2` |
| `range` | `{ "min": a, "max": b }` | `FloatRange` |
| `T[]` | boş olmayan liste | `std::array<T, N>` |
| `table` | `columns` + `rows` | `std::array<<Alan>Row, N>` |

- Alan ve sütun adları camelCase'tir, struct üyesi olur. Dosya adı PascalCase'tir ve struct'ın adını verir:
  `Glow.tuning.json` → `GlowTuning`.
- `type` yazılmazsa sayı `float`, `true`/`false` `bool` olur (`3` kendiliğinden `int` olmaz); diğer tiplerde zorunlu.
- `min` / `max` geçerlilik sınırıdır (slider aralığı değil): listede her elemana, `vec2` / `range`'de her sayıya
  uygulanır; `bool` ve `color` sınır almaz.
- Tablo sütunu tek değerli tip ya da liste olabilir; tablo içinde tablo yok. Liste sütununun uzunluğu her satırda aynı.
- Liste uzunluğu ve satır sayısı yapısaldır (`std::array` boyutu). Enum'a bağlı bir listenin uzunluğu kullanıldığı
  yerde denetlenir: `static_assert(std::tuple_size_v<decltype(GlowTuning::steps)> == StepCount);`
- "Şanslar toplamı 100" gibi alanlar arası kurallar dosyaya girmez; kodda ya da testte denetlenir.

## Kod

```cpp
#include "Effects/Glow.tuning.h"

void GlowView::Draw() const {
    const auto& tuning = Tuning<GlowTuning>();
    DrawCircleV(_center, tuning.radius, ToColor(tuning.tint));
}
```

`Tuning<T>()` her iki modda da aynıdır ve `const T&` döner; referansı karenin içinde kullanın, saklamayın (değerler
geliştirmede yenilenir). Testler değeri struct'tan okur, literal yazmaz.

| | Geliştirme (Debug, editör) | Release yapılandırmaları |
| --- | --- | --- |
| Değerin kaynağı | İlk `Tuning<T>()` dosyayı okur; dosya derlendiğinden beri değiştiyse yeni değer | Derlenmiş `constexpr` |
| Dosya değişince | Game her karenin başında bakar (en çok 200 ms'de bir), ya hep ya hiç uygular | — |
| Maliyet | Struct alanı okumak | Sabit |

Mod `ENGINE_TUNING_LIVE` tanımıdır (Release, MinSizeRel, RelWithDebInfo'da 0). Geliştirme exe'si dosyayı derlendiği
kaynak yolundan okur; proje klasörü taşınırsa yeniden derleyin. Canlı değerler ana iş parçacığında yenilenir.

## Değişiklik ne zaman derleme ister

- Var olan değeri değiştirmek: derleme yok.
- Alan / sütun eklemek ya da silmek, tipini değiştirmek, liste uzunluğunu ya da satır sayısını değiştirmek: struct
  değişir; çalışan oyun dosyayı uygulamaz, "build to apply" bildirir. Derleyince geçer.

## Hatalar

Hiçbir değer sessizce kırpılmaz ya da yarım uygulanmaz.

- **Derleme:** bozuk JSON, yanlış tip, sınır dışı değer, eksik sütun derlemeyi `dosya:satır: error: ...` ile durdurur.
- **Çalışırken:** geçersiz dosya uygulanmaz, son geçerli değerler sürer; hata stderr'e (editörde Output'a ve Tuning
  paneline) `dosya:satır` ile yazılır.
- **Açılışta:** dosya geçersizse oyun derlendiği andaki değerlerle açılır ve uyarır.

## Editör

Tuning paneli projenin bütün tuning dosyalarını listeler (build'in `Tuning/TuningFiles.json`'u; yeni dosya bir sonraki
configure'dan sonra görünür). Sayılar kutuya yazılır ve Enter'la girer; Enter'sız yazılan kutudan çıkınca geri
alınır. Sınır dışı değer kabul edilmez, kutu kırmızı kalır. Değer diskteki dosyaya tek satırlık bir değişiklikle
yamanır; önizleme dosyayı kendisi okur. Panel önizlemenin durumunu gösterir: Live, Not applied (satırıyla),
Build needed (Build & Refresh düğmesiyle). Ayrıntı: [Editör kullanımı](Editor/Usage.md).

## Biçim denetimi

Dosyaların tek bir yazım biçimi vardır: 2 boşluk girinti, alanlar dosyadaki sırayla ve her biri kendi satırında, alan
içi anahtarlar `type, value, min, max` (tablo: `type, columns, rows`) sırasında, tablo satırları satır başına bir.
Editör hep bu biçimde yazar. `python Engine check` geçersiz ya da biçim dışı tuning dosyalarını bildirir;
`python Engine check --fix` geçerli dosyaları biçime sokar (sayılar yazıldığı gibi kalır).

## Engine tarafı

- Üretici ve denetim: `Tools/tuning/` (`python -m Tools.tuning generate <json> <header>`, sadece standart kütüphane).
- Biçim, doğrulama, yamama: `Core/Tuning/TuningFile.h`; canlı taraf: `Core/Tuning/TuningLive.h`; erişim:
  `Core/Tuning/Tuning.h`.
- Kurallar Python'da ve C++'ta iki kez uygulanır; `Tools/Tests/TuningCases/` iki tarafın da koştuğu ortak örneklerdir.
  Kuralı değiştiren ikisini ve örnekleri birlikte değiştirir.
