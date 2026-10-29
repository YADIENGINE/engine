# Editör kullanımı

## Pencere

Kayıtlı dock yerleşimi ve panel görünürlüğü korunur. İlk açılışta Develop düzeni Project, View ve Inspector'ı
gösterir; Output ve Profiler kapalıdır. Alt durum çubuğu bu araçları ve okunmamış hata sayısını erişilebilir tutar.

- **Sol (Project):** arama, kategori, All / Favorites / Recent. İçerik tek tıklamayla açılır; Ctrl+P aramaya odaklanır.
  Yıldız favoriyi değiştirir. Recent son başarılı 10 açılışı tutar.
- **Orta (View):** worker'ın gerçek görüntüsü. Panelin View menüsünde Fit tüm canvas'ı, Focus content içeriğin
  bildirdiği alanı, 100% mantıksal ölçüyü gösterir. Aynı menüdeki Zoom veya Ctrl+tekerlek yakınlaştırır,
  orta tuşla sürükleme kaydırır; Reset pan and zoom görünüm ayarlarını sıfırlar.
  Ctrl basılı değilken tekerlek ürüne gider. Büyüt düğmesi veya görünüm özetine çift tıklama geçici odak modunu açar.
  Kapalı, yüklenen veya hatalı önizlemelerde durum açıklaması ve uygulanabilen başlatma / yeniden deneme / iptal
  eylemi gösterilir. Playback yalnız global toolbar'dadır.
- **Sağ (Inspector):** gruplu kontroller, özellik araması ve Modified only filtresi. Vurgu noktası varsayılandan
  farklı değeri, Pending rozeti yanıt bekleyen değeri gösterir. Sıfırlama ikonu yalnız o kontrolü varsayılanına
  döndürür. Eylemler Actions altında. Live information grubu salt okunurdur.
  Choice kontrolleri aranabilir; seed gibi geniş tamsayılar doğrudan yazılabilir.
  Seçenek düğmeleri etiketin hemen altında panelin tam genişliğini kullanır. Normal kontroller dar panelde
  etiketin altına geçer; düğme ve alan adları harf harf sarılmak yerine kısalır, tam metin tooltip'te görünür.
- **Output:** zaman / kaynak / seviye; sayaçlı Info / Warning / Error filtreleri ve arama. Copy menüsü seçili ya da
  filtrelenmiş satırları kopyalar; Ctrl+tıklama çoklu seçim yapar. Follow ve Clear panel içindedir. Gizliyken gelen
  hatalar alt durum çubuğundaki Output sayacında görünür.
- **Profiler:** Live / Benchmarks / Compare. Benchmark örneği seçildiğinde açılır; warmup / ölçüm kareleri / tekrar
  çiftleri Advanced benchmark settings altındadır. Glow override tek bir seçim grubudur; vurgu son istenen değeri
  gösterir. Compare içindeki Export menüsü JSON ve Markdown sunar. Ayrıntılar: [Profiler](Profiler.md).
- Paneller dock edilebilir, sekmede birleştirilebilir, kapatılıp View menüsünden açılabilir. Preview → Language
  seçili önizlemenin dilini değiştirir; önizleme açılmadan da seçilebilir ve açılış tarifinde kullanılır. Dil kontrolü
  Inspector'da gösterilmez. Menü her zaman açılır; dil desteklemeyen önizlemelerde açıklama gösterir. View ayrıca
  arayüz ölçeğini (%75 / %100 / %125 / %150) ve önizleme performans ölçülerini yönetir. Varsayılan %100,
  önceki %125 boyutundadır;
  arayüz ve teknik metin fontları kullanıcı ölçeğiyle ve monitör DPI'ıyla birlikte ölçeklenir.

View → Layout altında Develop genel önizleme, Inspect daha geniş Inspector, Profile ise Profiler odaklı düzeni
açar. Elle oluşturulan son yerleşim Last custom layout ile geri çağrılabilir; hazır düzene geçmek onu silmez.
Alt çubuktaki Output / Profiler düğmeleri ilgili aracı açar veya kapatır; Ctrl+J son kullanılan alt aracı açıp kapatır.

## Toolbar

Global toolbar'da tek bir ana eylem vardır: durumuna göre Open, Stop veya Cancel olur. Pause / Resume ayrı bir
zaman kontrolüdür. Ana eylem ve Build & Refresh yeterli genişlikte etiketlidir; dar alanda ikincil kontroller
taşma menüsüne alınır. Tooltip'ler ad, kısayol ve kullanılamıyorsa nedenini gösterir.

| Düğme | Anlamı |
| --- | --- |
| Open / Stop / Cancel | Kapalıyken seçili içeriği açar; hazır veya pause'dayken worker'ı kapatır; katalog okuma, açılış, derleme veya benchmark sırasında o işlemi iptal eder. Kapanış sürerken devre dışıdır |
| Pause / Resume | Update'i ve sunum saatini dondurur; çizim, parametreler, eylemler ve Reset kullanılabilir |
| Step | Pause'dayken 1/60 s ilerletir |
| Hız | 0.1×..4× |
| Reset | Kabul edilen tariften yeni örnek kurar (saat 0'dan başlar) |
| Build & Refresh | Worker'ı kapatıp yalnız worker hedefini derler, seçimi ve değerleri geri getirir |

Reset önizlemeyi aynı worker sürecinde yeniden kurar. Worker sürecini yeniden başlatmak için Stop → Open kullanılır.

| Kısayol | Eylem |
| --- | --- |
| F5 | Duruma bağlı ana eylem |
| F6 | Pause / Resume |
| F7 | Pause sırasında tek kare |
| Ctrl+R | Reset |
| Ctrl+Shift+B | Build & Refresh |
| Ctrl+P | Project araması |
| Ctrl+Shift+P | Komut paleti |
| Ctrl+J | Son alt araç panelini aç / kapat |

Komut paletinde önizleme eylemleri, panel görünürlüğü ve yerleşimler aranır; yukarı / aşağı ile seçim,
Enter ile çalıştırma, Escape ile kapatma yapılır. Kullanılamayan eylemin nedeni gösterilir. Metin düzenlerken
ve popup açıkken playback / build kısayolları çalışmaz; editörün tükettiği girdi önizlemeye aktarılmaz.

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

Yerleşim, son özel düzen, görünürlük, ölçek, favoriler, son açılanlar ve Output tercihleri
`UserDataDirectory()/EngineEditor/preferences.json`
dosyasındadır (build dizini + proje adına göre ayrılır). Oyuncunun progress / settings dosyaları etkilenmez.
