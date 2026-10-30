# Tuning dosyaları (tasarım)

Durum: kabul edildi, ayrıntılar kapandı (2026-10-31), uygulanmadı. Karar kaydı: [README → D2](README.md#d2-ayar-değerleri-tuning-dosyaları).

## Amaç

Editörde slider ile değiştirilen bir değer kalıcı olmalı, git'te görünmeli ve AI tarafından okunup
değiştirilebilmeli. Editör C++ kaynağını düzenlemez; ayar değerleri ayrı bir metin dosyasında durur.

## Dosya düzeni

Ayarlanabilir değeri olan class'ın yanında, aynı adla:

```
MyEffect.hpp
MyEffect.cpp
MyEffect.tuning.json
```

- **Opt-in:** ayar değeri olmayan class'ın tuning dosyası olmaz.
- Ad her zaman `X.tuning.json`; araç bu kalıptaki dosyaları otomatik bulur, CMake'e elle eklenmez.
- **Girer:** elle ince ayar yapılan sayılar: yarıçap, hız, renk, süre, oran, ve oynanış dengesi değerleri
  (puan, hız, olasılık, fiyat).
- **Girmez:** yapısal sabitler: dizi boyutu, yuva sayısı, enum'a bağlı sayılar. Bunlar `.cpp` / `.hpp`'de kalır.

## Biçim

JSON tek kaynaktır: alan adı, tipi, değeri ve editör bilgisi (aralık, tip ipucu) aynı dosyadadır.

```json
{
  "glowRadius": { "value": 12.0, "min": 0, "max": 40 },
  "pulseSpeed": { "value": 2.5, "min": 0, "max": 10 },
  "tint": { "value": "#FF4080", "type": "color" }
}
```

## Üretilen header

Derleme sırasında engine aracı her tuning dosyasından bir header üretir. Bu dosya `build/` altındadır,
elle yazılmaz ve repoya girmez:

```cpp
// MyEffect.tuning.h (üretilir)
struct MyEffectTuning {
    float glowRadius = 12.0f;
    float pulseSpeed = 2.5f;
    Color4 tint = {255, 64, 128, 255};
};
```

Kod değeri okur:

```cpp
#include "MyEffect.tuning.h"

void MyEffect::Draw() const
{
    const auto& t = Tuning<MyEffectTuning>();
    DrawGlow(_pos, t.glowRadius, t.tint);
}
```

## Geliştirme ve Release

| | Geliştirme (editör / Debug) | Release |
| --- | --- | --- |
| Değerin kaynağı | Açılışta JSON bir kez okunur, struct'a yazılır | JSON değerleri `constexpr` olarak koda gömülür |
| Editörde değer değişince | Editör JSON'a yazar, çalışan oyun dosyayı yeniden okur; derleme yok | — |
| Kare başına maliyet | Bir struct alanını okumak | Yok; bugünkü `constexpr` sabitle aynı |
| Exe yanında JSON | Var | Yok |

JSON her karede okunmaz; sadece açılışta ve dosya değiştiğinde okunur.

## Kurallar

- Var olan bir değeri değiştirmek derleme istemez. Yeni alan eklemek ya da alan silmek struct'ı değiştirir,
  derleme ister.
- Testler değeri tuning struct'ından okur, literal yazmaz (bugün `Config` sabitlerinde olduğu gibi).
- Release'de geliştirme moduna ait okuma / izleme kodu derlenmez.

## Tipler

| JSON | C++ | Inspector |
| --- | --- | --- |
| `12.0` | `float` | sayı kutusu |
| `3`, `"type": "int"` | `int` | tamsayı kutusu |
| `true` | `bool` | onay kutusu |
| `"#FF4080"`, `"type": "color"` | `Color4` | renk seçici |
| `[1.5, 2.0]`, `"type": "vec2"` | `Float2` | iki sayı kutusu |
| `{ "min": 1, "max": 3 }`, `"type": "range"` | `FloatRange` | iki sayı kutusu |

- Üretilen header headless'tır (mantık ve `Config` de kullanır): `Color4` bugün yok, `Utilities/`'e raylib'siz
  eklenir, raylib `Color`'a çevirisi `Graphics/`'te.
- `float` ve `bool` değerden anlaşılır; diğer tiplerde `"type"` zorunludur (`3` kendiliğinden `int` olmaz).
- Slider yok: sayılar kutuya yazılır, değer Enter'a basınca girilir (yazarken ara değer uygulanmaz, kutudan
  çıkınca Enter'sız yazılan geri alınır). `min` / `max` slider aralığı değil, geçerlilik sınırıdır.
- Enum seçimi v1'de yok: üretilen header oyunun enum'unu bilemez, JSON'da değer listesi C++ ile çift kaynak olur.
  Enum'ların da tuning'den tanımlandığı bir model ayrı tasarım konusudur; o zamana kadar enum'a bağlı değer kodda kalır.

## Liste ve tablolar

Dengenin çoğu tablodur; üç biçim de girer:

```json
{
  "scoreBonusPercents": { "type": "int[]", "value": [10, 15, 20, 25] },
  "tierLuck": { "type": "float[]", "value": [0.0, 0.5, 1.0, 2.0] },
  "tierOdds": {
    "type": "table",
    "columns": {
      "fromWorld": { "type": "int", "min": 1 },
      "chances": { "type": "float[]", "min": 0, "max": 100 }
    },
    "rows": [
      { "fromWorld": 2, "chances": [70, 30, 0, 0] },
      { "fromWorld": 5, "chances": [55, 35, 10, 0] }
    ]
  }
}
```

- **Liste** (`T[]`, `T` tek değerli tiplerden biri): `std::array<T, N>`; `min` / `max` her elemana uygulanır.
- **Enum'a bağlı liste** (kademe başına değer) aynı biçimdir; enum JSON'a girmez, uzunluk kullanıldığı yerde
  denetlenir: `static_assert(t.tierLuck.size() == TierCount)` (geliştirmede de `std::array` sabit boyutludur).
- **Tablo:** sütunlar bir kez tanımlanır; üretici satır struct'ı (`<Alan>Row`) ve `std::array<Row, N>` üretir.
  Sütun tek değerli tip ya da liste olabilir; tablo içinde tablo yok.
- **Uzunluk yapısaldır:** eleman / satır sayısını değiştirmek alan eklemek gibi derleme ister. Inspector sadece
  değer düzenler (tablo: hücreleri sayı kutusu olan ızgara); satır eklemek / silmek JSON'da yapılır.
- Tablolar arası kurallar ("şanslar toplamı 100") JSON'a girmez; oyunun mantığıdır, kodda ya da testte denetlenir.

## Üretilen header'ın yeri

Header'ın yolu JSON'un yolunun aynısıdır, build içinde:

```
Game/Configs/WorldConfig.tuning.json
build/<preset>/Tuning/Game/Configs/WorldConfig.tuning.h
```

```cpp
#include "Configs/WorldConfig.tuning.h" // kaynak header'larla aynı kök ve yazım
```

- **Sahiplik:** tuning dosyası, kaynak kökü onu içeren CMake hedefine aittir (`Core/` → `EngineHeadless`, oyunun
  kaynakları → oyun hedefi). Üretim klasörü o hedefin include dizinine eklenir.
- **Bulma:** configure sırasında hedefin kaynak kökünde `*.tuning.json` taranır (`CONFIGURE_DEPENDS`); CMake'e elle
  yazılmaz.
- **Üretim:** JSON başına bir `add_custom_command`; üretici `Tools/` içinde, sadece standart kütüphane. İçerik
  değişmediyse header'a dokunulmaz (gereksiz derleme yok). Configure sırasında da bir kez üretilir (IntelliSense).
- **İsim:** struct `<DosyaAdı>Tuning`. Aynı hedefte aynı adlı iki tuning dosyası üretim hatasıdır (struct'lar global).
- **Erişim:** iki modda da `Tuning<XTuning>()` (`const&`); geliştirmede canlı nesne, Release'de `constexpr` örnek.

## Hatalı değer

Geliştirme ve Release'de kural aynıdır; hiçbir değer sessizce kırpılmaz ya da yarım uygulanmaz.

- **Derlemede:** bozuk JSON, yanlış tip, `min` / `max` dışı değer, eksik tablo sütunu üretimi durdurur
  (`dosya:satır: açıklama`). Derlenmiş her exe'nin değerleri geçerlidir.
- **Çalışırken dosya değişince:** dosya ya tamamen uygulanır ya hiç; tek hata varsa son geçerli değerler sürer, hata
  editörde ve konsolda `dosya:satır` ile görünür. Sınır dışı değer reddedilir, kırpılmaz.
- **Yapısal fark** (alan eklendi / silindi / adı değişti, uzunluk değişti): hiçbir şey uygulanmaz, "derleme gerekli"
  bildirilir; editör Build & Refresh'e yönlendirir.
- **Açılışta geçersiz JSON:** üretilen header derlendiği andaki değerleri varsayılan taşır; oyun onlarla açılır ve
  uyarır. Tuning yüzünden açılış çökmez.
- **Inspector:** geçersiz ya da sınır dışı değeri Enter'da kabul etmez (kutu kırmızı, değer geri döner, dosyaya
  yazılmaz). Hatalı değer dosyaya yalnız elle ya da ajan düzenlemesiyle girebilir.

## Yazma ve eşzamanlılık

- **Tek yol dosyadır:** editör değeri çalışan oyuna ayrıca göndermez, JSON'a yazar; oyun dosyayı izler. Elle, ajanla
  ve editörle yapılan değişiklik aynı yoldan geçer. Worker build snapshot'ından çalışsa da tuning dosyalarını kaynak
  ağacından okur (kaynak kökü worker'a açılışta verilir).
- **Kanonik biçim:** 2 boşluk girinti; alan sırası dosyadaki sıradır (yeniden sıralanmaz); her alan kendi satırında,
  kısa listeler tek satırda, tablo satırları satır başına bir. Editör hep bu biçimde yazar, tek değer değişikliği tek
  satırlık diff'tir. `python Engine check` biçime uymayan tuning dosyalarını bildirir, `--fix` düzeltir.
- **Editör yazarken** (Enter): dosyayı diskten yeniden okur, sadece değişen alanı yamar, geçici dosyaya yazıp atomik
  olarak yerine koyar. Diskteki dosya o an geçersizse yazmaz, hatayı gösterir. Aynı alana iki taraf yazarsa son yazan
  kazanır.
- **Inspector** dosya dışarıdan değişince tazelenir; kutuda Enter'lanmamış yazı silinmez, Enter'da yeni dosyanın
  üzerine yamanır.

## Var olan kod

Göç yapılmaz: tuning yeni yazılan kod içindir. Var olan `constexpr` sabitleri ve `Config` tabloları olduğu gibi
kalır; iki yöntem aynı projede yan yana yaşayabilir (sistem opt-in'dir, tuning dosyası olmayan hedefe hiçbir şey
eklenmez). Kalıcı göç aracı yazılmaz. Yeni kodda adlandırma: alanlar camelCase (`baseProgressToComplete`), dosya
değerleri kullanan header / class'ın yanında.

## Açık ayrıntılar

- ~~Editörün dosya biçimini koruması; ajan ile aynı anda yazma~~ → [Yazma ve eşzamanlılık](#yazma-ve-eşzamanlılık).
- ~~Geçersiz JSON ya da aralık dışı değer~~ → [Hatalı değer](#hatalı-değer).
- ~~Tablo / dizi değerleri~~ → [Liste ve tablolar](#liste-ve-tablolar).
- ~~Üretilen header'ın yolu~~ → [Üretilen header'ın yeri](#üretilen-headerın-yeri).
- ~~Var olan sabitlerin taşınma yolu~~ → [Var olan kod](#var-olan-kod).
