# Görsel asset'ler: katalog, galeri ve pişirme (tasarım)

Durum: **pişirme ertelendi**, uygulanmadı. Karar kaydı: [README → D10](README.md#d10-görsel-assetler-katalog-galeri-ve-pişirme).
Katalog ve galeri kısmının yerini [Asset library](AssetLibrary.md) (D22) aldı; aşağıdaki katalog ve galeri maddeleri
artık geçerli değildir.

## Erteleme notları (2026-11-01)

- Bu belge Painter'dan (D17) önce yazıldı. Artık tek batch çok sayıda şekil taşıyabiliyor; olası maliyetler CPU'da yol
  ve metin işleme, köşe yüklemesi ve GPU'da üst üste boyamadır. Hiçbiri henüz ölçülmedi: önce ölçüm.
- Saat okuyan çizimler kural 7 yüzünden pişirilemez, bu yüzden aday sayısı beklenenden az olabilir. En umut verici
  aday tam ekran arka planların durağan kısımlarıdır; bunlar atlas'a değil ayrı dokuya girer.
- Belgede ele alınmayan bedeller: büyütmede bulanıklık, mip olmadan küçültmede tırtık, bloom için ayrı emission eki.
- Alternatif: piksel yerine kaydedilmiş köşeleri önbelleğe almak. Keskinlik ve bloom korunur, atlas gerekmez, ama
  yalnız CPU maliyeti düşer. Uygulanabilirliği denetlenmedi.
- Gerekirse sıra: ölçüm → parça başına `Surface` + LRU bütçe → ancak doku yuvaları dolarsa atlas.

## Amaç

Görseller kodla üretilir (D1). Bunun iki sonucu var:
1. **Performans:** her karede yeniden çizilen karmaşık bir çizim (çok katmanlı portre, prosedürel doku, harita
   arka planı) çok kez çizilince pahalılaşır.
2. **Düzen:** Unity'deki asset klasörünün karşılığı yok; görseller kodun içine dağılmıştır, göz atılamaz.

Görsel asset, tekrar kullanılan bir görsel parçanın katalogdaki kaydıdır: saf bir çizim fonksiyonu, parametreleri
ve pişirme politikası. Katalog editörde galeri olur; `Baked` politikalı parça bir kez dokuya çizilip önbellekten
kopyalanır.

## Pişirme parametresi ve canlı parametre

Önbellek parametrelere göre çalışır: aynı fonksiyon aynı pişirme parametreleriyle ikinci kez istenirse hazır doku
kullanılır, farklı parametreyle ayrı bir doku pişirilir. Bir fonksiyondan sınırlı sayıda çeşit (onlarca) pişirmek
normaldir; 128x128 bir doku ~64 KB'tır. Tehlike sonsuz çeşittir: her karede değişen bir parametre her karede yeni
pişirme demektir. Bu yüzden parametreler ikiye ayrılır:

| | Pişirme parametresi | Canlı parametre |
| --- | --- | --- |
| Ne | Dokunun kendisini değiştirenler | Hazır dokunun nasıl kopyalandığını değiştirenler |
| Örnek | renk paleti, sayı, stil, şekil | konum, döndürme, ölçek, saydamlık, renk tonu |
| Değişirse | Yeni pişirme (önbellek anahtarı) | Bedava |

**Kural:** sürekli değişen değer canlı parametre olmak zorundadır. Canlı parametreyle ifade edilemeyen hareket
(tek tek sallanan parçalar) pişirilmez; görsel katmanlara bölünür: pişirilmiş durağan katman + canlı çizilen
hareketli katman.

## Kurallar

1. **Politika parça başınadır:** `Live` (her karede çizilir) ya da `Baked`. Varsayılan `Live`; pişirme faydalı
   olduğu yerde bilinçli açılır.
2. **Önbellek sadece bellekte durur, diske yazılmaz.** Kod değişince worker yeniden başlar (D9) ve önbellek
   kendiliğinden temizlenir; bayat doku hatası oluşamaz.
3. **Tuning değerleri (D2)** pişirme parametrelerinin parçasıdır; değer değişince anahtar değişir ve yeni doku pişer.
4. **Bellek bütçesi:** önbelleğin üst sınırı vardır; dolunca en uzun süredir kullanılmayan doku silinir. Hatalı
   bir parça sonsuz çeşit üretse bile oyun çökmez.
5. **İlk kullanımda pişir:** önceden toplu pişirme yok.
6. **Çözünürlük:** dokular pencere çözünürlüğünde pişirilir (canvas dokusuyla aynı ilke); pencere boyutu değişince
   önbellek temizlenir.
7. **Saf fonksiyon:** çizim fonksiyonu sadece parametrelerine bağlıdır; global durum, saat (`Painter::Time()`) ya da
   oyun durumu okumaz. Zaman gerekiyorsa canlı parametredir ya da parça `Live`'dır.

## Editör

- **Galeri:** katalogdaki her parça, parametreleriyle (tuning) birlikte; Icon Gallery'nin genelleştirilmiş hali.
- **Önbellek paneli:** doku sayısı, bellek, isabet oranı, parça başına yeniden pişirme sayısı.
- **Pişirmeyi kapat:** pişirilmiş ve canlı hali karşılaştırmak için genel anahtar.
- **Uyarı:** bir parça kareler boyunca sürekli yeniden pişiriliyorsa "bu parametre her karede değişiyor, canlıya al".

## Kod taslağı

```cpp
struct AppleLook { Color4 body; int leafCount; };   // pişirme parametreleri

void DrawAppleShape(const AppleLook& look);         // saf çizim

const VisualAsset<AppleLook> Apple{"Apple", DrawAppleShape, Bake::Cached};

Apple.Draw(look, {.pos = p, .scale = breath, .alpha = fade});   // + canlı parametreler
```

## Kapsam dışı (ileride)

- Animasyonu kare kare pişirmek (flipbook).
- Diske kalıcı önbellek.

## Açık ayrıntılar

- Pişirme parametresinin anahtarı nasıl üretilir (struct hash'i; float'ların yuvarlanması).
- Dokunun sınırları (bounds) ve parıltı gibi taşan efektlerin payı.
- Mevcut çizimlerin (ikonlar, portreler, harita arka planları) kataloğa göçü ve sırası.
- Önbelleğin Engine'deki yeri (`Graphics/` altındaki katman) ve `Surface` ile ilişkisi.
