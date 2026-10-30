# Ajan API'si (tasarım, v1 sonrası)

Durum: v1'e girmiyor; engine'in stratejik önceliği. Karar kaydı:
[README → D5](README.md#d5-ajan-apisi-v1-sonrası-stratejik-öncelik).

## Sorun

Görsellerin tamamı kodla üretilir ve kodu çoğunlukla AI ajanı yazar. Ajan kodu derleyip testleri geçirebilir ama
ekranı göremez: değişikliğin doğru ve güzel görünüp görünmediğini bilmez. Bugün görsel kontrol geliştiricidedir;
"değiştir → geliştirici açıp bakar → geri bildirim → düzelt" döngüsü en büyük darboğazdır.

## Fikir

Exe komut satırından da çalışır ve ajanın kullanabileceği komutlar sunar. Ajan değişikliğin sonucunu kendisi
görür ve kaba hataları (bozuk, taşmış, görünmüyor, yanlış zamanlama) kendisi düzeltir. Geliştiricinin işi
"son hali beğendim mi" kararına iner.

```sh
yde render MyPreview --param glowRadius=18 --time 1.5 --out shot.png
```

| Komut | Ne yapar | Ajan ne kazanır |
| --- | --- | --- |
| `render` | Önizlemeyi pencere açmadan çizer, PNG'ye yazar | Görseli görür; önce / sonra karşılaştırır |
| `render --frames a..b --step s` | Kare dizisi (ya da tek görselde ızgara) | Animasyonu ve zamanlamayı görür |
| `inspect` | Önizlemenin o anki salt okunur durumu, JSON | Sayısal durumu okur |
| `run-scenario` | Tohumlu, kayıtlı input ile senaryo oynatır | Hatayı tekrar üretir |
| `build`, `test` | Yapılandırılmış hata / sonuç | Derleme hatasını ve test sonucunu ayrıştırır |

**MCP:** aynı işlevler MCP sunucusu olarak da sunulabilir; ajan komut yazmak yerine aracı doğrudan çağırır.
CLI'dan sonra gelir, aynı çekirdeği kullanır.

## Mevcut altyapı

Editör worker'ı önizlemeleri ayrı süreçte çizer, parametre ve inspection API'si vardır
([Editor architecture](../Editor/Architecture.md)). Eksik olan: pencere açmadan çizip PNG'ye yazan ve kapanan
bir worker modu, ve bunu dışarıya açan komut.

## Gereken özellikler

- **Determinizm:** sabit zaman adımı, tohumlu RNG, kayıtlı input; aynı komut her seferinde aynı görseli verir.
- **Sunum saati:** `render --time` saati doğrudan ayarlar (`DrawTime()` zaten worker'da kontrol edilebilir).
- **Makine okunur çıktı:** hatalar, log'lar, inspection ve test sonuçları JSON.
- **Sürüm uyumu:** komut launcher'dan çağrılır, iş projenin kendi editörüne / worker'ına devredilir (D3).

## Sınırlar

- AI'ın görsel yargısı geliştiricininki kadar iyi değildir: kaba hataları yakalar, estetik karar geliştiricide kalır.
- Görsel çıktı test doğrulaması (hash / golden image) olarak kullanılmaz; tasarım değişince kırılır. Amaç ajanın
  bakması, otomatik karşılaştırma değil.

## Açık ayrıntılar

- Çözünürlük ve ölçek (canvas 1600x900 mi, küçültülmüş mü; ajan bağlamı için görsel boyutu).
- Birden fazla önizlemeyi ya da parametre taramasını tek görselde ızgara olarak vermek.
- Oyunun gerçek akışından (sahne, menü) görüntü almak; sadece önizlemeler mi?
- Editör açıkken komutun çalışan worker'ı mı kullandığı, yoksa ayrı bir süreç mi açtığı.
