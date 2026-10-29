# Editör kullanımı

## Pencere

Editör açılırken Project ve View panelleri görünür ve sekmelerinde öndedir; kayıtlı dock yerleşimi korunur.

- **Sol (Project):** arama, kategori, All / Favorites / Recent. İçerik tek tıklamayla ya da odaklanıp Enter ile açılır.
  Yıldız favoriyi değiştirir. Recent son başarılı 10 açılışı tutar.
- **Orta (View):** worker'ın gerçek görüntüsü. Fit tüm canvas'ı, Focus content içeriğin bildirdiği alanı,
  100% mantıksal ölçüyü gösterir. Zoom slider'ı veya Ctrl+tekerlek yakınlaştırır, orta tuşla sürükleme kaydırır.
  Ctrl basılı değilken tekerlek ürüne gider. Başlıktaki büyüt düğmesi / çift tıklama geçici odak modunu açar.
  Önizleme durduğunda veya ilk kare beklenirken görüntü alanı boş ve karanlık kalır.
- **Sağ (Inspector):** içeriğin kontrolleri gruplu; `*` varsayılandan farklı, `(pending)` yanıt bekleyen değeri
  gösterir. Default yalnız o kontrolü sıfırlar. Eylemler Actions altında. Live information grubu salt okunurdur.
  Choice kontrolleri aranabilir; seed gibi geniş tamsayılar doğrudan yazılabilir.
- **Output:** zaman / kaynak / seviye; Info / Warning / Error filtresi, arama, Clear, Copy. Gizliyken gelen hata
  Windows menüsünde rozet çıkarır.
- Paneller dock edilebilir, sekmede birleştirilebilir, kapatılıp Windows menüsünden açılabilir. Options → Language
  seçili önizlemenin dilini değiştirir; önizleme açılmadan da seçilebilir ve açılış tarifinde kullanılır. Dil kontrolü
  Inspector'da gösterilmez. Menü her zaman açılır; dil desteklemeyen önizlemelerde açıklama gösterir. Options ayrıca
  yazı boyutunu (%75 / %100 / %125 / %150), performans ölçülerini ve çıktı takibini yönetir.
  Varsayılan %100, önceki %125 boyutundadır.

## Toolbar

Open / Stop, Pause / Resume, Reset, Build & Refresh ve Cancel yalnız ikonla gösterilir. Her düğmenin adı,
üzerinde 1 saniye bekleyince tooltip olarak açılır; başka düğmeye geçilince süre yeniden başlar.

| Düğme | Anlamı |
| --- | --- |
| Open / Stop | Önizleme kapalıyken Open seçili içeriği açar; worker çalışırken aynı düğme Stop olur ve worker'ı kapatır. Derleme sırasında Stop derlemeyi iptal eder |
| Pause / Resume | Update'i ve sunum saatini dondurur; çizim, parametreler, eylemler ve Reset kullanılabilir |
| Step | Pause'dayken 1/60 s ilerletir |
| Hız | 0.1×..4× |
| Reset | Kabul edilen tariften yeni örnek kurar (saat 0'dan başlar) |
| Build & Refresh | Worker'ı kapatıp yalnız worker hedefini derler, seçimi ve değerleri geri getirir |

Reset önizlemeyi aynı worker sürecinde yeniden kurar. Worker sürecini yeniden başlatmak için Stop → Open kullanılır.

Replay, Reset ve ürünün kendi Pause ekranı farklıdır: Replay içeriğin kendi sunumunu baştan oynatır, Reset örneği
yeniden kurar, toolbar Pause zamanı durdurur.

## Kontrollerin uygulanma zamanı

- **Immediate:** mevcut görünüşe hemen uygulanır.
- **Next action:** bir sonraki Replay / Emit / Apply offer için hazırlanır.
- **Reset to apply:** yeni örnek kurulurken kullanılır.
- Randomize ve içerik etkileşimlerinin değiştirdiği kontroller sağ panele geri yansır. Rastgele sonuçlar tarife
  (seed) yazılır; Reset aynı sonucu kurar.

## Ürün girdisi

Önizlenen ürün UI'ının gezinme, hover, seçim ve kaydırma girdileri korunur. Görüntüye tıklamak klavye odağını verir; Inspector'daki metin kutuları ve popup'lar girdiyi tüketir.
Görüntüde başlayan sürüklemenin panel dışında bırakılması korunur. Odak kaybı basılı tuşları temizler.

## Gerçek oyun akışları

Katalog böyle bir giriş sunuyorsa worker üretim sahnelerini özel Game adaptörü üzerinden çalıştırabilir.
Başlatma seçenekleri ve bellek profili politikası proje tarafından tanımlanır.
Ürünün Back eylemi oyun gezinmesine aittir; toolbar Stop worker'ı kapatır.
Host OS pencere ve capture boyutunu sabit tutar.

## Tercihler

Yerleşim, favoriler, son açılanlar ve Output filtreleri `UserDataDirectory()/EngineEditor/preferences.json`
dosyasındadır (build dizini + proje adına göre ayrılır). Oyuncunun progress / settings dosyaları etkilenmez.
