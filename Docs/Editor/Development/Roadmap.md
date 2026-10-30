# Editor development notes

Bu dosya desteklenen özellik listesinden ayrı tutulur.
Güncel API: [Api](../Api.md); üretim önizleme rehberi: [CreatingPreviews](../CreatingPreviews.md).

## Mevcut sınırlar

- UI ve worker süreçleri Windows masaüstüne özeldir.
- Görüntü aktarımı GPU readback ve iki paylaşımlı tampon kullanır; sıfır kopya değildir.
  En büyük canvas 1920×1080; meşgul tamponda eski kareler atlanabilir.
- Durum tutan View'lar için rastgele zamana scrub yoktur; Pause, Reset ve Step kullanılır.
- Kullanıcı recipe'lerini ayrı preset dosyasına kaydetme, reflection ve otomatik kaynak yazımı yoktur.
- Custom Game adaptörü kendi kayıt/telemetry izolasyonunu kurmalıdır.
- UI/worker protokol veya InputKey yerleşimi değişirse iki hedef birlikte derlenir.

## Yeni iş ekleme ölçütü

Genel ihtiyaç birden fazla projeye hizmet etmelidir. Oyun kataloğu, özel galeriler ve
olay koreografisi Engine'e eklenmez. Yeni public davranışta kullanım/API/mimari
belgeleri ve ilgili en küçük doğrulama bölümü güncellenir.
