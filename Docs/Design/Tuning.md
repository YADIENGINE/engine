# Tuning dosyaları (tasarım)

Durum: kabul edildi, uygulanmadı. Karar kaydı: [README → D2](README.md#d2-ayar-değerleri-tuning-dosyaları).

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

## Açık ayrıntılar

- Desteklenen tipler: `float`, `int`, `bool`, renk; vektör, aralık (`FloatRange`), enum seçimi?
- Tablo / dizi değerleri (ör. kademe başına olasılıklar) tuning'e girer mi, nasıl yazılır?
- Üretilen header'ın yolu ve include kuralı (kaynak köküne göre yol).
- Geçersiz JSON ya da aralık dışı değer: geliştirmede uyarı + son geçerli değer, Release derlemesinde hata.
- Editörün değeri yazarken dosya biçimini (sıra, girinti) koruması; ajan ile aynı anda yazma.
- Var olan `constexpr` sabitlerin ve oyun `Config` tablolarının tuning'e taşınma yolu.
