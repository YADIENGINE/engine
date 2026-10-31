# Editör kullanımı

## Pencere

Kayıtlı dock yerleşimi ve panel görünürlüğü korunur. İlk açılışta Develop düzeni Project, View, Inspector ve
Tuning'i (Inspector'la aynı sağ alanda sekme) gösterir; Output ve Profiler kapalıdır. Alt durum çubuğu bu araçları ve okunmamış hata sayısını erişilebilir tutar.

- **Sol (Project):** arama, kategori, All / Favorites / Recent. İçerik tek tıklamayla açılır; Ctrl+P aramaya odaklanır.
  Yıldız favoriyi değiştirir. Recent son başarılı 10 açılışı tutar.
- **Orta (View):** worker'ın gerçek görüntüsü. Panelin View menüsünde Fit tüm canvas'ı, Focus content içeriğin
  bildirdiği alanı, 100% mantıksal ölçüyü gösterir. Aynı menüdeki Zoom veya Ctrl+tekerlek yakınlaştırır,
  orta tuşla sürükleme kaydırır; Reset pan and zoom görünüm ayarlarını sıfırlar.
  Ctrl basılı değilken tekerlek ürüne gider. Büyüt düğmesi veya görünüm özetine çift tıklama geçici odak modunu açar.
  Kapalı, yüklenen veya hatalı önizlemelerde durum açıklaması ve uygulanabilen başlatma / yeniden deneme / iptal
  eylemi gösterilir. Playback yalnız global toolbar'dadır.
- **Sağ (Inspector):** başlıktaki `?` açıklamayı tooltip veya tıklamayla açar. Özellik araması ve Modified sayacı
  değişen alanları bulur; en fazla altı görünür alanda arama Search düğmesiyle açılır. Vurgu noktası varsayılandan
  farklı değeri, Pending rozeti worker yanıtı bekleyen değeri gösterir; sonraki eylemde uygulanacak ayar anlamına gelmez.
  Sıfırlama ikonu değişen alanlarda, diğerlerinde hover / klavye odağında görünür ve yalnız o alanı varsayılana döndürür.
  Ana eylem panelin altında sabittir; ayarlar, ikincil Actions ve Live information kendi alanında kayar.
  Choice kontrolleri aranabilir; seed gibi geniş tamsayılar doğrudan yazılabilir.
  Seçenek düğmeleri etiketin hemen altında panelin tam genişliğini kullanır. Normal kontroller dar panelde
  etiketin altına geçer; checkbox sığdığında etiketle aynı satırda kalır. Normal alanlar ortak etiket sütununu kullanır;
  düğme ve alan adları harf harf sarılmak yerine kısalır, tam metin tooltip'te görünür.
  İleri ayar grupları ilk açılışta kapalıdır; arama ve Modified filtresi eşleşen grupları açar.
  Canlı bilgilerde sayılar sağa hizalı teknik fontla, kısa durumlar rozetle, açıklamalar satır sararak gösterilir.
- **Output:** `Last Build Try` son Build & Refresh tıklamasından preview hazır olana, hata veya iptale kadar
  olan çıktıyı; `Full` editör, worker ve bütün denemelerin son 1000 satırını gösterir. Son denemenin başlangıç
  saati, süresi, sonucu ve build exit code'u görünür. Info / Warning / Error sayaçları, arama ve Copy Visible
  seçili kapsama uygulanır. Tıklama tek satır, Ctrl+tıklama çoklu seçim, Shift+tıklama aralık seçimi yapar.
  Satır alanı odaktayken Ctrl+A görünür satırları seçer, Ctrl+C seçimi kopyalar; sağ tıkta da Copy Selected /
  Copy Visible vardır. Çift tıklama mesajı fareyle metin seçilebilen salt okunur ayrıntıda açar. Arama ve ayrıntı
  metin alanlarında normal Ctrl+A / Ctrl+C çalışır. Follow, yukarı kaydırınca veya satır seçilince durur.
  Clear yalnız Full tamponunu temizler; son deneme ve dosyalar korunur. `Open Log` seçili kapsamın tam dosyasını açar.
  Dosyalar build dizinindeki `editor-logs/` altında oturum ve deneme başına saklanır; otomatik silinmez.
  Son deneme tamponu 10000 satır / 8 MiB ile sınırlıdır; sınır aşılırsa ilk hata korunur ve tam dosyaya yönlendirme görünür.
  Build hatası veya başarılı build sonrası refresh hatası bir kez popup açar; iptal popup açmaz. Popup'taki
  salt okunur metin fareyle seçilip Ctrl+C ile kopyalanabilir; Ctrl+A tüm metni seçer. Copy düğmesi hata özeti,
  varsa build exit code'u ve ilk hata mesajını birlikte kopyalar. Show Output,
  büyütülmüş preview'dan çıkıp Output'u öne getirir, Last Build Try'ı açar ve ilk hataya kaydırır. Bu görünümde
  önceki arama/seviye tercihleri geçici olarak kaldırılır; kapsam değiştirilince kayıtlı tercihler geri gelir.
- **Tuning:** projenin `*.tuning.json` dosyaları ([Tuning](../Tuning.md)); liste build'in `Tuning/TuningFiles.json`'undan
  gelir, yeni dosya bir sonraki configure'dan sonra görünür. Üstte dosya seçimi, altında önizlemenin o dosya için son
  durumu: Live, Not applied (dosya geçersiz; satırı ve nedeni), Build needed (alan / uzunluk değişti; Build & Refresh
  düğmesi), Unreadable. Önizleme dosyayı kullanmıyorsa ya da açık değilse bu yazılır. Değer alanları etiket / kutu
  satırlarıdır, tablolar ızgaradır. Sayı, vec2, range ve renk (`#RRGGBB[AA]`) kutuya yazılır ve Enter'la girer;
  Enter'sız yazılan kutudan çıkınca geri alınır; onay kutusu tıklanınca yazılır. Sınır dışı ya da geçersiz değer
  gönderilmez, kutu kırmızı kalır, neden tooltip'tedir. Kabul edilen değer diskteki güncel dosyaya tek değer olarak
  yamanıp atomik yazılır (dışarıdan yapılan değişiklikler ezilmez); dosya o an geçersizse yazılmaz ve Output'a uyarı
  düşer. Dosya dışarıdan (elle ya da AI ile) değişince panel tazelenir. Satır / alan eklemek panelde değil dosyada
  yapılır.
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
| Open / Stop / Cancel | Kapalıyken seçili içeriği açar; hazır veya pause'dayken önizlemeyi kapatır; katalog okuma, açılış, önizleme yokken derleme veya benchmark sırasında o işlemi iptal eder. Önizleme açıkken süren derleme komut paletindeki Cancel build ile iptal edilir |
| Pause / Resume | Update'i ve sunum saatini dondurur; çizim, parametreler, eylemler ve Reset kullanılabilir |
| Step | Pause'dayken 1/60 s ilerletir |
| Hız | 0.1×..4× |
| Reset | Kabul edilen tariften yeni örnek kurar (saat 0'dan başlar) |
| Build & Refresh | Worker hedefini derler; önizleme derleme boyunca çalışmaya devam eder. Başarılı build seçimi, değerleri, pause'u ve hızı alıp hazır olunca eskisinin yerine geçer; hata varsa önizleme değişmez. Terminalden: `python Engine refresh` |

Reset önizlemeyi aynı worker sürecinde yeniden kurar. Worker sürecini yeniden başlatmak için Open kullanılır.

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
- Uygulama zamanı aynı olan grupta tek açıklama gösterilir; içerik eylem bağlantısını bildirmişse Start / Replay gibi
  gerçek düğme adı kullanılır. Karışık gruplarda `*` işaretli alanın tooltip'i uygulama zamanını açıklar.
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
