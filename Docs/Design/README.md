# Engine tasarımı: exe-editör planı

Bu klasör engine'in geleceğine ait mimari planıdır, bugünkü kullanımı anlatmaz. Bugünkü API ve araçlar
`Docs/` altındaki diğer belgelerdedir. Kararlar tartışılarak alınır ve burada kayda geçer; uygulanan karar
ilgili kullanım belgesine taşınınca buradaki kayıt "uygulandı" olarak işaretlenir.

**Ad:** engine'in adı henüz kesin değildir. Ad ve sürümün tek kaynağı `EngineManifest.json`'dur (şu an
`YADIEN`, `0.1.0`). Belgelerde ad geçmez: düz metinde "engine", komut / exe / klasör adlarında `<engine>` /
`<Engine>` yer tutucusu (ör. `<engine> build`, `%LOCALAPPDATA%\<Engine>`).

## Hedef

Godot gibi tek exe olarak çalışan bir engine: exe açıldığında proje yöneticisidir (yeni proje kurma, var olanı
açma), proje açıldığında editördür. Geliştirme kod editöründe (VS Code) yapılır; engine editörü arkada açık
durur, derler, önizler, ayar yaptırır ve hata ayıklamaya yardım eder.

## Design goal'lar

1. **Görseller kodla üretilir.** Unity / Godot'taki gibi görsel asset import edilip oyun onlarla kurulmaz.
   Import edilen şeyler sadece fontlar, sesler (dış kaynaklı ses serbest) ve platform paketleme dosyalarıdır
   (store / launcher ikonu gibi). Bkz. [D1](#d1-asset-kuralı).
2. **Kullanımın %99'u koddur.** Editör sahne kurmaz, nesne yerleştirmez, kod yazmaz. Görevi kod değişince
   sonucu göstermek, parametrelerle oynatmak ve hata ayıklamaya yardım etmektir.
3. **AI ajanlarıyla geliştirmeye göre tasarlanır.** Engine kaynağı her projenin reposundadır; ajan engine'in
   neyi nasıl yaptığını okuyabilir. Her şey diff'lenebilir metindir.
4. **Tek exe: proje yöneticisi + editör.** `p.py` / `editor.py` sarmalayıcılarının işini (derleme, çalıştırma,
   test, editör) exe üstlenir.

## Godot / Unity'den farkı

| | Godot / Unity | Bu engine |
| --- | --- | --- |
| Görsel içerik | Import edilen asset'ler | Kodla üretilir; asset sadece font ve ses |
| Sahne / seviye | Editörde kurulur, serileştirilmiş dosyada saklanır | Kodda yazılır |
| Editörün rolü | İçeriği üreten araç | Kodun sonucunu gösteren, ayar yaptıran araç |
| Ayar değerleri | Sahne / prefab dosyasında | Repoda `*.tuning.json` metin dosyası ([Tuning](Tuning.md)) |
| Hedef geliştirici | Elle çalışan insan | İnsan + AI ajanı |

## Kararlar

### D1. Asset kuralı
**Karar:** Görsel içerik koddan üretilir. Import edilebilenler: font, ses (dış kaynak serbest), platform
paketleme dosyaları. Durum: kabul edildi.

### D2. Ayar değerleri: tuning dosyaları
**Karar:** Editörde ayarlanabilen değerler class'ın yanındaki `X.tuning.json` dosyasında durur. JSON tek
kaynaktır; struct derleme sırasında ondan üretilir. Geliştirmede değerler canlı okunur, Release'de koda gömülür.
Kural opt-in'dir. Görsel değerler ve oynanış dengesi değerleri aynı sistemi kullanır. Ayrıntı: [Tuning](Tuning.md).
Durum: kabul edildi.

### D3. Exe modeli: launcher + projeden derlenen editör
**Karar:** Unity Hub modeli. İki parça:
- **Launcher exe:** kullanıcının tıkladığı tek dosya; önceden derlenmiş, nadir değişir. Proje listesi, yeni proje
  kurma, açma, toolchain kontrolü; projeyi açınca editörü derleyip başlatır.
- **Proje editörü:** her projenin kendi `Engine/` kaynağından derlenir (bugün `editor.py`'nin yaptığı gibi).
  Worker ile aynı engine sürümünden geldiği için protokol / API uyumsuzluğu olmaz.

**Neden:** Her proje kendi engine sürümünü taşır ve C++ olarak derlenir. Önceden derlenmiş tek bir editör, farklı
sürümlü projelerle IPC / API olarak uyuşmaz; engine sık değiştiği için eski projeler sürekli kırılırdı. Farklı
engine sürümlü projeler yan yana yaşar. Bedeli: projenin ilk açılışında editörün bir kez derlenmesi.
Launcher ile proje arasındaki sözleşme (proje tarifi, derleme komutları) küçük ve kararlı tutulur.
Durum: kabul edildi.

### D4. Engine dağıtımı: kullanıcı modu ve developer modu
**Karar:** Mod launcher'a aittir; herkes developer moduna erişemez.

| | Kullanıcı modu (varsayılan) | Developer modu |
| --- | --- | --- |
| Kimin için | Engine ile oyun yapan herkes | Engine'i geliştirenler |
| Projedeki `Engine/` | Launcher'ın taşıdığı sürümün düz kopyası | Engine reposunun git submodule'ü |
| Engine kaynağının kaynağı | Launcher'ın taşıdığı tek sürüm; diğerleri GitHub tag'inden iner ([D16](#d16-sürümleme-ve-launcher-reposu)) | Engine reposu (erişim gerekir) |
| Engine'i düzenlemek | Yasak: salt okunur, AI için okunacak belge | Serbest; değişiklik engine reposuna push'lanır |
| Güncelleme | Launcher kopyayı yeni sürümle değiştirir | `submodule update --remote` |

- **Engine manifest'i:** `EngineManifest.json` (engine reposunda var: ad, sürüm, notlar). Kopyaya alınırken
  içe aktarma betiği engine commit'ini ve dosya özetlerini ekler (D16). Launcher bununla projenin sürümünü bilir,
  güncellemeyi temiz yapar ve salt okunur kuralını denetler (değişmiş dosya varsa açılışta / derlemede uyarır,
  güncellemede üzerine yazılacak dosyaları gösterir).
- Developer modundaki launcher submodule işlerini otomatikleştirir: yeni projede `git init` + submodule + ilk
  commit; "Engine'i güncelle"; "Engine değişikliklerini gönder" (engine'de commit + push, sonra projede pointer
  commit'i, detached HEAD düzeltmesi).

**Neden:** Engine'i kullanan basitlik ister (tek repo, git / GitHub şartı yok, private engine sorun olmaz);
geliştiren, oyunda çalışırken engine'i düzenleyip değişikliği iki yöne akıtmak ister. Salt okunur kural kopyaların
sessizce ayrışmasını ve AI'ın engine'i "düzeltmesini" önler. Durum: kabul edildi.

**Kopyanın kaydı (2026-10-31):** iki dosya.
- `Engine/EngineManifest.json` engine reposundakiyle aynıdır (elle yazılan kimlik ve notlar); içe aktarma ona
  dokunmaz.
- `Engine/EngineCopy.json` içe aktarma betiğinin ürettiği kayıttır:
  `{"format": 1, "source": {"tag": "v0.2.0", "commit": "<tam sha>"}, "files": {"<engine köküne göre yol>": "sha256:<hex>"}}`.
  `files` kopyadaki her dosyayı (manifest dahil, `EngineCopy.json` hariç) kapsar; yollar `/` ayraçlı, sıralı.
- Kopyaya giren her dosya tag'deki haliyle bayt bayt aynıdır; ayıklanan geliştirme dosyaları (D14) hiç girmez.
- **Projenin türünü dosyalar söyler:** `Engine/EngineCopy.json` varsa kullanıcı modu kopyası; yoksa ve `Engine/` bir
  git submodule'üyse developer projesi. Launcher bunun için ayrı bir proje ayarı tutmaz.

**Neden:** elle yazılan kimlik ile makinenin ürettiği uzun liste ayrı kalır; manifest kendi özetini taşıyamazdı,
ayrı kayıtta o da denetlenir; engine araçları manifest'i değişmeden okur.

**Developer modunun açılması (2026-10-31; D19 ile değişti):** engine public olunca şifre kalkar; developer modu
Ayarlar'da uyarılı düz bir anahtardır ("`main`'deki kararsız engine; engine'e katkı için. Göndermek engine reposuna
push yetkisi ister"). `DeveloperAccess`, şifre betiği, gömülü özet ve testleri silinir. Aşağıdaki şifre maddeleri
o güne kadar geçerlidir. **Neden:** `main` herkesin klonlayabildiği bir dal olur, göndermeyi zaten GitHub yetkisi
korur; public exe'ye gömülü özet çevrimdışı kırılabilir, bir şey korumaz. Dışarıdan katkı için "kendi fork'una gönder
+ PR aç" gerekirse sonra eklenir.

Eski karar: Ayarlar'da şifreyle.
- Launcher reposunda şifrenin kendisi değil, ondan türetilen özet durur (`DeveloperAccess.json`: PBKDF2-SHA256,
  salt, tur sayısı ≥ 600 000, özet); launcher derlenirken içine gömülür. Özeti launcher reposunun betiği üretir;
  şifre değiştirmek = betiği yeniden çalıştırmak. Launcher Windows'un kripto API'siyle (BCrypt) denetler.
- Doğru şifre modu açar ve `Settings.json`'a yazar; kapatmak şifre istemez.
- Şifre yanlışlıkla / meraktan açılmaya karşı bir kapıdır, güvenlik değildir: asıl koruma engine reposuna GitHub
  erişimidir (submodule işleri onsuz yapılamaz).
- Engine reposunun adresi `EngineManifest.json`'da `"repository"` alanıdır (yeni alan; engine'in kendi adresi).
  Developer modunda Sistem kontrolüne git maddesi eklenir (git kurulu mu, `user.name` / `user.email` ayarlı mı).

**Projenin türü ve launcher'ın modu (2026-10-31):** proje her zaman açılır; derleme, çalıştırma, editör ve CLI her
türde çalışır (iş projenin `Engine/Tools`'unda, D7). Türe göre değişen sadece engine'e dokunan işlemlerdir:

| Tür (dosyalardan) | Salt okunur denetimi | Engine güncellemesi |
| --- | --- | --- |
| Kopya (`Engine/EngineCopy.json`) | var | `Engines/`'dan, üzerine yazılacaklar listelenip onayla (her iki modda) |
| Submodule (`Engine/` git submodule'ü) | yok (engine git'te) | kullanıcı modunda yok, satırda "engine git ile yönetiliyor" notu; developer modunda submodule işleri |
| Kayıtsız (ikisi de değil) | yok; "engine'in kaynağı bilinmiyor" uyarısı | "`Engines/`'daki bir sürümle değiştir": üzerine yazılacak her şey listelenir, onayla yapılır |

Mod launcher'ın yetkisini belirler, projenin türünü değil. Kullanıcı modu hiçbir zaman git çalıştırmaz (D14) ve
submodule'ü kopyaya çevirmeyi önermez (kullanıcının reposunu değiştirirdi).

### D5. Ajan API'si: v1 sonrası, stratejik öncelik
**Karar:** AI ajanının değişikliğin sonucunu kendisi görmesini sağlayan komutlar (önizlemeyi PNG'ye render,
inspection, senaryo, yapılandırılmış build / test; sonra MCP) v1'e girmez. Engine'in Godot / Unity'den ayrıldığı
ana özelliklerden biri olarak v1'den sonraki ilk büyük iş alanıdır; v1 kararları bunu zorlaştırmamalı.
Ayrıntı: [Agent API](AgentApi.md). Durum: kabul edildi (ertelendi).

### D6. CLI: launcher exe aynı zamanda komut satırı aracıdır; editör açıksa derlemenin sahibi editördür
**Karar:** Launcher exe argümanla çağrılınca pencere açmadan komut çalıştırır (`<engine> build`, `<engine> test`,
`<engine> run`, `<engine> release` ...). Düğmeler ve komutlar aynı kodu çağırır; tek bir derleme mantığı vardır.
- Komut önce projenin editörü açık mı diye bakar. **Açıksa** komutu editöre iletir: editör worker'ı durdurur,
  derler, worker'ı yeniden başlatır, sonucu terminale geri gönderir. **Kapalıysa** komut işi kendisi yapar.
- Kaydedince otomatik derleyen dosya izleyicisi yoktur; derleme açık tetiktir (editör düğmesi ya da komut).

**Neden:** AI terminalde çalışır; düğmeye basamaz, derleme ve testi kendisi çalıştırabilmeli. Editör açıkken
ikinci bir derleme aynı `build/` klasöründe çakışır ve Windows çalışan worker exe'sinin üzerine yazdırmaz
(`LNK1104`). Derlemenin tek sahibi olunca AI'ın derlemesi editörde kendiliğinden yenilenir; AI art arda birçok
dosya değiştirip tek seferde derler. Durum: kabul edildi.

**Uygulama (2026-10-31):**
- Aşama 1 bulgusu: oyun (`build/`) ile editör (`build/<editör>`) ayrı klasörlerde derlenir; çakışma yalnız editörün
  worker build'indeydi ve snapshot'larla bitti. "Editöre ilet" işini engine'in `refresh` komutu yapar; launcher
  derleme mantığı yazmaz ve editörün açık olup olmadığına bakmaz, sadece devreder.
- **Launcher'ın kendi komutları `<engine> launcher <iş>` altındadır** (ör. `system-check`, `projects`, `version`);
  geri kalan her argüman projenin engine'ine aynen devredilir (`<engine> build --release` → `python Engine build
  --release`). Launcher engine komutlarını tanımaz; engine'e eklenen komut launcher güncellenmeden çalışır. Engine
  sözleşmesi `launcher` adında komut eklemez (ToolsContract).
- Proje bulma: `--project KÖK` verilmişse o; yoksa çalışma klasöründen yukarı doğru ilk `Project.json`'lu klasör.
- Launcher, projeyi çalıştırmadan önce `Engine/EngineManifest.json`'u doğrudan okur (`toolsContract`, sürüm); her
  satır için Python başlatmaz. Manifest'in alanları bu yüzden sözleşmenin parçasıdır (ToolsContract). Projenin
  `toolsContract`'ı launcher'ın bildiklerinden değilse komut çalışmaz, "launcher'ı güncelle" der (çıkış 2).

### D7. Python: launcher gömülü Python taşır, araçlar projenin engine'inden çalışır
**Karar:** Launcher resmi gömülebilir (embeddable) Python'u yanında taşır; kullanıcı Python kurmaz. Derleme,
test, release ve editör komutları launcher'ın içinde yeniden yazılmaz: launcher **projenin**
`Engine/Tools/` giriş noktalarını bu Python ile çalıştırır. Launcher ile proje arasındaki sözleşme:
`Project.json`'u oku, `Engine/Tools/`'taki giriş noktasını çağır.
- Engine araçları sadece standart kütüphane kullanır (bugün de böyle); bu kural korunur.
- AudioGeneration (numpy / scipy) varsayılan Python'a gelmez; isteyen geliştirici için launcher "ses araçlarını
  kur" seçeneği sunar. Oyunun derlenmesi / çalışması bunu gerektirmez.
- **Ses araçlarını kur (2026-10-31):** istendiğinde indirilir, pakete girmez. Ayarlar'daki düğme NumPy / SciPy
  wheel'lerini sürümü ve SHA-256'sı sabit bir kilit dosyasından (`Toolchain.lock.json` gibi; özet tutmazsa
  kurulmaz) indirir ve Velopack kökünde `current/`'in dışına açar (launcher güncellemesi silmez); pip yok, wheel bir
  zip'tir. İnternet ister (Sistem kontrolünün MSVC kurulumu gibi); paket herkes için ~60 MB büyümez.
  **Uygulandı (2026-10-31):** kilit launcher'ın `Toolchain.lock.json` `audio` bölümü (NumPy 2.5.4, SciPy 1.18.1;
  PyPI'nin özetleri), çıkarma Windows'un `tar.exe`'si, yer `<veri>/AudioTools/site/`. `._pth` değiştirilmedi:
  gömülü Python kullanıcının PATH'inde değildir, ses betiğini çalıştırmanın bir yolu gerekiyordu; bu yol
  `<engine> launcher python SCRIPT [ARGS]`'tır ve betiğin klasörünü + `site/`'ı `sys.path`'e kendisi koyar
  (`._pth` yaması geliştirme build'inde çalışmaz, klasörü kuruluma bağlardı). CLI'da `launcher audio-tools
  [status|install|remove]` da var (AI / betik için).

**Neden:** Derleme mantığı engine ile birlikte sürümlenir; her proje kendi engine sürümünün araçlarıyla derlenir
(D3 ile aynı ilke). Launcher ince ve kararlı kalır; mevcut araçlar ve testleri olduğu gibi kullanılır.
C++'a taşımak hem büyük bir yeniden yazım olurdu hem de derleme mantığını launcher'a bağlayıp sürüm
uyumsuzluğunu geri getirirdi. Durum: kabul edildi.

### D8. Derleyici: MSVC, launcher otomatik kurar
**Karar:** Derleyici MSVC (VS 2022 Build Tools) olarak kalır. MSVC'nin lisansı dağıtıma izin vermediği için
launcher onu taşımaz; açılışta sistem kontrolü yapar (`vswhere`), eksikse kurulumu kendisi otomatikleştirir
(gerekli bileşenlerle, ör. `winget`). CMake ve Ninja launcher ile gelir; kullanıcının ayrıca kurmasına gerek yoktur.

**Neden:** Bugünkü kod, uyarı ayarları (`/W4`), test altyapısı ve Visual Studio debugger aynen kalır; derleyici
göçü engine'i ürünleştirirken riski ikiye katlardı. Bedel ilk kurulumda bir kerelik büyük indirmedir.
**İleride:** kurulumsuz deneyim için launcher ile dağıtılan llvm-mingw (Clang) değerlendirilebilir; derleme
mantığı `Engine/Tools`'ta olduğu için (D7) bu kapı açık kalır. Şu an öncelik değil.

**Launcher'ın araçları (2026-10-31):** gömülü Python (resmi embeddable), CMake ve Ninja launcher reposuna ikili
olarak girmez. Repoda sadece bir kilit dosyası durur (araç başına sabit sürüm, indirme adresi, SHA-256); launcher
reposunun betiği bunları git'e girmeyen bir klasöre indirir ve özet tutmazsa durur. İnternet sadece launcher'ı derleyen
makinede bir kez gerekir; son kullanıcı hiçbir şey indirmez (kurulum exe'si araçları taşır). Yükseltme = kilit
dosyasında bir satır. Çalışırken launcher kendi `cmake` / `ninja` klasörlerini devrettiği alt sürecin `PATH`'inin
başına ekler; engine araçları onları zaten `PATH`'ten aradığı için engine değişmez. Sistem kontrolü araçları
kilit dosyasındaki sürümlere göre doğrular. **Neden:** ikililer git geçmişinde kalıcı olarak birikirdi (her
yükseltmede ~55 MB); özet, indirilenin tam o dosya olduğunu garanti eder. Durum: kabul edildi.

**Paketin içeriği ve boyutu (2026-10-31):** araçların ve engine kopyalarının hepsi pakette kalır (çevrimdışı derleme;
sonradan inen yalnız MSVC ve istenirse ses araçları, D7). CMake budanır: kilit dosyasında araç başına `exclude`
listesi (`doc/`, `man/`, `bin/cmake-gui.exe`, `bin/cpack.exe`); paket betiği onları almaz, engine yalnız `cmake` ve
`ctest` kullanır. Ölçüm (0.1, deflate): CMake 121 → 44 MB, Python 21 → 11, `Engine0.1` 26 → 5,4 (24 MB'ı
`ThirdParty/`), Ninja 0,6 → 0,3; budamayla paket ~60 MB'tan ~30 MB'a, kurulum ~170 MB'tan ~85 MB'a iner.
**Neden:** araçlar olmadan ilk derleme bile çalışmaz; sonradan indirmek D8'in sabitlenmiş, çevrimdışı ilkesini
bozar. Engine kopyası küçüktür ve eksiksiz kalmalıdır (D4).

**Bağımlılıklar engine içinde:** raylib, nlohmann_json, ImGui, rlImGui ve Catch2'nin kaynakları `FetchContent`
ile internetten inmez, engine reposunda durur (ör. `Engine/ThirdParty/`). Derleme internet istemez, D4'teki
engine kopyası kendi başına eksiksizdir, dış adresler kaybolsa da eski projeler derlenir. Bedeli engine
reposunun ~20–30 MB büyümesidir. Durum: **uygulandı** (`ThirdParty/`, 24 MB; lisans bildirimi ve denetimi de
oraya taşındı, ayrıntı `ThirdParty/README.md`).

### D9. Döngü hızı: hot reload yok, yanına derle sonra değiştir
**Karar:**
- **Hot reload (DLL değişimi) yok.** Kod değişince worker süreci yeniden başlatılır ve durum geri getirilir.
  Değer değişiklikleri zaten derleme istemez (D2).
- **Yanına derle, sonra değiştir:** derleme sırasında eski worker çalışmaya devam eder. Yeni worker farklı adlı
  bir dosyaya derlenir (çalışan exe'nin üzerine yazılmaz). Derleme başarılıysa editör eski worker'ı kapatır,
  yenisini açar ve durumu geri getirir: seçili önizleme, parametreler, sunum saati, pause, kamera zoom / pan.
  Derleme başarısızsa eski worker sürer, hata Output'ta görünür, önizleme kaybolmaz.

**Neden:** C++ / raylib'de DLL değişimi kırılgandır (bellek düzeni değişen struct'lar, raylib global durumu,
GL bağlamı); AI'ın yazdığı kod sık çöker ve süreç ayrımı editörü korur. Bugün Build & Refresh worker'ı derlemeden
önce kapattığı için derleme boyunca ve hatada önizleme kaybolur. D6 ile AI'ın derlemesi de önizlemeyi kesmez.

**Mühendislik işi (karar değil):** derleme süresini kısaltmak: önceden derlenmiş header'lar, unity build,
include temizliği. Durum: kabul edildi.

### D10. Görsel asset'ler: katalog, galeri ve pişirme
**Karar:** Tekrar kullanılan görsel parçalar saf çizim fonksiyonu olarak bir kataloğa kaydedilir; editör bundan
galeri üretir. Parça `Live` (varsayılan) ya da `Baked` olabilir; `Baked` parça pişirme parametrelerine göre bir kez
dokuya çizilir ve bellek içi, bütçeli bir önbellekten kopyalanır. Sürekli değişen değerler canlı parametredir
(konum, döndürme, ölçek, saydamlık, ton). Önbellek diske yazılmaz. Ayrıntı: [Visual assets](VisualAssets.md).

**Neden:** "Görseller kodla" diyen bir engine'de görsellere göz atabilmek asset klasörünün karşılığıdır; karmaşık
çizimlerin maliyeti pişirmeyle düşer. Ciddi bir engine için baştan doğru kurulur.
**Uygulama:** ayrı oturumda, grafik kodunun göçüyle birlikte (düzenleme + optimizasyon fırsatı).
Durum: katalog ve galeri kısmının yerini [D22](#d22-asset-library) aldı; pişirme kısmı ertelendi (ölçüm gerektirir).

### D11. Uzamsal değerler: v1'de sadece sayı
**Karar:** v1'de konum, dikdörtgen ve yol gibi uzamsal değerler tuning'de (D2) sayı olarak durur; editörde sayı
kutusu / slider ile ayarlanır. Editör nesne yaratmaz, yerleştirmez.

**İleride (v1 sonrası, beğenildi):** tuning'e `point`, `rect`, `path` tipleri ve editörün önizleme üstünde
sürüklenebilir tutamaçları. Sürüklemek değeri JSON'a yazar, derleme olmaz; yeni dosya türü / sahne / nesne yoktur.
Sınır: sayısı değişmeyen şeyler sürüklenir, sayısı değişen şeyler (yeni duvar, yeni düşman) koddadır; `path`
noktası ekleme / silme istisna olabilir. Örnek kullanım: ekran yerleşim bölgeleri, yol noktaları, efekt çapaları.
Durum: kabul edildi (sürükleme ertelendi).

### D12. Hata ayıklama: dış debugger'a kolay bağlanma + metin çökme raporu
**Karar:** Editör adım adım debugger yazmaz; durum inceleme (Inspection, Pause, Step, hız, Profiler) olduğu gibi
kalır. Üç ek parça v1'e girer, **v1'in en son işleri** olarak:
- **a) VS Code ayarları şablonda:** yeni proje `.vscode/tasks.json` (`<engine> build`) ve `launch.json` ("Oyunu
  debugger ile başlat", "Worker'a bağlan") ile kurulur; F5 elle ayar istemez.
- **b) "Debugger'ı bekle":** editörde anahtar; açıkken yeni worker başlar ve debugger bağlanana kadar bekler.
  Sebep: D9'da her derleme yeni worker (yeni süreç) açar ve ilk karedeki breakpoint kaçırılır.
- **c) Metin çökme raporu:** worker / oyun çökünce yığın izi (fonksiyon, dosya, satır) DbgHelp ile yakalanır;
  Output'a, bir dosyaya ve `<engine> build` / `<engine> test` çıktısına yazılır. AI çoğu çökmeyi debugger açmadan okur ve
  düzeltir.

**Neden:** Debugger'ı yeniden yazmak yerine var olan araçları bağlamak; çökmenin metin olarak görünmesi AI
odaklı iş akışının (goal 3) parçası. Durum: kabul edildi (v1 sonu).

### D13. Platform: sadece Windows masaüstü
**Karar:** Android desteği kaldırılır; launcher, editör ve oyun hedefi Windows masaüstüdür. Durum: kabul edildi.

### D14. Proje şablonu
**Karar:**
- **Boş proje:** "Yeni proje" örnek kod kurmaz; `Game/` boşken bugünkü starter oyun / worker devreye girer.
  Örnek kod belgelerdedir (bugünkü kural sürer).
- **AI dosyası yok:** kullanıcı modunda şablon `CLAUDE.md` / `AGENTS.md` ya da başka bir AI ayarı üretmez.
  Herkes kendi AI'ından sorumludur; engine AI'a okunabilir kaynak ve belge sunar, ayar dosyası yazmaz.
- **Git yok:** kullanıcı modunda launcher git'e hiç dokunmaz (init, commit, kutucuk yok); bu kullanıcının işidir.
  Developer modunda git D4'teki gibi.
- `p.py` / `editor.py` şablondan kalkar; yerlerine `<engine>` komutu (D6 / D7). `.vscode/` D12 ile (v1 sonu) gelir.

**Geliştirme dosyalarının ayıklanması (2026-10-31):** neyin kopyaya girmeyeceğini engine reposu bilir: engine'in
`.gitattributes`'unda `export-ignore` satırları (`CLAUDE.md`: engine geliştirme kuralları, kopyada AI'ı engine'i
düzenlemeye yöneltir; `Docs/Design/`: planlar, AI bugünkü özellik sanabilir). İçe aktarma kopyayı `git archive <tag>`
ile alır: `.git` ve `export-ignore` dosyaları gelmez, çalışma ağacındaki değişiklik sızamaz, içerik tag'le kesin
aynıdır (temiz ağaç denetimi gereksizleşir; tag'in var olması ve origin'e push'lanmış olması denetlenir). Yeni
ayıklanacak dosya = engine'de bir satır. Engine testleri (`Core/Tests`, `Tools/Tests` ...) kopyada kalır.

**Developer modunda AI dosyası (2026-10-31):** yok; şablon iki modda aynıdır. Developer projesinde `Engine/` tam
submodule olduğu için engine'in kendi `CLAUDE.md`'si oradadır; projenin AI dosyasını geliştirici yazar.
Durum: kabul edildi.

The engine provides `AgentDocs/` as readable material; projects opt in by referencing `Engine/AgentDocs/Index.md`.

### D15. Launcher
**Karar (yapı):** Launcher engine'den ayrı bir üründür ve en üst katmandır; engine sürümlerini içinde taşır:

```
Launcher/
  Engines/
    Engine1.0/ ...
    Engine1.1/ ...
```

- Launcher derlenirken desteklenen engine sürümleri (genelde 2–3) içine paketlenir. `Engines/` sadece kopyalama
  içindir: yeni proje kurmak ve kullanıcı modunda engine güncellemek (D4).
- Oyun launcher'dan çalışmaz; her proje kendi `Engine/` kopyasından derlenir ve çalışır. Launcher güncellense ya da
  eski bir sürümü artık taşımasa bile eski projeler kendi engine'leriyle açılır (D3 / D7).
- Launcher ile proje arasındaki sözleşme (`Project.json` biçimi + `Engine/Tools` giriş noktaları) geriye uyumlu
  kalır; değişirse `Project.json` `"version"` artar ve launcher eski sürümleri de okur.

**Karar (ekranlar ve akış):** ilk ekran sade.

| Ekran | İş |
| --- | --- |
| Projeler | Son açılanlar; Aç, Yeni, Var olanı ekle (`Project.json`'lu klasör), Listeden kaldır (diske dokunmaz). Satırda engine sürümü, mod, "Güncelleme var" rozeti |
| Yeni proje | Ad, konum, engine sürümü (`Engines/`); developer modunda engine dalı / commit'i |
| Sistem kontrolü | MSVC denetimi ve tek tuşla kurulum (D8); launcher'la gelen araçların doğrulanması; ilk açılışta otomatik |
| Ayarlar | Mod (kullanıcı / developer), PATH'e ekle, arayüz ölçeği |

- **Aç:** manifest denetimi (D4) → editör derlenmemişse ya da engine değiştiyse derle (ilerleme çubuğu) → editörü
  başlat, launcher kapanır. Editör menüsünde "Proje listesine dön".
- Engine güncellemesi her zaman kullanıcı onayıyla yapılır.
- Launcher editörle aynı ImGui arayüz katmanını ve temayı kullanır; yeni UI sistemi yazılmaz.
- **Developer modu kapısı:** Ayarlar'da şifre (ayrıntı D4); asıl koruma engine reposuna GitHub erişimidir,
  submodule işleri onsuz yapılamaz.

**Karar (dağıtım ve kurulum):**
- **Tek exe dağıtılır** (`<Engine>-Setup-<launcher sürümü>.exe`, ~30 MB; ölçüm D8 "Paketin içeriği"): launcher + gömülü paket (Python, CMake, Ninja,
  `Engines/`).
- **İlk çalıştırma kurulumdur:** paketi `%LOCALAPPDATA%\<Engine>\`'e çıkarır (yönetici izni istemez), masaüstüne
  kısayol koyar (exe kopyası değil: tek exe, güncellemede ayrışmaz), isteğe bağlı `<engine>`'yi PATH'e ekler (varsayılan
  açık), Windows "Uygulamalar" listesine kaydolur (kaldırma oradan), sonra launcher'ı ve Sistem kontrolünü açar.
- **Kurulum klasörü:** `<engine>.exe`, `Tools/` (python, cmake, ninja), `Engines/`, `Settings.json` (proje listesi, mod,
  tercihler); Velopack düzeninde ikisi ayrılır (aşağıda "Karar (kurulum programı)"). Ayarlar ekranında "Kurulum klasörünü aç".
- **Projeler** varsayılan `Belgeler\<Engine> Projects\` altına; yeni proje ekranında değiştirilebilir.
- **Güncelleme:** yeni sürümün exe'si var olan kurulumun üzerine kurar; proje listesi ve ayarlar korunur, `Engines/`
  yeni sürümün desteklediklerle değişir. Desteği düşen sürümü kullanan projeler etkilenmez (kendi kopyaları var);
  sadece o sürümle yeni proje kurulamaz. Launcher'ın kendi güncellemesi: aşağıda "Karar (uygulama içi güncelleme)".
- İmza: aşağıda "Karar (imzalama)" (açık kaynak olunca SignPath Foundation; o güne kadar imzasız).

**Karar (kurulum programı, 2026-10-31):** tek exe bir **Velopack** `Setup.exe`'sidir (MIT; indirilen exe
kurulumdur, bitince launcher'ı açar). Sihirbaz yok: çift tıklanınca kurulurken bizim görselimiz (png / animasyonlu
gif) görünür, sonra launcher açılır; ilk gerçek ekran Sistem kontrolüdür. Kullanıcı başına
`%LOCALAPPDATA%\<id>\` (yönetici izni yok), masaüstü + Başlat menüsü kısayolu, Uygulamalar kaydı ve kaldırma
Velopack'tandır. Paket betiği launcher build'ini, `ToolCache/`'i (`Tools/` olarak) ve `Engines/`'i `vpk pack`'e
verir.
- **Launcher'a girenler:** Velopack'in hazır C kütüphanesi (`velopack_libc`, launcher reposunda sürümü sabit) ve
  `<engine>.exe` / `<engine>.com` başında `VelopackApp::Build().Run()`. Kurulum / kaldırma hook'ları orada çalışır;
  PATH ekleme / silme launcher'ın kendi `UserPath`'iyle hook'tadır (tek uygulama).
- **Düzen (2026-11-01'de düzeltildi):** Velopack uygulamayı `<id>\current\`'e koyar; güncellemede `current/`'i,
  yeni bir Setup üzerine çalıştırılınca **bütün kökü** değiştirir (eski kökü yana alır, temiz kurar, kurulum hook'unu
  çalıştırır, eskiyi siler; kaldırma hook'u çalışmaz). Bu yüzden veriler (`Settings.json`, `AudioTools/`) kökün
  yanındaki `%LOCALAPPDATA%\<id> Data\`'dadır (`LauncherLayout::velopackInstall`). `Engines/` ve `Tools/`
  `current/`'in içindedir (sürümle gelir). İlk karar (veriler kökte) denemede yanlış çıktı: üzerine kurulum audio
  tools'u sildi.
- **Kaldırma (2026-10-31):** iz bırakmaz: Velopack kökü ve kısayollar gider, kaldırma hook'u PATH satırını ve veri
  klasörünü siler (yalnız gerçek kaldırmada çalışır, üzerine kurulumda değil). Projelere hiç dokunulmaz; yeniden kurulumda liste Projects Folder taramasıyla geri gelir, ilk
  açılış (Sistem kontrolü) yeniden çalışır. MSVC (VS Build Tools) kalır: sistem geneli kurulumdur, başka uygulamalar
  da kullanabilir; kaldırmak isteyen Visual Studio Installer'dan kaldırır (belgede yazılı). Velopack'in kaldırmada
  kökü bütünüyle sildiği deneme paketinde doğrulanır. **Neden:** temiz kaldırma; kaybolan liste tek taramayla
  kurulur. `%APPDATA%` (Roaming) iz bırakır ve etki alanı hesaplarında var olmayan proje yollarını taşır.
- **Sürüm (2026-10-31):** paketin sürümü launcher'ınkidir (`LAUNCHER_VERSION`, kendi semver'i; Velopack'in
  güncelleme karşılaştırması, exe adı `<Engine>-Setup-<sürüm>.exe`, Uygulamalar listesi). Launcher düzeltmesi, araç
  yükseltmesi ya da taşınan engine'in değişmesi onu artırır; yeni engine sürümü tek başına artırmaz (D16). Taşınan engine sürümleri Engines sekmesinde ve release
  metninde ("içerir: engine 0.1.1, 0.2.0") görünür; launcher'ın kendi değişiklikleri release metnindedir, engine
  notları manifest'te kalır (D16). **Neden:** engine sürümüne bağlansa yalnız launcher'ı değiştiren yayının numarası
  olmazdı ve iki seri taşınırken numara yanıltırdı.
- **Paketleyen makine:** `vpk` bir .NET aracıdır (.NET SDK gerekir); sürümü launcher'ın kütüphanesiyle aynı tutulur.
- **Deneme paketi (yapıldı 2026-10-31, vpk 1.2.161, Debug build):** Velopack engel çıkarmadı; geri dönüş (Inno
  Setup 6.6) gerekmedi. Görülenler:
  - Düzen: `%LOCALAPPDATA%\<id>\` altında `current\` (uygulama), `Update.exe`, `packages\` ve kısayolların
    hedefi olan küçük bir `<id>.exe` başlatıcısı. Uygulamalar kaydı `Update.exe --uninstall` ile kaldırır.
  - Boyut (Debug exe'lerle): Setup 47 MB, tam paket 40 MB; aynı içerikli sonraki sürümün delta paketi 2,3 MB.
  - Kurulumdan sonra exe `--veloapp-install`, kaldırmada `--veloapp-uninstall` ile çalıştırılır (30 / 60 sn
    sınır); `VelopackApp::Build().Run()` yoksa launcher normal açılır ve öldürülür. Entegrasyon şarttır.
  - Kaldırma temizdir (klasör, kısayollar, kayıt gider); yalnız Velopack'in `%LOCALAPPDATA%\velopack\`
    günlükleri kalır (hook'tan sonra da yazıldığı için silinmez; kabul).
- **Çalışan süreçler (Q4, 2026-10-31):** yeni sürümün Setup'ı kurulum klasöründen çalışan her süreci (launcher,
  `<engine>` komutları, onların python / cmake / ninja'sı) sormadan kapatır; aynı ya da eski sürümü de üzerine
  kurar. Editör projenin `build/`'inden çalıştığı için kapanmaz, yalnız o anki Build & Refresh'i bozulur. Elle
  kurulumda bu kabul edilir: kesilen derleme kaldığı yerden sürer (Ninja), ayarlar her değişiklikte diske yazılır,
  engine güncellemesi yeni kopyayı yanına kurup sonda yer değiştirir. İndirme sayfası ve belgeler "kurmadan önce
  launcher'ı kapat, derlemelerin bitmesini bekle" der. Otomatik güncelleme (v1 sonrası) launcher'dan yapılır ve
  yalnız hiçbir `<engine>` komutu ya da iş çalışmazken, launcher kapanırken uygulanır; `<engine>.com` çalışırken
  adlı bir mutex tutar. **Neden:** Velopack'in kapatması hook'larımızdan önce olur; elle kurulumu da korumak kendi
  kurulum sarmalayıcımızı, yani reddedilen seçeneği geri getirirdi.
- **Neden:** en modern kurulum deneyimi; v1 sonrasına bırakılan otomatik güncelleme ve delta güncelleme (yalnız
  değişen dosyalar iner, GitHub Releases'tan) aynı temelin üstünde neredeyse hazır gelir. Kendini kuran launcher
  seçeneğinde çıkarma, kendini silen kaldırıcı, kilitli dosya ve antivirüs sezgileri (~1–1,5k satır) bize kalırdı.
  Bedel: hazır bir ikili kütüphane, paketleyen makinede .NET SDK, Velopack'in genç olması (2024).

**Karar (uygulama içi güncelleme, 2026-11-02):** launcher kendini Velopack ile günceller; kod launcher `Core/Update/`
ve `Apps/Ui/Update/`'te, kullanım launcher `Docs/CoreApi.md` ve `Docs/Packaging.md`'de.
- **Yer:** yalnız launcher penceresi. Editör launcher'ı bilmez (engine → launcher bağımlılığı olmaz) ve proje açılınca
  launcher kapandığından bildirim her oturumun başında zaten görülür.
- **Akış:** açılışta arka planda denetim; internet yoksa sessiz. Yeni sürüm üst çubukta düğme olur ve paneli her
  açılışta bir kez, başka panel ya da iş yokken kendiliğinden açılır (2026-11-05; önce açılmıyordu, yalnız düğme gözden
  kaçtı); panelde notlar, boyut ve Update / Later / Skip this version. Later o açılış için kapatır, Skip o sürüm için.
  İndirme bitince Restart now / Later. Ayarlarda açılış denetimi kapatılabilir.
- **Çalışan süreçler (Q4'ün uygulaması):** `<engine>.com` ve launcher'ın ön plan işleri, kimsenin sahip olmadığı adlı bir
  mutex açık tutar: birden çok komut aynı anda tutabilir, süreç öldürülse de kalmaz. Restart bunlar bitene kadar bekler.
- **Kaynak:** Velopack'in GitHub kaynağı değil, `DownloadFile` kullanan özel kaynak (`custom_callback`). **Neden:**
  Velopack'in indirmesi iptal edilemez ve `Job` kapanışta thread'i bekler; pencere indirme sürerken takılırdı. Sistem
  proxy'si de böyle kullanılır. SHA-256 denetimi Velopack'te kalır. **API değil indirme adresleri (2026-11-05):** feed
  `releases/latest/download/releases.win.json`'dan, her paket kendi sürümünün `releases/download/v<sürüm>/`'ünden iner.
  Release API'si adres başına saatte 60 istek verir; ortak ağda (yurt, kampüs) biten limit denetimi her seferinde
  bozuyordu ve Velopack asıl hatayı "null pointer" mesajıyla gizliyordu. Son release'in feed'i önceki sürümlerin
  paketlerini de listeler (yayın betiği önceki paketin üstüne paketler); launcher release'leri pre-release değildir, bu
  yüzden "latest" her zaman en yeni launcher'dır. Bir kaynak hatası kullanıcıya kendi metniyle gösterilir.
- **Sınama:** otomatik geri alma yoktur, bozuk sürüm ancak elle Setup ile düzelir. Bu yüzden `dev.py update-test` gerçek
  kurulumu yerel dosyalardan güncelleyip sınar ve `dev.py release` her yayında çalıştırır.
- **Güven:** paketler imzasız, SHA değerleri paketle aynı yerden gelir; güvenlik GitHub organizasyonuna bağlıdır, 2FA
  zorunlu olmalıdır.

**Karar (imzalama, 2026-10-31):** "Windows bilgisayarınızı korudu" (SmartScreen) kurulum aracına değil imzaya
bağlıdır. İmzasız exe'de itibar her sürümde sıfırdan başlar; Windows 11 Smart App Control imzasız dosyayı hiç
çalıştırmayabilir. EV sertifikası 2024'ten beri SmartScreen'i aşmaz; imzalı yeni dosya da ilk indirmelerde
uyarı alır ama yayıncı adı görünür ve itibar aynı sertifikanın sonraki sürümlerine taşınır.
- **Engine ve launcher açık kaynak olacak** ve paket **SignPath Foundation** ile ücretsiz imzalanır (CI kurulunca;
  CI/CD D19'da ertelendi). Şartları: OSI
  lisansı, public repo, pakette kapalı kaynak bileşen yok, imzalayan ekip repo sahibi, MFA, imzalanan ikilinin public
  repodan CI'da derlendiğinin doğrulanması. Sertifikada yayıncı "SignPath Foundation"dır.
- O güne kadar imzasız (yalnız tanıdık test kullanıcıları; "Yine de çalıştır"). Paket betiğinde imza adımının yeri
  baştan vardır.
- Microsoft Store (MSIX) hiç uyarı göstermeyen tek ücretsiz yoldur ama şimdilik uymaz (salt okunur kurulum klasörü,
  PATH yazımının sanal kayıt defterine gitmesi, her sürümde mağaza incelemesi); ileride ikinci kanal olabilir.
  Azure Artifact Signing bireylere yalnız ABD / Kanada'da açık.

**Karar (dil ve giriş noktaları, 2026-10-31):**
- Launcher C++20'dir. Bütün iş (ayarlar, proje listesi, proje işlemleri, modlar, manifest denetimi, sistem
  kontrolü, CLI'ın devri) **headless bir çekirdek kütüphanededir** ve testleri vardır; arayüz onu çağırır.
- Aynı adlı iki ince giriş: `<engine>.exe` pencere programıdır (çift tıklama, kısayol: proje listesi),
  `<engine>.com` konsol programıdır (terminalde `<engine> build` yazınca Windows `.com`'u önce seçer; pencere açmaz,
  çıktı ve çıkış kodu terminale gelir). Visual Studio'nun `devenv.exe` / `devenv.com` modeli. `.com` ImGui'ye
  link'lenmez. Dağıtım yine tek kurulum exe'sidir.
- Python launcher'da sadece iki yerde: projenin `Engine/Tools`'u (D7) ve launcher reposunun geliştirici betikleri
  (içe aktarma); son kullanıcının çalıştırdığı launcher kodu C++'tır.

**Neden:** Arayüz editörün C++ / ImGui katmanıdır (yukarıda); çekirdek de C++ olunca dil köprüsü olmaz. Tek bir
exe hem konsol hem pencere programı olamaz: konsol exe'si çift tıklamada siyah pencere açar, pencere exe'si
terminalde çıktı vermez ve beklenmez (AI / CI çıkış kodunu okuyamaz).

**UI hazırlığı (2026-10-31):** launcher ekranlarından önce verilen kararlar.
- **Paylaşılan araç arayüzü katmanı:** `Engine/ToolUi/`, CMake hedefi `EngineToolUi`; include'lar engine köküne göre
  (`#include "ToolUi/ToolTheme.h"`). İçinde ImGui + rlImGui kaynakları, tema (renkler, ölçüler, `ToolTone`), fontlar
  (IBM Plex, Font Awesome; `ToolFont`; [D23](#d23-tool-look-cabinet)), ImGui bağlamı + DPI / ölçek (`ToolUiContext`) ve genel widget'lar
  (`ToolWidgets`: rozet, bölüm başlığı, boş durum, tooltip, ikon düğmesi, segment). Editöre özgü olanlar
  (`EditorPhase` renkleri, parametre / Inspector widget'ları, playback, docking) `Editor/`'de kalır; docking bağlam
  kurulurken seçenektir. Editör ve launcher aynı katmanı kullanır, kopya yoktur.
  **Neden:** `Core/` oyuna giren koddur, oyunlar ImGui'ye link'lenmez; `Editor/` altında olsa launcher editörün
  önizleme API'sini de çekerdi. `Ui` adı oyun arayüzüyle (`Core/Graphics/Ui`) karışırdı.
- **Proje listesine dön:** launcher engine komutu çalıştırırken (Aç ya da `<engine> editor`) çocuk sürece
  `ENGINE_LAUNCHER=<launcher'ın pencere exe'sinin tam yolu>` koyar; değişken `python Engine editor` üzerinden editöre
  kendiliğinden geçer (ToolsContract'ta yazılı, araçlar değişmez). Editör değişken varken File menüsünde
  "Back to project list" gösterir: o exe'yi başlatır, sonra normal kapanışla kapanır (Godot gibi). Değişken yoksa
  (`python Engine editor`) madde görünmez.
  **Neden:** editör launcher'ın yerini ve adını bilmez; argüman engine sözleşmesine seçenek ekletir ve araçların
  aktarmasını gerektirirdi. Editörün kapanması açık editörün `Engine/` klasörünü kilitleyip engine güncellemesini
  bozmasını ve iki projenin derleme sahipliğinin karışmasını önler.
- **Arka plan işi:** çekirdekte tek tip `Job<T>` (`Core/Jobs/`): kendi thread'i, durum (`Running` / `Succeeded` /
  `Failed` / `Canceled`), adım metni + adım sayısı ("Building editor", 2/3), birikmiş çıktı satırları (UI "şu
  satırdan sonrakiler"i okur; son ~5000 satır tutulur), `Failed`'da tek cümlelik hata, `Cancel()` (bayrak; çalışan
  süreç ağacı sonlanır), bitince `Result<T>`. Yıkılırken sürüyorsa iptal edip thread'i bekler. Yüzde yoktur;
  ilerleme adım + "donmadı" göstergesi (dönen çubuk; görsel UI işi) + canlı çıktıdır. Uygulama aynı anda en çok bir
  ön plan işi gösterir (model kuralı; tip birden fazla örneğe izin verir).
  **Neden:** derleme süresi önceden bilinmez; Ninja'nın `[12/340]` çıktısını ayrıştırmak launcher'ı engine'in çıktı
  biçimine bağlardı.
- **Dil:** launcher şimdilik yalnız İngilizce; yerelleştirme ve dil seçimi yok. Metinler ekran fonksiyonlarında
  yazılır; gerekirse sonra bir tabloya taşınır (çekirdek etkilenmez).
  **Neden:** çekirdeğin hata mesajları ve editör İngilizce; karışık dil olmaz.
- **Düzen ve gezinme (Godot proje yöneticisi benzeri):** bölgeler ve eylemler burada, ölçü / renk / yerleşim UI'ın.
  Üst gezinme (ad, sekmeler, sistem durumu, Settings), araç çubuğu, proje listesi, ayrı sağ yardımcı panel. Alt durum
  çubuğu yoktur; görünüm kuralları [D23](#d23-tool-look-cabinet)'te, ortak tema `ToolUi/`'dadır. Hepsi tek uygulama
  modelinde.
  - **Sekme** (`LauncherTab`, tam ekran, her an bir tane): **Projects**: araç çubuğu (filtre: ad ya da yol, sıralama:
    Last opened / Name / Path, `Settings.json`'da saklanır; Add existing, New project), tek seçimli liste
    (çift tıklama = Open), sağ proje denetçisi. Hiç proje yokken bunların yerine kendi ilk açılış ekranı görünür
    (New project, Add existing, derleme araçları ve engine durumu, proje klasörü). **Engines**: bu bilgisayardaki ve indirilebilen sürümler (sürüm, tarih,
    notlar; kırıcılar öne çıkar, kullanan proje sayısı); developer modunda "dev (main)" satırı; salt okunur.
  - **Sağ proje denetçisi**: yalnız seçili projenin bilgileri ve eylemleri; görünür bir proje varken model hep birini
    seçili tutar (son açılan önce), denetçi boş kalmaz. Dar pencerede Project details açılır panelinden erişilir.
    Launcher sekmesi, arayüz ölçeği ayarı ve tarama satırı yoktur. Varsayılan proje klasörü, PATH, kurulum klasörü,
    developer modu ve lisanslar Settings'tedir.
    **Project eylemleri** (uygun olmayan pasif): Open; Run (`<engine> run` işi: derleme, sonra oyun kapanana kadar
    "Running" + Stop); Show in folder; kopya projede Update engine / Repair engine, kayıtsız projede Replace engine
    (onay paneli → iş); developer modu + git projesinde Engine git (Update from remote, Publish changes); Remove /
    Remove missing (listeden; diske dokunmaz). Bilerek yok: Rename (proje adı engine'in kuralına bağlı), Duplicate
    (`build/` ve kayıt kimlikleri), Tags (sonra eklenebilir).
  - **Panel** (`LauncherPanel`, sekmenin üstünde en çok bir tane): New project, Settings, System check (ilk açılışta,
    `Settings.json` yokken otomatik; üst gezinmedeki nokta durumu gösterir: hazır / uyarı / derlenemez), Engine
    update onayı (notlar, kırıcı olabilir, üzerine yazılacak / silinecek dosyalar; onay işi başlatır), Job progress
    (iş sürerken yalnız Cancel ile kapanmaz; bitince başarıda işin hedefine gider: Aç → launcher kapanır, yeni proje →
    Projects'te seçili; hatada hata + çıktı + Back).
  - **Onay popup'ı** (`model.pending`): "Remove from list?", developer şifresi gibi küçük onaylar; onayda modelin
    eylemi çağrılır.
  - Bozuk `Settings.json`: Projects'in üstünde hata + "Start with defaults"; ayrı ekran yok.
- **Developer projesinin başlangıcı:** "dev (main)" her zaman dalın en son commit'inden başlar; dal / tag / commit
  seçimi yok (plan dışı, gerekirse sonra).
- **Hata ve uyarılar:** seviyeler Error / Warning / Info (`ToolTone`). Uzun iş hatası iş ekranında kalır (başlık,
  çekirdeğin mesajı, kopyalanabilir tam çıktı, Back). Anlık eylem hatası modelde tek bildirimdir (`model.notice`:
  seviye + başlık + mesaj; kapatılana ya da yenisi gelene kadar). Projenin kalıcı durumu kendi satırında, çözümü
  varsa düğmesiyle (Repair, Replace engine, Remove from list). Başlığı UI yazar ("Could not add project"), altına
  çekirdeğin mesajı aynen gelir; UI mesajı yeniden yazmaz. `MessageBox` yalnız pencere açılamazsa.

### D16. Sürümleme ve launcher reposu
**Karar:**
- **Numara `ANA.ARA.YAMA`:** YAMA hata düzeltmesi, ARA geriye uyumlu ekleme, ANA kırıcı değişiklik. Launcher'daki
  klasörler `Engine<ANA.ARA>` adını taşır ve o serinin son yamasını içerir.
- **Ayrı repolar:** engine ve launcher ayrı repolarda, ayrı geliştirilir. Bir engine sürümü stabil sayıldığında
  launcher reposuna kopyalanır (`Engines/Engine<ANA.ARA>/`); launcher reposunda 2–3 sürüm durur. Kopyalar orada
  düzenlenmez; aktif engine launcher reposunda submodule'dür (aşağıda). Exe'ler istendiğinde release olarak yayınlanır.
- **Güncelleme notları engine manifest'indedir** (sürümün yanında); launcher güncellemeden önce gösterir. Ayrıntılı
  tasarım sonra.
- Developer modundaki projeler sürüm takip etmez, engine reposunun dalını takip eder.

**Launcher reposu (2026-10-31):** GitHub'da `YADIENGINE/yadien` (D19). Repo adı ürünün görünen adı değildir:
launcher kodu ve belgeleri adı yine sabit yazmaz, gösterdiği ad içe aktarılan engine manifest'inden gelir.

**Aktif engine launcher reposunda submodule (2026-10-31; 2026-11-02'de kalktı, launcher'ın checkout'unu artık `dev.py`
seçer, launcher `Docs/Development.md`; `Engines/` de aşağıdaki "Sürümler isteğe bağlı iner" ile repodan çıktı):**
```
<launcher reposu>/
  Engine/        submodule: engine reposu, main (aktif geliştirme; iki adımlı akışla düzenlenir)
  Engines/
    Engine0.1/   stable kopyalar (tag'lerden git archive; hiç düzenlenmez)
```
- Launcher kendini `Engine/` submodule'üne karşı derler (en yeni stable kopyaya değil): UI'ın engine'den
  istediği değişiklik tag / içe aktarma beklemeden yapılır; launcher sürümü yine pointer'ın commit'ine bağlıdır.
- İçe aktarma betiği tag'i submodule'ün git'inden okur (yol argümanı yok).
- Submodule sadece repodadır; kurulan paket yalnız stable `Engines/` taşır (private / stable olmayan kaynak
  kullanıcıya gitmez). Kurulmuş launcher'ın developer modunda sürüm listesinde "dev (main)" seçeneği de vardır:
  proje engine'i manifest'in `repository` adresinden submodule olarak alır (D4 developer projesi). Kullanıcı
  modunda bu seçenek yoktur: aktif engine stable olmadan (tag + içe aktarma) kullanıcıya görünmez.

**Kırıcı değişiklik (2026-10-31):** bir önceki sürümle derlenen ve çalışan bir proje yeni sürümde elle değişiklik
istiyorsa kırıcıdır. Örnekler: public header / tip / fonksiyonun silinmesi, adının ya da anlamının değişmesi
(`Internals/` hariç, orası serbest); CMake fonksiyonlarının (`engine_add_game`, `engine_add_tuning` ...) imzası;
`Project.json` şema `version`'ı ya da `toolsContract`'ın artması; kayıt / ayar dosyası biçiminin eskiyi okuyamaması;
editör önizleme API'si (`Editor/Api`). ARA: geriye uyumlu ekleme; YAMA: API'ye dokunmayan düzeltme.

**Güncelleme notlarının biçimi (2026-11-02):** her sürüme bir dosya, `ReleaseNotes/<sürüm>.json`; yeni sürüm yalnızca
kendi dosyasını ekler (önceden manifest'in `"notes"` alanındaydı; eski tag'ler öyle kalır, launcher ikisini de okur):
```json
{ "date": "2026-11-20",
  "changes": ["Tuning: color type"],
  "breaking": [ { "change": "DrawUiChip takes a style", "migrate": "Pass CurrentTheme().ui.chip first" } ] }
```
Launcher güncellemede projenin sürümünden yeni sürüme kadar aradaki bütün notları gösterir; `breaking` satırlarının
`migrate` açıklamasını öne çıkarır (AI da okuyup uygular). Engine testi tutarlılığı denetler: `version` notların ilk
satırıyla aynı, sürümler azalan sırada ve tekrarsız, tarih `YYYY-MM-DD`; böylece notsuz sürüm artırılamaz.

**İlk yayın (2026-10-31):** 0.x ile başlanır. 0.x'te **ARA kırıcıdır** (0.2 → 0.3 kırabilir), YAMA geriye uyumlu
ekleme ya da düzeltmedir; launcher klasörleri `Engine0.1`, `Engine0.2` ... ve 0.x'te ARA geçişini "kırıcı olabilir"
diye gösterir. **1.0.0** v1 planı (Aşama 3–6) bitip bir oyun launcher üzerinden yayınlanınca verilir; ondan sonra
semver tamdır (kırıcı = ANA). Kırıcılık kuralı ve not testi aynıdır, sadece baktığı hane 0.x'te ARA'dır.

**Öneriler (uygulamada):**
- **İçe aktarma betiği:** launcher reposunda tek bir betik engine reposundaki bir tag'i `git archive` ile kopyalar
  (tag origin'de mi denetimi; geliştirme dosyaları engine'in `export-ignore` satırlarıyla ayıklanır, D14) ve
  `EngineCopy.json`'u yazar (kaynak tag / commit, dosya özetleri; D4). Kopyanın kaynağı her zaman izlenebilir.
- **Yama akışı:** eski bir seride hata düzeltmesi engine reposunda o sürümün tag'inden açılan bir dalda yapılır,
  yeni yama tag'lenir ve tekrar içe aktarılır.
- **Atlanan sürümlerin notları:** 1.1'den 1.3'e geçen kullanıcı 1.2'nin notlarını da görmeli; manifest notları
  sürüm geçmişini birikimli taşır.
- **Launcher'ın kendi derlemesi:** launcher UI'ı editörün ImGui katmanını kullandığı için (D15) kendini `Engine/`
  submodule'üne karşı derler (yukarıda).
- **Repo boyutu:** git aynı içerikli dosyaları bir kez saklar; sürümler arasında değişmeyen `ThirdParty/` ve
  dosyalar yer kaplamaz, sadece değişen dosyalar büyütür.

Durum: kabul edildi.

**Sürümler isteğe bağlı iner, geçiş önce denenir (2026-11-02):** yukarıdaki "launcher 2–3 sürüm taşır" ve D4'teki
"internet gerekmez" kararının yerine geçer.
- **Launcher tek sürüm taşır** (`LauncherManifest.json` `engine`), böylece internetsiz kullanıcı ilk projesini açabilir.
  `Engines/` repodan çıktı; `dev.py` o sürümü tag'den `build/Engines/`'e alır, paket oradan kopyalar. Diğer sürümler veri
  klasöründeki önbelleğe iner (launcher güncellemesi silmez). Klasör sürüm başınadır (`Engine0.3.1`), seri başına değil.
- **Kaynak:** tag'in kaynak arşivi + o tag'in GitHub Release'indeki `EngineCopy.json`. Launcher her dosyayı bu kayda göre
  denetler; eksik, fazla ya da farklı dosya reddedilir. Engine yayını artık launcher yayını gerektirmez; launcher'ın
  bilmediği `toolsContract` "önce launcher'ı güncelle" der. **Neden:** boyut değil (engine kurulumda yaklaşık 6 MB);
  engine yayınını launcher'dan ayırmak ve her eski sürüme erişmek.
- **Geçiş projenin geçici kopyasında denenir** (temiz oyun derlemesi, editör derlemesi, testler), proje karar verilene kadar
  değişmez. Geçerse engine değişir ve build klasörleri silinir; kalırsa hatalar ve `migrate` notları gösterilir, seçenekler
  "kal" ve "yine de geç". **Reddedilen:** engine'i proje içinde değiştirip derlemek, hata varsa geri koymak. **Neden:**
  gerçek denemede (0.2.0 → 0.3.0) mevcut build ağacında derleme hiçbir şey derlemeden başarılı döndü, geri alınan engine
  bozulmuş build ağacında derlenemedi, oyun ve testler geçerken editör kaldı. Kopyada geri alınacak bir şey yoktur; bedeli
  ek disk ve geçişten sonra bir derleme daha. Ayrıntı: launcher `Docs/Engines.md`.
- **"Yine de geç" şarttır:** kırıcı değişiklik tanım gereği derlemeyi bozar; yoksa hiçbir proje kırıcı sürümü geçemez.
- **Geri dönüş seçilebilir**, uyarıyla: oyunun yeni engine ile yazdığı kayıtları eski engine okuyamayabilir, bunu derleme
  yakalayamaz.
- **Otomatik kod düzeltme yok** (ileride `python Engine migrate` olabilir). Rapor AI'a verilecek metin olarak kopyalanır.

Durum: kabul edildi, uygulandı.

**Engine sürümü launcher sürümünü artırmaz (2026-11-05):** yeni engine yalnız engine olarak yayınlanır; kurulu
launcher'lar onu Yeni proje listesinde en üstte, varsayılan olarak "(download)" ile gösterir (liste panel açılınca da
okunur). `LauncherManifest.json` `engine` yalnız launcher yayınında değişir: launcher'ın derlendiği ve kurulumla gelen
(internetsiz ilk proje) sürümdür. Launcher'ın sürebildiği contract'lar `LauncherManifest.json` `toolsContracts`'tadır
(kod da oradan okur); `dev.py release`, son yayımlanmış launcher'ın sürmediği `toolsContract`'lı engine'i tek başına
yayınlamaz, ikisi birlikte çıkar. Launcher yalnız kendi değişikliği, contract, launcher'a yansıması istenen `ToolUi`
değişikliği ya da taşınan engine'in değişmesi için yayınlanır. **Neden:** launcher numarası her engine'de artınca
değişmeyen launcher'ın güncellemesi kullanıcıya boşuna iner ve numara bir şey söylemez.

### D17. Render sistemi: kendi GPU katmanı üstünde yeni 2D çizim sistemi
**Karar:** Çizim raylib'in anlık API'sinden alınır. Engine'e ait, raylib'siz bir `Painter` API'si çağrıları kare
boyunca kaydeder (DrawList) ve Engine'e özel `GpuDevice` arayüzü üzerinden doğrudan OpenGL 3.3 core'a gönderir.
Premultiplied alpha (alpha ve additive aynı batch'te), SDF şekiller, dinamik glif atlası, emission'dan bloom; durum
RAII kapsamlarla. Raylib pencere, input ve ses için kalır. Eski raylib çizimi geçiş köprüsüyle aynı karede çalışır;
oyunun View'ları tek tek taşınır, sonra köprü ve eski yol silinir. Ayrıntı: [Render sistemi](Rendering.md).

**Neden:** Engine çizim durumunu, batch'lemeyi, kenar yumuşatmayı ve shader'ı sahiplenmeden ne kalite ne performans
tek yerden iyileşir. rlgl üstüne kurmak sabit köşe biçimi ve kontrolsüz yüklemeyle kalıcı borç bırakırdı; kendi
katmanımız köşe biçimini, yüklemeyi ve ölçümü bize verir, başka arka uç yeni bir `GpuDevice` uygulamasıdır.
Durum: uygulandı (2026-10-31), çalışma zamanı doğrulaması bekliyor; kullanım [Painter](../Graphics/Painter.md).

### D18. Console paneli: editörde gerçek terminal
**Karar:** Editörün alt dock'unda `Output`'un yanında ayrı bir `Console` paneli. İçinde ConPTY ile düz bir kabuk
çalışır, libvterm ekranı çözer, ImGui çizer. Çekirdek ortak `Terminal/` hedefindedir (launcher da alabilir).
Editör AI'a özel arayüz ya da komut yazmaz; AI CLI'ı kendi TUI'siyle olduğu gibi çalışır. İlk sürüm tek oturumdur
(MVP + seçim / kaydırma geçmişi); sekmeler, link algılama ve MCP köprüsü sonra. Ayrıntı: [Console](Console.md).

**Neden:** AI ile prototipleme editörden çıkmadan yapılabilmeli. Kendi sohbet arayüzümüz CLI'ın komutlarını, izin
akışını ve her yeni özelliğini yeniden yazmayı gerektirir, hep geride kalır; gerçek terminal hiçbir şeyi yeniden
yazmaz ve bakımı küçüktür (ConPTY işletim sisteminin, libvterm kararlı). D6 ile konsoldaki derleme editöre gider.
Durum: aşama 1 uygulandı (2026-10-31), çalışma zamanı doğrulaması bekliyor.

### D19. Açık kaynak ve yayın altyapısı
**Karar (2026-10-31):** engine ve launcher **public** olur; geliştirme döngüleri ayrı olduğu için ayrı repolar kalır.
- **Ev:** GitHub organizasyonu **`YADIENGINE`** (kuruldu 2026-10-31; `yadien` adı başka bir kullanıcının). İçinde
  `YADIENGINE/yadien` (launcher; ön kapı, organizasyonda sabitlenir, README'sinde indirme) ve
  `YADIENGINE/engine` (engine kaynağı; README'si "kullanmak için launcher'ı kur" der). Godot'nun
  `godotengine/godot` düzeni: organizasyon adı ne olduğunu, ana repo ürün adını söyler. Repolar taşınarak
  (transfer) gelir; eski adresler yönlenir. Manifest `repository`'si, submodule adresleri ve belgeler güncellenir.
- **Lisans:** iki repo da **MIT**. Engine kodu oyunlara derlendiği için kapalı kaynak ticari oyunlara izin veren
  bir lisans gerekir; MIT'in tek şartı (bildirim) `release`'in yazdığı `ThirdPartyNotices.txt`'e engine'in kendi
  satırı eklenerek kendiliğinden karşılanır. Paket, gömdüğü araçların lisans metinlerini de taşır (Python PSF,
  CMake BSD-3, Ninja Apache-2.0, Velopack MIT). GPL ticari oyunu kapattığı için, zlib daha az bilindiği, MIT +
  Apache-2.0 tek kişilik proje için ağır olduğu için seçilmedi. Telif satırı
  `Copyright (c) 2026 YADIEN contributors` (Godot gibi kişi adı yok; ürün adı burada hukuki metin olduğu için
  yazılıdır, ad değişirse `LICENSE` de değişir).
- **Belge dili:** engine ve launcher belgeleri bundan sonra **İngilizce**. Engine `Docs/`'u her projenin
  `Engine/`'ine girdiği için kullanıcının okuduğu dil odur. İlk public yayından önce kullanıcıya dönük olanlar
  çevrilir (engine `Docs/` — `Design/` hariç —, iki README, launcher belgeleri, yeni CONTRIBUTING); `Docs/Design/` ve
  `CLAUDE.md`'ler aynı kuralla sonra. İki repodaki "belgeler Türkçe" kuralı İngilizce olur. Oyun repoları kendi
  dillerini seçer.
- **Developer modu:** şifresiz, uyarılı düz anahtar (D4).
- **Açılmadan önce temizlik:** yeni commit'ler GitHub'ın gizli adresiyle (`<id>+<kullanıcı>@users.noreply.github.com`,
  repo git ayarında; GitHub'da "Keep my email addresses private" + "Block command line pushes that expose my email").
  Geçmiş yeniden yazılmaz: hash'ler değişse oyunların eski submodule pointer'ları var olmayan commit'leri gösterirdi.
  Engine belgelerindeki / testlerindeki oyun adları genelleşir (`MyGame`). Repolarda şifre / anahtar yok (kayıt
  anahtarı kodda saklanmaz).
- Oyun repoları (ör. bir oyunun kendisi) private kalabilir; public engine / launcher'ı submodule olarak alır.
- **Yayın (şimdilik elle):** paket yerelde `dev.py package` (D15) ile üretilir ve launcher reposunun Releases'ına
  elle yüklenir (Setup.exe + Velopack güncelleme dosyaları); indirme sayfası `releases/latest`. Yayınlar imzasızdır.
- **CI/CD ertelendi (2026-10-31):** GitHub Actions (her push'ta derleme + test; `v*` tag'inde paket → SignPath
  imzası → Release) şimdi kurulmaz, v1 dışıdır. SignPath imzası CI'da derlenmiş ikili istediği için o da onunla
  birlikte bekler. Paket betiği CI'ın sonra aynen çağıracağı tek komut olarak yazılır.
- **Neden:** organizasyon iki repoyu tek ürün gibi gösterir, büyüyebilir (belge, örnek, site) ve ortak eklemek
  kolaydır; kişisel hesapta kalmak taşımayı sonraya ertelerdi.

Durum: kabul edildi (2026-10-31); uygulama Aşama 4'te.

### D20. Editor automation through the control endpoint
**Decision (2026-11-01):** the open editor is driven and observed through its existing control endpoint (named pipe,
JSON lines), not a separate headless process. Workflow: the developer runs an AI CLI in the editor's Console; the agent
lists, opens and adjusts editor scenes (Game Flows are scenes), seeks, takes screenshots, reads inspection and logs, and
brings the screen to the developer. Supersedes the separate-process plan of [AgentApi](AgentApi.md) for an open editor.
- **Generic automation, not an AI feature:** commands name editor operations; no AI name or AI-specific behaviour (D14,
  D18). Any script or test can use them.
- **CLI only:** `python Engine ctl <command>` (JSON on stdout). No MCP server: the CLI works with every agent and needs
  no AI configuration.
- **Visible changes:** every state-changing command shows a short "Control: ..." notice in the editor and is logged.
  Changes apply immediately; the developer's input is never blocked.
- **Fresh frames:** a screenshot or inspection first syncs with the worker (its reply means every earlier command is
  applied) and then waits for frames drawn after it; an agent never judges a stale frame.
- **Reproducible time:** `seek` pauses and advances the worker's playback clock in fixed steps (rewinding through a
  reset), without frame limit or readback until it arrives.
- **Discovery:** the console's shell gets `ENGINE_EDITOR_CONTROL` (the pipe), so commands reach that editor even when
  several are open; otherwise the build directory's announcement file.
- **Captures:** PNG under `<build>/editor-captures/`, newest 50 kept; optional scale and canvas-space crop.
- **Phase B (2026-11-01):** `ctl frames` (labelled grid of evenly spaced times), viewport Mark (toolbar toggle or
  Alt+drag; pauses, saves the region, pastes a context line into the Console without Enter), and an `Errors (N):`
  summary after failed builds (Python, `Tools/build_errors.py`).
- **Later:** headless rendering without an open editor, several clients at once. Scripted input scenarios: D26.

Durum: kabul edildi (2026-11-01); uygulandı.

### D21. Agent knowledge and the editor's agent workspace
**Decision (2026-11-01):** the engine actively supports working with an AI agent, in two layers. Revises D14 (no AI
names or files at all) and D18 (shell only): the template still ships no AI file by default, but the engine documents
an opt-in and the editor knows agent CLIs by name.
- **Knowledge, by kind:** `AgentDocs/Agent.md` is a short page loaded every session (tool rules, hard invariants, a
  table of skills); `AgentDocs/Skills/<name>/SKILL.md` are procedures read when a request matches (editor,
  visual-change, build-fix, api), in the Agent Skills format; recipes, API pages and `Docs/` stay reference material.
  `AgentDocs/Index.md` is the API routing table the `api` skill points to. Projects opt in by importing `Agent.md`
  from their own agent file. `check` validates skills and every `python Engine ...` call written in AgentDocs.
- **Tools for agents:** `ctl find` / `describe` turn a subject into a scene, choices and one ready `ctl open` command
  (choices are searched, so per-project content maps are unnecessary); `ctl open --set --action --screenshot` is one
  call; `status` says the editor is open and what to do next; `ctl note` reports progress to the status bar.
- **Agent workspace:** the Console holds several sessions, one of them the agent session running the developer's CLI
  (Claude Code, Codex or a custom command; a user preference). Agent menu / palette: open the agent, send the scene
  (with a capture), the build failure or Output lines as `[editor] ...` context, ask for a build fix. Bells mark a
  session that waits; the status bar shows the agent's state; an Agent layout puts the Console beside the Viewport.
  "Agent control" lets the developer make the endpoint read-only; "Visual checks" (2026-11) refuses captures, so
  looking stays with the developer and agents spend no tokens on images. The editor never calls a model or stores keys:
  the CLI keeps its own sessions, permissions and updates (D18's reason still holds).
- **Deferred: Claude Code plugin.** The engine folder could be a plugin marketplace whose plugin is loaded in place
  from the project's `Engine/` (skills from `AgentDocs/Skills/`, hooks such as `ctl note`, version = the submodule
  commit), enabled from a project's `.claude/settings.json` (`extraKnownMarketplaces` with a `directory` source +
  `enabledPlugins`). Open before adopting: how a relative `path` resolves, what a teammate sees on first open (beyond
  the workspace trust dialog), and Windows behaviour; verify with a clean clone.
- **Deferred: MCP server.** Same core as `ctl`, tools always in the agent's context and images returned inline; costs
  a constant context share and a second interface, and dynamic catalogs cannot be static enums. Decide with
  measurements (an agent eval set: prompts + expected final `ctl status`), not by default.

Durum: kabul edildi (2026-11-01); knowledge layer, agent workspace backend and workspace UI implemented
(grouped menu, session tabs, state badges, setup and Inspector context).

### D22. Asset library
**Decision:** Ready-made graphics and audio are browsed in a Unity-style Assets panel, separate from the scene catalog
but sharing its viewport. Folders: read-only `Engine/Assets/Graphics` and free-form `Game/Assets/Graphics`, next to the
runtime `Audio` and `Fonts`. Every graphic is a set (a single drawing is a set of one). Registration is compiled into
the editor worker only, so Release has no catalog. Use is optional. Prefabs are deferred. This takes over D10's catalog
half; baking stays in D10, deferred. Details: [Asset library](AssetLibrary.md).

**Why:** the motivation is organization and reuse, not performance. A code-first engine still needs a place to look.
Status: accepted (2026-11-01), implemented; prefabs and baking deferred.

### D23. Tool look: Cabinet
**Decision:** The launcher and editor share one look, in `ToolUi/`. Warm graphite surfaces (Cabinet palette) replace the
earlier plum. Regions are separated by tone, not lines: panes and controls have no border, only what floats above them
does. One corner radius (3 px) and one control height (30 px). At most one amber button on a screen. IBM Plex Sans and
Mono are embedded and replace Segoe UI and Consolas. The identity comes from the logo's pixel block: `ToolPixel` draws
the product name, project tiles, square state marks and the activity indicator from the launcher icon's 5x7 font.
The native title bar is colored to match. The launcher loses its status bar, which only repeated the top bar. With no
projects it shows its own first-run screen, and the list always keeps a visible project selected so the details panel
is never empty. The editor's scene list is named Scenes, and the viewport sits on a neutral well.

**Rejected on purpose:** gradients, glass and blur, glow, shadowed card grids, pill buttons, large radii, accent rails on
cards, emoji, and greetings. Motion only shows that work is running. Nothing game-specific enters this layer.

The editor adds one hue of its own, a periwinkle only the agent uses (the toolbar's agent strip, a working Console
session), so an agent at work never reads as the amber primary button or as a result's green or red. Amber on the strip
means the agent is waiting for the developer.

Editor buttons and status text carry meaning through color, on the unchanged surfaces. Green fills the button that starts
the scene and marks a live state. A rose tint marks Stop, never a solid fill, so stopping does not read as an error. Teal
is work in progress and the Build icon. Amber stays with selection, pause, warnings and the Inspector's primary action.
Only a button that starts something is filled; other colors stay on icons, text and quiet tints. These colors live in
`Editor/Ui/Style/EditorTheme`, so the launcher keeps its amber primary button.

**Why:** the previous look framed everything equally, kept ImGui's stock parts (triangle headers, large square
checkboxes, block combo arrows, permanent tab controls), and tinted every surface, which also changes how a game's
colors read in the viewport. Removing frames costs little and helps most; the pixel block is an identity the product
already owned. An installed engine copy keeps its look until a project moves to a newer engine.
Status: implemented (2026-11-02).

### D24. Prebuilt engine
**Decision:** Each engine release publishes the engine compiled: Release static libraries and the editor executable, about
100 MB (15 MB compressed). The launcher downloads it with the engine and also carries the one for the engine it ships, so
an engine release needs no launcher release and the first project is fast offline. A project links them and compiles only its own code. The
sources stay in the project's `Engine/`, because the compiler reads the headers and developers and AI agents read the
rest. There are no Debug libraries: a project's Debug configuration compiles its own code unoptimized against the Release
libraries, with the same runtime. `"engine": { "build": "source" }` keeps the full Debug build, and a project falls back to
it by itself when the package cannot be right: a checkout, an edited copy, an older compiler, its own runtime, tests.
Details: [Prebuilt engine](../Prebuilt.md).

**Why:** A new project spent nearly all of its first build (43 s on a 16-thread machine, likely minutes on a laptop) compiling
code identical in every project; the project's own part took about 2 s. Static libraries keep the game a single
executable, which DLLs would not; MSVC links across versions when the linker is at least as new as its inputs. This
replaces the shared local build cache considered earlier: that left the first project waiting, and the third-party
libraries it covered were only 4% of the build.

**Cost:** No stepping into engine code in a prebuilt project, and no standard library debug checks; the source build
restores both. Live tuning and the Painter's programmer-error checks are decided per executable
(`ENGINE_TUNING_LIVE`, `Diagnostics/DevelopmentChecks.h`), so both builds keep them. Status: implemented (2026-11-02), window
and debugger checks pending.

### D25. Source control
**Decision:** Git is a fixed part of the editor, not a dock panel. The state and the three actions live in the menu bar's
empty right end; the file list is a drawer that opens above the panels. It runs the system's `git.exe`, not libgit2, so
LFS, the credential manager, hooks and submodules behave as in the developer's terminal. The design serves people who
make games and may be new to git: every state has a next step in plain words, and nothing it offers can lose work. So
there is no force push, reset or discard, Pull only fast-forwards unless the developer chooses Merge, a stash is named and
kept on conflict, nothing is ticked by default, and submodules never leave their branch.

**Why:** A dock panel can be closed, which would hide the signal it exists for, and a fixed one needs no layout code. A
permanent 280 px panel would have cost 53% of a 16:9 scene's pixels at 1600×900 to show what is one line of state most of
the time; the menu bar's free space costs nothing. Pull pulses for 6 seconds and then stays tinted, because a button that
blinks for hours stops being read, competes with the scene and overrides the unread-error and waiting-agent pulses.

**Rejected on purpose:** history graphs, rebase and a three-way merge editor in the first version. They add risk and
little of the engine's own value. That value is in reading game files: pictures and sounds compared as what they are.
Conflicts in text go to the developer's editor or the agent.

**Later:** an agent-written commit message, parameter-level tuning diffs, and marking files an agent changed.
Status: implemented (2026-11-03), first real-window check pending.

### D26. Deterministic simulation, replay and scenario tests
Status: implemented (2026-11-04), first real-window check pending. Usage: [Simulation](../Simulation.md).

**Goal:** gameplay advances in fixed steps from a seed and per-step input, so a run can be replayed, checked by hash and
driven without a window by an agent. The guarantee is the same build on the same machine. Same-screen multiplayer and
replay are in scope; online play is not.

**Measured (2026-11-04):** one toy simulation (7200 steps; float and double arithmetic, CRT math, engine helpers, standard
distributions, containers) and a second program for tight vectorizable loops, compile-time folding, sums and denormals.
MSVC 19.44, x64, one machine, the engine's normal flags (no `/fp` or `/arch` set, so `/fp:precise` and SSE2).

| Compared | Result |
| --- | --- |
| Debug, Release, RelWithDebInfo; each run twice (toy simulation) | Identical at every step |
| Debug against Release (33 values) | Only raw bytes of a struct with padding differ |
| Two runs of the same Release program | Same exception: padding bytes change from run to run |
| Debug against Release with `/arch:AVX2` | Same exception only |
| Debug against Release with `/fp:fast` | 22 of 33 differ; the compiler switches to vector math calls |
| Negative control: one input moved by one unit in the last place | Detected |

This does not widen the guarantee: other compilers, other CPUs and a real game were not measured. Live tuning is a real
Debug/Release difference that this test does not cover: `ENGINE_TUNING_LIVE` reads files in Debug and compiles constants
in Release ([Tuning](../Tuning.md)).

**Follows from the code and the measurement:**
- `Game` cannot run without a window: its constructor creates the `Renderer`, and `Runtime/Game.cpp` belongs to the raylib
  `Engine` target. Headless code cannot go through it.
- Simulation code is built with an explicit `/fp:precise`, so that a project flag cannot change it silently.
- State is hashed field by field, never as raw struct bytes.
- No global random generator. A simulation owns its generators and seeds them from the run's seed; visual code owns
  separate ones, so it cannot touch the simulation's sequence. `WeightedIndex` moves to the engine's generator and
  `std::discrete_distribution` is removed. Its only caller today is a test.
- Hitstop and slow motion stay in `TimeScale`, outside the simulation: scaled time feeds the step accumulator, so only the
  steps per second change. Time is never recorded. The simulation asks for hitstop through state or events that the scene
  reads. Playback seconds are therefore not linear in steps, and steps are the exact position in a run.
- An input latch sits between frames and steps. Presses are remembered until the next step and delivered to one step only;
  held state always follows the latest frame. Cancellation clears pending presses and releases held actions. A press
  during hitstop survives; a paused scene does not feed the latch.
- Too many steps owed in one frame are dropped, up to a fixed limit per frame. The tape counts steps, so this cannot harm a
  replay.
- Tuning belongs to the build, not to the tape. A tape replayed under changed values gives a different run, and the hash
  check reports it.
- Launcher: not affected. It does not call `ctl` and only checks `toolsContract`, and this design only adds commands and
  options, so the contract stays at 2. A bump would make current launchers reject the engine.

**Decided:**
1. *Where the simulation lives (2026-11-04):* a plain `Simulation` class in a low-level target that cannot reach drawing
   or audio, hosted by a scene through a `SimulationRunner`; not a new `TimeDomain` that runs scenes in fixed steps. The
   frame loop in `Game::Run` does not change, and menus and UI keep variable time. The headless tool runs the simulation
   without a scene. One `State(StateVisitor&)` function feeds the hash, the readable dump and scenario checks. Rejected:
   `TimeDomain::Fixed`, because scene children mix views, effects and model in one update chain.
2. *What the tape stores (2026-11-04):* per-player actions, not raw `DeviceInput`. The game declares named buttons (at
   most 32 per player) and axes; the tape stores `held` and `pressed` separately, so a tap shorter than a step survives.
   Axes are rounded to a fixed precision before the simulation sees them, so live play and replay read the same value.
   A command channel carries events that are not keys, such as `{"at": 300, "command": "pick_upgrade", "value": 3}`.
   The engine adds `InputBindings`, a plain player, action and key table; new devices change bindings, not the format.
   Bindings run before the tape, so a replay does not reproduce binding bugs; those need their own unit tests. Rejected:
   raw `DeviceInput`, because it mixes same-keyboard players and ties tapes to key layout, canvas coordinates and devices.
3. *Step rate and `ctl` (2026-11-04):* each simulation declares a whole number of steps per second, 1 to 240, default 60.
   The tape stores it and it does not change during a run; slow motion lowers steps per real second instead. Existing
   commands keep their meaning on every scene: `ctl step N` is N × 1/60 s and `ctl seek S` is playback seconds. New
   options work in exact steps: `ctl step --sim N`, `ctl seek --step N`, `ctl frames --steps`; `ctl status` adds `step`
   and `stepsPerSecond`. On a scene without a simulation these options exit with 1. The accumulator rounds float noise
   to a whole step, so 1/60 s is exactly one step at 60 Hz. `seek --step` runs frames at the fixed step duration until
   the counter arrives, hitstop frames included. Rejected: `ctl step` counting steps in simulation scenes, because one
   command would silently mean different durations by scene and rate.
4. *Editor timeline (2026-11-04):* one log per scene: the tape in a simulation scene, the existing action timeline
   elsewhere. Viewport input and `ctl action` both reach the tape, the action through `SimulationRunner::Command`.
   Rewinding resets the scene and replays the tape to the target from step 0, keeping what follows; playing on replays
   the rest, then goes live. A press or a command during that replay cuts the tape there; mouse movement alone does not.
   Rewinding uses current parameters and recomputes the tape's hashes, so a value can be changed and watched under the
   same input. An action that only changes the view is a parameter, because parameters survive a reset. `ctl replay
   load FILE` and `ctl replay save FILE` are new; `ctl status` adds `replay`. No snapshots yet: rewinding costs what
   `seek` costs today and keeps view effects correct. Rejected: actions on the timeline beside a tape, because a saved
   replay would lack them and seconds and steps drift apart under hitstop; and replacing the timeline everywhere,
   because a scene without a simulation has no steps.
5. *How tools find the simulation (2026-11-04):* a project implements `RegisterSimulations(SimulationRegistry&)`.
   `engine_add_simulation` builds its sources into a static library linked by the game, the worker and the tests, plus
   a console tool that needs no window. The new `sim` command builds and runs that tool: `sim list`, `sim run SCENARIO
   [--out TAPE]`, `sim verify TAPE`, `sim diverge TAPE`, `sim state TAPE --step N`, each with `--release`. Answers are
   one JSON object; exit 1 is a failed check, exit 2 is usage or a missing simulation. Scenarios are JSON files, and C++
   tests use the same `Scenario` class. Tuning moves to its own low-level target, because simulations read tuning.
   Rejected: a mode of the game executable, because `Game` needs a window.
6. *Tape file, hashes, identity (2026-11-04):* text JSON with sparse changes, so an agent can read and write it. Hashes
   are FNV-1a 64 over state fields, at step 0 and then every `hashEvery` steps: one second of steps for game recordings,
   every step for `sim run` and the editor. `sim verify` reports the range the first difference lies in, which is one
   exact step at `hashEvery` 1. `sim diverge` runs a tape twice and names the first differing step and fields, which
   finds nondeterminism such as uninitialized memory. The tape stores the simulation id and version, engine version,
   configuration, step rate, players, schema, seed and start parameters. A different id, step rate or schema refuses to
   play; different versions are reported and the hashes decide; configuration is information only, as measured above.

**As built (2026-11-04):** `HostedSimulation<T>` links a scene's simulation to the editor; the worker keeps the tape in
`WorkerSimulation`. `ctl replay` is the only way to load a replay, because the editor has no file dialog yet. A Build &
Refresh starts a new worker, so the tape does not carry over. The `configuration` in a tape names the engine library, so
a project on the prebuilt engine records Release even in its Debug build. Visual randomness in `ParticleSystem2D` and
`NumberPopup` keeps its own `std::mt19937`: separate from any simulation by construction, so moving it to `Rng` is not
needed for this guarantee.

**Measured on the finished tools (2026-11-04):** a tape recorded by the Debug tool verifies in the Release tool and through
the prebuilt package, with the same hashes, and a tape saved by a running editor verifies in the tool.

**Out of scope:** collision, tweens, 2.5D, the bullet kit, gamepad, online play, crash reports (D12).

### D27. Collision
Status: implemented (2026-11-04). Usage: [Collision](../Collision.md).

**Decision:** collision detection, not a physics engine. `Core/Physics/Collision` is a small API in engine types over Box2D
3.2.0, whose whole tree is vendored unmodified in `ThirdParty/Box2D` and compiled as a library; only the collision files reach
a game, because the linker drops the rest. Box2D's names reach no public header. Dynamics can later sit beside it in
`Core/Physics/Dynamics` without moving anything.

**Parts:** `Shape` (circle, box, capsule, segment, convex polygon up to 8 corners) and `Pose` are plain values. `Collision.h`
answers questions about two shapes. `CollisionWorld` indexes many colliders in Box2D's dynamic tree: `Overlapping`, `At`,
`Raycast`, `Sweep`, `Pairs`. `MaskOf` makes layers from a game's enum. `DrawColliders` outlines colliders for a view.
`EnginePhysics` sits below `EngineHeadless`, depends only on `EngineSimulation`, and is built with `/fp:precise`, Box2D
included. `engine_add_simulation` links it, so a simulation only includes the headers. `Physics/Debug` compiles into
`EngineHeadless`, so a simulation cannot reach it; `check` names the mistake earlier.

**Decided (2026-11-04):**
1. *The world is an index.* The game owns positions and calls `Move`. Rejected: positions living in the world, like a physics
   engine's bodies, because that splits a game's state in two and hides it from `Simulation::State`. The world still appears
   in `State`, so dumps show colliders beside the game's positions. A forgotten `Move` replays the same way every time, so
   hashes cannot catch it; a dump or `DrawColliders` shows it.
2. *`Pairs`, not events.* "Contact started" and "ended" would make the world keep state of its own between steps.
3. *Results do not depend on the tree's shape.* Results come in id order, and equal cast distances go to the lower id. The
   tree is rebuilt by replaying a tape from step 0 anyway, but game rules should not depend on its inner order.
4. *Errors throw in every configuration.* A wrong id or a non-finite pose would otherwise reach Box2D, whose checks exist only
   in its Debug build, and behaviour would differ between Debug and Release.

**Measured (2026-11-04, MSVC 19.44, x64, one machine):** every query kind, 7200 steps of hashed results: Debug, Release and
Release without SIMD gave the same hash, each run twice. A tape from a simulation whose rules use the world, recorded in
Debug, verifies in Release. Costs through the public API in Release, per step:

| Case | Cost |
| --- | --- |
| 1,000 / 5,000 / 20,000 moving objects, each asking what is near, evenly spread | 0.8 / 8 / 105 ms |
| 1,000 / 5,000 of them packed so that nearly everything overlaps | 4 / 100 ms |
| 500 / 2,000 / 5,000 bullets sweeping against 100 / 500 / 2,000 moving enemies | 0.1 / 0.6 / 2.8 ms |

Against a simple grid of 48-unit cells, without exact shape tests on either side, the tree cost 4 ms and the grid 2.7 ms at
5,000 objects; the tree won at 20,000. So the engine has no grid of its own for now.

**Costs:** the engine copy grows by 5.7 MB, and every game's notices name Box2D, because notice scopes cannot follow what a
game links. Polygons are convex with at most 8 corners. A swept shape does not turn while it moves. Box2D stops a cast a hair
before the surface (0.005 units).

**Deferred:** sliding along walls (`b2SolvePlanes`, which needs planes gathered without Box2D's world), contact events, tile
grids, rigid-body dynamics, a fatter tree margin or a grid if measurements ask for one, and a collider overlay switch in the
editor.

### D28. Animation
Status: implemented (2026-11-04). Usage: [Animation](../Animation.md).

**Decision:** own code, in `Core/Animation`, with no third-party library, in three layers over one set of curves. Constant data with no
clock: `Track`, `Clip`, `ClipSet`, `Timeline`. Something that plays them: `Animator`. A value that moves by itself: `Tween`, with `UpdateAll`
and `TweenSet` for convenience. Each comes on steps and on seconds. `Graphics/Anim` is gone: `Graphics` draws, `Animation` computes values
over time.

The first version had only curves and timelines, on the argument that a tween is a position, a curve and `Mix`. That is true and was not enough:
it left the caller to write what people come to an animation system for, so the layers above were added on top of it the same day.

**Library survey (2026-11-04):** Tweeny 4 (MIT, header-only, rewritten in August 2026) is the only serious candidate and came
closer than expected, because its durations are whole frames and `peek(frame)` is const. It was still rejected: no named moments, so
the timeline would have to be written anyway; events are callbacks stored in the object, which a state hash cannot read; `seek` and
`step` need a stored position; its positions are frames only, while views here run on float seconds. Choreograph (BSD-2, last pushed
in 2023) is stateful and looks unmaintained. The others are a few curves, or tied to Unreal or ImGui. What a library would save is a
few short, well-known formulas.

**Decided (2026-11-04):**
1. *`Core/Animation`, not `Modules/` or `Graphics/`.* `check` refuses `Graphics/` inside a simulation, so curves could not stay
   there. The engine core uses curves in 14 files, and a module may not sit under the core; the one existing exception stays the
   only one. Timelines and curves are used together, so they stay together. It is headers only, depending on `Utilities`, so it has
   no target; `Simulation/TimelineMoments.h` is what connects it to a run.
2. *Positions in steps inside a simulation.* `StepTimeline` has `int` positions, compared exactly. `SecondsTimeline` is for views, on
   ordinary time. Both are one template, and a simulation cannot mistake one for the other by writing `1.5`. `StepsIn` lets an author
   think in seconds, rounding once.
3. *Everything in both places.* Rules may need a value that animates, and a view reads the same data between steps. The two forms are one
   template, so there is one thing to learn.
4. *`before <= at < after`.* With a clock from 0, one call per step meets every moment once, including a moment at 0, with no -1
   start. The older `Crossed` counts `before < moment <= after`; it was left alone because views use it on single moments.
5. *Names, not ids.* Agents read and write names, and tools need them. Unknown names throw in every configuration. A misspelled
   comparison inside `ForEachCrossed` cannot throw, which is the known cost; a scenario shows it.
6. *`SpringPoint`, `AnimatedCounter` and `ButtonAnim` moved too.* None of them draws, so they had no reason to be in `Graphics`.
   `ButtonAnim` could not go to `Graphics/Ui`, because `Draw/Neon.h` reads its state and `Ui` depends on `Draw`. Their two `.cpp` files
   became headers: left in `EngineHeadless`, a simulation could include them and then fail to link. They stay for views, because they
   keep private state that `State` cannot reach.
7. *The animator is driven by code, not by a state graph.* A graph earns its place with a visual editor, which is out of scope, and it would put a
   second copy of the rules beside the simulation's. A graph could later sit on top of `Play`; the reverse would not work. Godot's AnimationPlayer is
   the model. Calling `Play` with the current clip does nothing, so rules can state what should show on every step.
8. *A tween is the value, not a pointer to one.* DOTween-style `tweens.To(enemy.scale, ...)` was weighed and rejected. A list of pointers goes wrong when the
   value moves or dies. DOTween and Godot manage that with safe mode, links and automatic kills, which need a runtime that can notice a dead object; C++ cannot, and a
   vector that grows moves every element. Pointers and stored callbacks could not go in `State` either, so simulations would need a second tween type.
   To keep the short form's comfort: tweens read like the plain value, `UpdateAll` makes one line, `TweenSet` owns values by key, and a callback passed to `Update` runs
   and is forgotten. The cost is `.Value()` where C++ cannot work out the conversion, a silent stop if `Update` is forgotten, and `auto` copying the tween.
9. *A pose is a typed struct that lists its fields once.* Named float channels were rejected: a typo would appear only at run time, and `Float2` and `Color4` would need
   more. The compiler checks a track's type against its field. `Fields` carries names, so a text loader stays possible. Its cost is a few lines per pose, and a field
   left off the list compiles and silently does not blend.
10. *An animator's state is clips and clocks.* The pose is worked out from them, so normal play adds no floats to the hash. A fade interrupted by another `Play` is the
    exception: no clip could give that pose again, so it is frozen and written through `Fields`. Clips are shared behind a constant clip set rather than pointed at, so nothing dangles.
11. *A tween's chain is four parts, inline.* It keeps tweens trivially copyable and free of allocation, which matters in a vector of enemies. Clips are for longer sequences. A curve
    is a `Curve` value because a function pointer cannot be hashed.
12. *`Simulation::Moments`, for tools only.* The answer can depend on the run, so `ctl seek --moment` resolves the name in the
   editor from the current status and then seeks by step, with no new worker message. The field is added to the status without
   raising the worker protocol or `toolsContract`, as with the other status additions.

**Measured (2026-11-04, MSVC 19.44, x64, one machine):** a simulation feeding all 22 curves, sampled over their whole range for 700
steps, into its hash: a tape recorded in Debug verifies in Release at every step, and two runs agree. A second one with six units on animators
changing clips at random, tweens and a tween set, 2,000 steps with 259 interrupted fades and every curve used: the same in Debug and Release, at every
step and in its final state. Cost in Release:

| Doing | Each | 5,000 per step |
| --- | --- | --- |
| `Wrap`, `ForEachCrossed` and eased `Progress`, asking for the last name: 4 / 8 / 32 moments | 18 / 30 / 76 ns | 0.09 / 0.16 / 0.38 ms |
| `StepTween`, running on an elastic curve | 10 ns | 0.05 ms |
| `StepTweenSet`: step, and read each by key | 6-36 ns, growing with size | 0.18 ms |
| `StepAnimator`, five tracks: `Play` of the current clip, `Step`, `Pose(alpha)` | 57 ns | 0.28 ms |
| The same, always fading between clips | 80 ns | 0.39 ms |

**Costs:** the include paths of `Easing.h` and the three helpers changed, and `StateVisitor` gained a `Color4` field. Curves are inline, so a game built with `/fp:fast` could lend a simulation another
copy of one; `StepSmoothed` already has this limit, and it was reasoned, not measured.

**Deferred:** a state graph; layers and additive blending; playback speed on steps; custom Bezier curves; loading clips from text; a clip's moments in
`Simulation::Moments`; a scenario operation that runs to a moment; listing a repeating timeline's later cycles; anything in the editor.

### D29. Effects: cues, one shape, and what Animation is for here
Status: implemented (2026-11-04). Usage: [Effects](../Graphics/Effects.md).

**Decision:** names are for cues, not effects. An `FxCue` is constant data, a list of typed actions with times; an `FxCueSet` names them the way a `ClipSet` names clips, and
`FxStage::Play` plays one. Single effects stay typed methods. Effects use Animation's vocabulary everywhere and its tweens only where one thing moves at a time.

**Decided:**
1. *No `fx.Play("shake", ...)`.* Each effect has different parameters, so a single name-based door needs an untyped bag of them, and a typo surfaces only when that effect first runs, perhaps at a boss's death.
   What games repeat is the combination: burst, shake, flash and a late shockwave. So the names go there, as data the compiler still checks.
2. *Actions are a closed `std::variant`, not callbacks.* A cue stays plain data that can be listed, validated when made and later loaded from text. A sound is not an action: Fx would depend on Audio, and a game can
   play a sound of the same name. A game's own effect is a call beside `Play`.
3. *A cue's waiting actions are copied into the stage.* Nothing points at a cue, so nothing can dangle. Delays run on `Update` time, so a hitstop holds them; that keeps a hit's parts together.
4. *`FxStage` gets a `TimeScale*`.* Hitstop and slow motion belong in a cue, and they were a trap: `SlowMotion` on the stage only looked slow, and `TimeScale::SlowMo` only was. It is now `SlowMotionLook`, and `FxSlowMotion` does both.
   A cue that changes time throws without `SetTime`, rather than quietly losing its freeze.
5. *Tweens only for single things.* `ScreenFlash`, banners, `SlowMotionOverlay` and `TimeScale` are one value moving, so they became tweens; `TransitionClock` became a track. Rings, shockwaves and text can be many, and all they hold is an age, so
   they gain a `Curve` in their style instead of up to 90 bytes each. Particles are physics. Shake, punch, pulse and motion react to their own state, retrigger by "the larger wins", and do not fit `To`, which replaces. So do glitch and the toast queue.
6. *`CameraShot` is a pose.* It was the largest gain for the smallest change: `BlendShots` was already a `Mix`, and `Fields()` gives tweens and clips of a camera.
7. *`ParticleRecipes` is data.* `debrisSpeed` and its neighbours are gone: a theme holds bursts, so a game's own burst is the same kind of thing and `FxStage` accepts it. `ParticleAppearance::draw` moved to an `Emit` argument, because a `std::function`
   could not be in a `constexpr` theme.
8. *One rule for styles.* Event effects take a style with each event, so two sizes can run together; `CameraEffects` and `PulseOverlay` are one continuous device and take it at construction. All invalid values throw.
9. *What is not an effect left.* `TimeScale` to `Runtime/`, and `TransitionClock` to `Scene/`, because `check` keeps simulations from both; `Animation/` would have opened the rule that simulations do not call `TimeScale`. `CollectFlights` to `Progress/`, `ShatterPattern` to `Utilities/Geometry/`,
   `DrawIrisTransition` and `DrawSpeedLines` to `Graphics/Draw/`. `FxImport.h` stays: it is the documented umbrella header.
10. *A fixed default seed, on `Rng`.* Particles were seeded from `std::random_device` when none was given, which made `ctl frames` differ between runs. Nobody can see that two runs of a burst are alike. `NumberPopup` keeps its `std::mt19937`; this was not needed for its optional seed.
11. *Folders by what an effect acts on.* `Particles/`, `Camera/`, `World/` and `Screen/`, with what ties them together (`FxStage`, cues, `FxStyle`, `FxQuality`, `FxText`) at the top of `Fx/`. 35 flat files read as one list.

**Fixed on the way:** with motion or flashes off, slow motion's trails and tint still drew; decays depended on the frame rate; a kick was a copy of `SpringPoint`; `Shockwaves` and `PulseOverlay` took invalid styles silently.

### D30. Effects: two clocks, shake as an amount, cues with a size
Status: implemented (2026-11-04). Usage: [Effects](../Graphics/Effects.md).

**Decision:** `FxStage::Update` takes game and real time, a shake is an amount rather than a duration, and one play of a cue has a size and may be limited by name.

**Decided:**
1. *`Update(gameSeconds, realSeconds)`, with no one-clock form.* `TimeScale` counts real seconds while everything in the stage counted game seconds, so slow motion's look outlasted it: at a constant 0.1 for 2 seconds, it lasted about 3.8. Toasts froze in a hitstop as well. A second
   method could be forgotten and leave them on the screen, and a clock read from `TimeScale` would be stale while paused. Passing both cannot be forgotten. `Game::RealSeconds()` is new for this. A flash, however, keeps game time: a hitstop holds it, which keeps a hit's parts together.
2. *Trauma instead of seconds.* `Shake(seconds)` had one strength, so only length separated a tap from an explosion. An amount that adds up to 1 and shows squared separates them, and hits in a row build up without a special rule. Smooth noise replaces two sines, whose pattern was the same each time, and adds a small turn.
   The old form is gone rather than kept beside it. The model is the one usually credited to Squirrel Eiserloh's GDC talk; this is from memory.
3. *Styles are checked when a stage is made.* D29 refused name-based effects because a typo would wait for a boss's death, and a bad theme value did exactly that. Each style has `IsValid`, its effect uses it, and `FxStage` checks them all and names the part. `DrawSpeedLines` could throw halfway through a frame for the same reason.
4. *`intensity` scales sizes, not times.* A small hit is smaller, not shorter, and a hitstop should not depend on it.
5. *A cooldown belongs to a name in a set, not to a cue.* Only `Play` by name has an identity to remember, and the same cue under two names may want two limits. It runs on game time. `Play` returns whether it played.
6. *`WorldCamera(paint, base)`.* A camera that scrolls could not use the stage's shake or punch at all. The pulse's tint moved to the overlay, since a rectangle in world positions would scroll away.
7. *Full particles replace the oldest.* Dropping new ones let a steady trail hide an explosion.
8. *Flashes are rationed always, not as a setting.* At most three starts a second, the usual limit for photosensitivity (WCAG 2.3.1, from memory). A stronger flash asked for too soon is lost too: delaying it would put it out of step with its event. With flashes off, shockwaves and a banner's curtain also go, as they light the screen in the same way. `ScreenGlitch` keeps its fast pattern behind that setting.
9. *`std::visit` with an overload set.* A new `FxAction` without a case no longer compiles; the `if constexpr` chain did nothing, silently.

### D31. Game UI
Status: first part implemented (2026-11-04). Usage: [UI](../Ui.md).

**Decision:** a small kernel that knows no widget, and everything else built on its public API. A screen is code that says what is on screen each frame; a tree is built from it, laid out, and drawn, and the next frame's input is judged against what was drawn. Behaviour and looks are separate, and `check` keeps them apart.

**Decided:**
1. *Described every frame, not a tree of kept objects.* Kept objects are a second copy of the game's state, which is kept in step by hand: a forgotten refresh shows wrong data with no error, a button can be deleted by its own callback, and a replay that seeks backwards would need every object refreshed. Here there is nothing to keep in step. It also obeys what D28 already said: no pointer to a value it does not own, no stored callback. The drawing lambda lives one frame. The whole screen is redrawn each frame anyway, so kept objects save little.
2. *A real layout pass, not a pure immediate mode.* Sizes come from content, which an immediate mode cannot know while drawing. It is why answers come from the last frame's rectangles: a click lands on what the player saw, and that one frame of delay is not visible. Nodes are a flat list with parents first, so the passes are plain loops, widths before heights because a wrapped text's height depends on its width.
3. *Direction keys find what lies that way on screen.* No element lists its neighbours, so a grid beside a footer needs no focus code. Sideways distance counts three times, which keeps a list a list.
4. *The kernel knows no widget.* `UiButton` is a function over public API, and so is a game's. This is what makes the system grow: new widgets and looks are additions. If one needs a change in the kernel, the kernel is missing a general idea, and that is decided here.
5. *Each widget's look is a type, found by type.* A closed interface with `DrawButton`, `DrawSlider` and so on would have to grow with every widget, and a game could not add to it. A look's default values draw it plainly, so a widget always works and a look set holds only what differs.
6. *Plain by default, neon as a choice (the user's).* Neon widgets used to be what the theme could only tune: `UiStyle` was made of neon types and `DrawUiButton` always drew neon, so a game could not be other than neon. `Theme` no longer holds widgets. The default palette and backgrounds are quiet too, since neon colours on plain widgets look wrong; `FxStyle` stayed as it was, because effects are not UI.
7. *A cursor per player from the start.* Same-screen play is in the engine's direction, and adding it later would have changed how focus is stored. A game with one player never sees it. The mouse acts as the first player, and does not reach an area a player owns when several are playing (the user's choice).
8. *Keyboard and mouse only, for now (the user's).* Gamepad and touch are not read. Moves are read through `UiKeys`, so a gamepad would be one more source of them.
9. *Layers carry input, focus and closing.* A closing layer keeps being described without taking input, which answers the usual problem of this model, content that animates out. An item leaving a list stays in the game's data until it is done; that is the honest limit.
10. *Sounds are events.* `AudioSystem` is above `EngineHeadless`, and a widget playing sounds would tie behaviour to audio. `Events()` also serves effects with no new mechanism.
11. *No new CMake target.* `GameSettings` holds looks and looks need the Painter, so a separate target adds a cycle or a wall where the linker already drops what is unused. `check` enforces the layers instead.
12. *No visual editor now, with the door open (the user's).* A text file could produce the same tree later. It would be more work than on kept objects; that cost is accepted. If an editor becomes a near goal, reopen decision 1.

**Measured:** a screen of 302 elements with 240 buttons, a cursor moving every frame: 0.18 ms to build and lay out and 0.15 ms to record its drawing per frame, on one machine in Release (`[.ui-perf]` in `UiTests.cpp`), about 2% of a 60 Hz frame. A null GPU device was used, so what the GPU itself costs is not in that.

**Added with the controls:**
13. *An element can take the direction keys.* A slider's left and right were the first thing that needed the kernel to change, and the change is general: `UiTraits::capture` and `Step()`. Everything else, a slider's drag and a stepper's arrows, is one element with its area worked out in the look's header, which input and drawing share.
14. *Tabs are one stop for the cursor.* Were each tab a stop, going up from the right of a page would land on another tab and switch it. The row takes the cursor and left and right; each tab is mouse-only.
15. *A cursor can go where it cannot be seen.* Elements keep two rectangles: where they would be, for the cursor, and what is left after clips, for the mouse. Otherwise a keyboard could never reach a row out of view.
16. *A list follows the keys, not everything.* Bringing a cursor into view only when keys moved it stops a mouse-hovered row from scrolling away under the mouse, and a list scrolled by the wheel from jumping back.
17. *Kit over plain values.* `AudioSystem` and `Game` are above `EngineHeadless`, so `UiOptionsScreen` works on `UiOptionsValues` and can be tested without a window. `Game::UiOptions` and `Game::ApplyUiOptions` connect them: public, because a scene holds a `Game&`, and the setters they use are protected. Which sounds it plays stays out: they are the game's.

**Added with sounds (2026-11-04):**
18. *Automatic, through a listener (the user's).* A game gives its sounds once. The kernel gains one general idea, `UiListener`, told at the end of each `Ui::End`, and knows nothing of what it does; `UiSoundListener` in `Ui/Sound` is the one a `Game` sets, and a test sets its own with no audio. It is global for the reason `ActiveUiLooks` is: no screen should have to be handed the game. It is read at each `End` rather than kept, so a `Ui` built before the `Game`, or outliving it, cannot dangle, and with no `Game` a `Ui` is as silent and headless as before.
19. *A widget reports a change; the kernel does not guess.* This replaces `UiEventKind::Stepped`, and is the real question here. The kernel emitted `Stepped` when a direction key reached a capturing element, before anyone knew whether the slider was at its end. Only the widget knows whether its value moved. Rejected: having the kernel compare values, since it holds no pointer to game data by design (decision 1); a `Stepped` that a widget can cancel, which makes every game's widget that forgets it noisy, and a held key at a slider's end a machine gun; patching the check into the slider alone, since a toggle, a stepper list of one and tabs have the same question, and a mouse drag was never a step at all. So `UiElement::ReportChange(user, sound)` adds a `Changed` event, and each control calls it once from one `if (value != before)`, whichever of keys, a press or the mouse moved the value. That one path is what fixes a silent drag too. `Step()` stays as it was: it is input, and `Changed` is the outcome. The cost is that a game's value widget must call it; the failure is silence, not noise, and is plain to hear. `ReportChange` is an ordinary public call, so an engine widget has no power a game's lacks.
20. *`Back` is an event only when a screen answered it.* It was emitted whenever Escape was pressed, so with automatic sound a key that does nothing would make a noise. Now the first true answer of a frame from `Ui::Back` adds it, which is why that call is no longer `const`. A question with an effect is not pretty, but the alternative, a separate `ui.ConsumeBack()`, would let the two disagree and be forgotten. Below a dialog it answers false, so the dialog's Escape is one event.
21. *`UiSound` is a meaning the kernel carries and does not read.* `UiTraits::sound` and each event's `sound` are an enum: `Auto`, `None`, `Move`, `Step`, `Confirm`, `Back`, `Deny`, `Open`, `Close`. That keeps `SoundKey` out of the kernel and out of widgets (`check` enforces both), and it means a `UiButton` can say "this is a back button" in a screen that does not know the game's enum. `.sound` governs what a press or a change plays. The cursor arriving still plays `move`, and a refused press still plays `deny`, because a button that goes silent when disabled tells the player nothing, and the game cannot play its own for a press that never reached it.
22. *A control's press has no sound of its own.* The rules "a press plays confirm" and "a toggle changing plays step" collide on a toggle pressed with Enter: the same act, two sounds. The change wins, since it says more. So `UiControlRow` and tabs set `UiTraits::sound = None`, and only `ReportChange` speaks. This also settles what a slider does: letting go of a drag is a press in the kernel's terms, and it must not end in a confirm. A press on a control that changes nothing is silent, which goes further than "every time" on purpose. The scroll bar's press is silent for the same reason.
23. *`UiSounds` lives among a `Ui`'s looks.* It has the life a look has: a value per type, one for the whole game that one `Ui` can replace, copied into a new `Ui` from `ActiveUiLooks`. A second registry would do the same thing with a second global. `Game` sets it from `GameSettings::uiSounds`, which always wins over one placed in `uiLooks` by hand, so there is one door. Its weak point: assigning a whole look set to `_ui.Looks()` drops the sounds, silently. The docs say to use `Set`.
24. *"Never over itself" is the listener's rule, not the row's.* A row's default is `Copies(4)`, and the user wanted this to hold whatever the row says. `UiSoundListener` asks `SoundPlayer::IsPlaying` for a cursor move and a change, and drops the new one. It goes by the event's kind, not by the sound it resolved to, so `{.sound = UiSound::Confirm}` on a slider is not a machine gun either. `IsPlaying` goes through the player, like everything else, and reaches the device only through `SoundOutput`.
25. *`back` plays once in a frame.* A back button pressed and a back key pressed in the same frame are one act. Nothing else is limited this way: "every time" stays literal, including two players pressing confirm in one frame and a held direction key on a disabled control, which repeats its deny.
26. *Checks in every configuration, at the earliest moment that can see them.* `Game`'s constructor checks `settings.uiSounds` with `CheckUiSounds`, not a development check, since a missing sound must not wait for a menu nobody tested. One `Ui`'s own set can only be seen once it exists, so the listener checks it at each `End`, which fails on the first frame that screen shows rather than the first time a rare button is pressed. It compares seven keys against the tables, per `Ui`, per frame.

Left out with sounds, by the user's choice: pitch variation or anything per event beyond what a row gives, a sound for every kind of hover, default sound files. A layer has no `.sound`: `open` and `close` are one pair per `Ui`. A modal layer that closes because of Escape plays `back` and, a frame later, `close`, if a game gives both: nothing links them, and a game that finds it too much leaves `close` empty.

**Why these names:** `Fill` is already a Painter style, and a global `Button` would hide a game's own `enum class Button`, so all UI names start with `Ui`.

**Order of work.** Done: kernel, layout, `UiLabel`, `UiButton`, `UiPanel`, `UiRow`, `UiColumn`, `UiGrid`, `UiSpacer`, `UiLayer`, `UiToggle`, `UiSlider`, `UiStepper`, `UiTabs`, `UiScroll`, `UiTooltip`, plain and neon looks; `Core/Menus` is gone; `UiOptionsScreen`. Sounds through events are done. Next: more of `Ui/Kit`, confirm and pause; a widget gallery in the editor; `ctl ui`, which lists what is on screen and presses it by name for agents. Later, when a game asks: tables, large lists, rich text, dragging, a second look beyond neon, text entry (`DeviceInput` has no character input yet).

### D32. Sound tables
Status: implemented (2026-11-04). Usage: [Audio](../Audio.md#sound-tables).

**Decision:** a game's sound effects are a `constexpr` table with one row per value of its own enum, and `game.Sounds().Play(Sfx::Hit)` plays one. The table generates ordinary catalog entries, so `AudioSystem` stays what it was and the tools keep working.

**Decided:**
1. *An enum, not strings (the user's).* A misspelled name would fail only while the game runs, perhaps only at a boss's death, the argument of D29. A name in a catalog would give nothing the enum does not, since a game's sounds are known when it is built.
2. *No hand-numbered IDs.* `SoundId` constants and catalog rows had to be kept in step by hand, and pitch and variant choices were repeated at each call. With a row, they are said once, and `RowsInIdOrder` makes a new value without a row a compile error.
3. *A constructor that throws, so one check serves both worlds.* In a `constexpr` table the `throw` is a compile error at the row; a table made while running, such as one loaded from text later, gets the same check as an exception. A game writes no second `static_assert` for it. The price: `SoundRow` is not an aggregate, and is made positionally with its options last.
4. *The row count is in the type.* `SoundTables` takes `std::array<SoundRow<Enum>, Size>` only when `Size == Enum::Count`, so a missing row fails at the assignment even without the `static_assert`. Order needs the values, so without the `static_assert` it is caught at startup.
5. *`std::array<std::string_view, 8>` for files.* The simplest form that stays constexpr, with no helper and no second template parameter. A ninth file fails to compile. If a game wants more variants, raising the number costs 16 bytes per row in a table that is small anyway.
6. *`SoundOverlap` is one value.* `Copies(n)` or `Skip`, so "skip, with four copies" cannot be written.
7. *`Copies(n)` counts per file.* It maps to one catalog entry's voices. A row with three files can reach 3n. An exact limit across a row needs stopping a voice of another entry, which `AudioSystem` cannot do, and the point of the limit is to keep a burst from becoming a wall of sound, which per file still does.
8. *`Skip` looks at the device.* It is the only decision that does, because only the device knows what still plays. So `SoundOutput` has two calls, `IsPlaying` and `Play`; the plan's "only the final play call" holds for everything else. `AudioSystem::IsEffectPlaying` is new, for a row with several files; `PlayEffectIfIdle` sees one entry.
9. *`SoundPlayer` is in `EngineHeadless` behind `SoundOutput`.* All of its logic, including the skip rule and its errors, runs in tests with a fake. The two lines that reach the device are `AudioSoundOutput`, in the Engine target.
10. *Cooldowns read unscaled frame time.* `Game` advances the player before a host's `FrameSeconds`, so an editor's speed setting or a pause does not stretch them. A hitstop does not either: it changes game time.
11. *Generated IDs start at 2^32.* A game's own IDs are far below; one above is rejected at startup. They are not hidden from `AudioSystem` by the type system: `LoadEffect` with such an ID is not checked, because nothing has a reason to do it. Next free ID instead of a fixed start would have made a table's IDs depend on the catalog and collide with a later `LoadEffect`.
12. *One door in the editor too.* `SceneServices::sounds` is the same player as the worker's `Game::Sounds()`, since `WorkerGame` is a real `Game` with the project's settings. `settings.audio` entries and tables reach `audio catalog` and Audio Preview through `CatalogWithSounds`. Thumbnail runs clear both, as they always cleared the catalog.
13. *`settings.sounds = {A, B}`, not `Add`.* One statement says the whole list, and a second assignment reads as what it does. The user's `settings.sounds = SfxTable;` works unchanged.

**The hole that remains:** a `SoundKey` of an enum whose table was never registered compiles, and throws when played. It is narrowed: an enum without `Count`, such as `SoundId`, does not convert, and `Has` lets a system check keys at startup. Closing it at compile time needs one of: making `Game` a template on its tables, which spreads through the engine; a type-level link from enum to table, which still does not prove the link reached `GameSettings`; or friend injection to make a missing registration a link error, which relies on CWG 2118, a behaviour the committee means to forbid. Not built: every key type seen by the linker could register itself at static initialization, and `Game` could check them against the tables at every start. No game would write more, and a forgotten table would fail on the first run, not at the boss's death; the cost is global state, and a test program that plays with a fake but links a `Game` would need care. The user's to decide.

**Left out, and why:** strings, as above. Mixer, buses and ducking: no game has asked, and the row options leave room. 3D or positional sound: a game can pass a pitch, and nothing else needs it. Music in the table: a track is one streamed file with a volume group, and has none of a row's choices. Scanning asset folders: it would bring back what a typo costs. Loading tables from text: not built; decision 3 keeps it possible, since `SoundRow` checks at run time and `SoundTable` owns its text, though a text table has no enum, so it will need a way to make a key. A sound in `FxCue`: D29 decided against it. Default sound files: the engine ships none (the user's). `Stop` and per-row volume changes: nothing needs them yet; they are one line each over the row's IDs.

### D33. Inspector and tuning
Status: implemented (2026-11-04). Usage: [Tuning](../Tuning.md#editor), [Editor usage](../Editor/Usage.md).

**Decision:** the Inspector edits both kinds of value: a scene's controls and the tuning files the running scene reads.
The Tuning panel is gone. The file format, the generated header, the all-or-nothing apply and Release `constexpr`
values stay; what changed is the editor around them and the plumbing between editor, file and worker.

**Why:** the first design ([Tuning](Tuning.md)) gave tuning its own panel because a file is project-wide. Used, that
split showed two parallel systems: two value types (`SceneValue`, `TuningValue`) with two colors, two widget sets, three
validations, a flat file list with no link to what is on screen, Enter-only boxes because "the file is the only path"
ruled out a preview, and no undo or "what changed". Unity and Godot edit both in one Inspector; their known pain is that
a play-mode change is lost because it does not say it is temporary. So here one Inspector shows both and every value
says what it is.

**Decided:**
1. *What the Inspector shows (the user's).* The selected scene: its controls, then one section per tuning file the
   running scene has read. Or one tuning file on its own, opened from the Inspector's file list or an Assets tile, with
   a way back. Picking a scene returns to it.
2. *Which files a scene reads.* `Tuning<T>()` marks its file in the current use period; the worker starts a period on
   every open, reset and rewind and sends `tuning-used` when the list grows. A file read only on a code path not taken
   yet is not listed; the file list reaches it.
3. *One model, one widget set.* Scene parameters and tuning fields convert to `PropertySpec` / `PropertyValue`
   (`Editor/Ui/Widgets/Properties.h`), drawn by `PropertyWidget`. Scene parameters keep their sliders; tuning numbers
   drag, so no range is needed and `min` / `max` stay validity bounds. A scene color is `Color4`, as in tuning files.
4. *The file stays the only lasting write path; a preview is not one (the user's).* While a value is dragged the editor
   sends it to the worker, which writes it in memory only; the end of the drag writes the file once, a one-line diff,
   and the worker rereads it at once. Hand and agent edits still take the file path; a preview is lost by any read.
5. *Modified means "since the last commit".* The committed text comes from `git show HEAD:` on a thread of its own
   (`TuningBaselines`); the reset button and Revert write it back. A value's default is what the repository has, not
   what a build compiled. Without git or a commit nothing is marked.
6. *Undo is the editor's own writes.* Whole-text before / after pairs; one is undone only while the file still holds
   what it wrote, so an agent's later edit is never overwritten.
7. *Editor text belongs in the file.* `note`, `unit`, `group` (one line each, optional) serve the Inspector, the header
   comment and an agent reading the file. Labels come from names.
8. *Choices are generated.* `"type": "choice"` with `choices` makes an `enum class` and its names in the header, so the
   file is the only list (the first design ruled enums out as a second source; generating them removes it).
9. *Lengths from the Inspector.* `+` / `-` copy the last element or row, or remove one, through `ResizeTuningField`;
   the scene reports Build needed until Build & Refresh, as for a hand edit.
10. *Paths from the project folder.* The header records the file's path relative to the project; the executable finds
    the project from its own folder (`Project.json` above it). Moving a project no longer needs a rebuild.
11. *New files show at once.* `TuningFiles.json` (version 2) also names the project folder and each target's root; the
    editor scans the roots, and a file no build has seen yet is Not built.
12. *Tapes name changed tuning.* A tape stores a digest of each registered file's values after its first step;
    `sim verify` reports `tuningChanged` when a replay disagrees and values moved since.
13. *Panels are saved by key.* Removing a panel shifted the old index-based visibility arrays; preferences now save
    `{"inspector": true, ...}`, and the old arrays are read once in their old order.

**Left out, and why:** a list column's length in a table (every row would change at once; edit the file). Undo of
scene controls (they last a session and reset in one click). A preview of a file the scene does not read (nothing would
show it). Tuning files outside an `engine_add_tuning` root (no build could read them).

### D34. Simulation events
Status: implemented (2026-11-04). Usage: [Simulation](../Simulation.md#events).

**Decision:** a simulation tells its view what happened by emitting typed events into a list it owns
(`SimulationEvents<E>`); each view reads them through its own cursor (`SimulationEventReader<E>`). Events are output,
not state.

**Why:** a simulation may not play sound, start effects or call `TimeScale`, so the view must learn that a hit
happened. The recipe compared a counter (`HitCount` against `_hitsSeen`): a frame that runs several steps loses how many
events there were, in which step and where, and every game rebuilt that bridge by hand. Bevy's `Events<T>` with per-reader
cursors and Photon Quantum's frame events (deterministic simulation, events kept out of the state, no replay of events
during re-simulation) were the models.

**Decided:**
1. *One list per game, a `std::variant` of plain structs.* One declaration, and the order between types is kept (a Hit
   before the Death it caused). A single struct works too. Each type names itself (`static constexpr Name`) so tools can
   show it; its fields go through an optional `State(StateVisitor&)`, the same visitor the state uses.
2. *The game owns the list; the base class does not learn its type.* `Simulation` gained only `EventLog()`, a type-free
   view for tools, null by default. No pointer into the simulation is stored, so a copy (N7 snapshots) stays valid.
3. *`Emit(step, event)` stamps the step.* The simulation already holds the step, so no hidden hook tells the list where
   a step starts.
4. *Readers keep cursors; the list keeps a time window.* Events are numbered; a reader returns what it has not seen, so a
   sound player and a popup do not consume each other's events. The list keeps the steps one frame can run
   (`SimulationMaxFrameSeconds`, shared with the runner) and one more, trimmed when the next event is emitted, so a
   headless run of a million steps stays bounded. A reader that falls further behind counts `Missed()`.
5. *Catching up is silent by default.* The runner holds `EventsSilencedUntil`; `RunTo` sets it to its target and the
   editor's seek sets it until it arrives. `Read` skips those steps; `ReadAll` does not, for a view that wants them.
   Without this a seek would play the sounds of every step it passed, because it runs the normal frame loop.
6. *Tools see events.* `Scenario` collects them every step; scripts gain `expectEvent` and `untilEvent`; `sim run`
   reports a count per name and, with `--events`, the list. Additions only: `toolsContract` stays.
7. *`Utilities/Overloaded.h`* builds the visitor. A missing alternative fails to compile, which shows every place a new
   event type must reach.

**Left out, and why:** a mapping from event types to cues and sounds as data (five lines of `std::visit` today; add it
when games repeat the same mapping, and it would need a home that knows both Fx and Audio). Queues per type (a visitor
is one line with `Overloaded`, and order between types would be lost). Listeners or subscriptions inside the simulation,
and events that feed back into the rules: those are state. Events in the hash (they follow from the steps). A `ctl`
command listing the editor's events (`sim run --events` covers agents for now).

### D35. Game camera
Status: implemented (2026-11-04). Usage: [Camera](../Camera.md).

**Decision:** a `Core/Camera` module with `Camera2D` as the entry point and `Cameras` as the scene's set (`Main()`, split
screen). It computes and draws nothing, builds with precise floats, and links below `EngineHeadless` (`EngineCamera`), so
the same camera serves a view on seconds and, when the camera decides gameplay, a simulation on steps.

**Why:** almost every game with a world larger than the screen needs a camera that follows, and same-screen multiplayer
needs one that frames several players. The engine had the pieces around one (`CameraView`, a cinematic `CameraShot`,
shake in `CameraEffects`, `SpringPoint`) and `FxStage::WorldCamera(paint, base)` waiting for a "normal camera" that no
game had. Godot's `Camera2D` (drag margins, limits, smoothing, a physics or idle update), Cinemachine (dead zone,
look-ahead, damping, confiner, target groups with weight and radius, group framing, `Camera.main`) and Itay Keren's
"Scroll Back" (GDC 2015) were the models.

**Decided:**
1. *One class a user meets first.* `Camera2D`: `Follow`, `Update`, `Snap`, `SetPose`, `View`, `ToWorld`, `Shows`. The
   rules behind it (gathering targets, dead zone, fitting, limits, snapping) are pure functions in
   `Camera/Internals/CameraSolver`, tested one by one; settings are plain structs with designated initializers.
2. *A camera may be part of the rules.* Platformers and scrollers wake enemies as the view reaches them and never scroll
   back; then the visible area is gameplay. So the module may be included by a simulation (`check` holds `Core/Camera`
   to Utilities and Animation; its `Debug/` draws and is forbidden to simulations). One class serves both, as Godot's
   `process_callback` does: `Update(step.seconds)` and `State` in a simulation, `View(alpha)` to draw between steps.
   This replaces NEWSYSTEMS' "never inside a Simulation", which would have forced a second, shared pure function.
3. *`CameraView` moved to `Core/Camera`.* It is plain math, and the simulation side needs it. It gained `viewport`, so one
   view says where on the canvas it draws, and `VisibleArea` and `ConfinedTo`.
4. *`Painter::Camera(view)`.* One scope that clips to the view's viewport and transforms, so split screen is a loop and
   leaves no clip behind. Graphics depends on the camera's view, not the other way.
5. *Critically damped smoothing.* `Animation/SmoothDamp.h` (Game Programming Gems 4, Unity's `SmoothDamp`): no overshoot,
   about the same course at any frame rate, deterministic on fixed steps. `SpringPoint` stays for UI.
6. *Sizes as fractions of the view.* Dead zone and look-ahead limit hold at any zoom, as in Godot and Cinemachine. The
   look-ahead is seconds of the targets' measured motion, eased by its own smoothing, per axis.
7. *Several targets.* Weight moves the middle; with `zoom.fit` the zoom fits every target's circle plus padding between
   `min` and `max`, and the camera is then moved the least to keep them all in view, so weights never cost a player the
   frame. Too far apart at `min`: centred, and the game decides whether a player may leave (clamped by its rules).
8. *`Cameras` and split screen now.* `Painter::Clip` made it cheap. `Split(1..4, layout, gap)`; new cameras copy the
   main one, so opening a split does not jump; `At(point)` finds the camera under the mouse.
9. *Effects over the game camera.* `FxStage::WorldCamera(paint, view, limits)`: a shot blends from the view in world
   positions and keeps the view's place on the canvas (it used to move to the stage's area, wrong in a split); with
   `keepInArena` it stays inside the camera's limits.
10. *No camera stack, no priorities.* A scene holds as many cameras as views; the shot and the effects are the layers
    above. Cinemachine's brain and blends between virtual cameras can come when a game needs authored camera switches;
    a tween of `CameraPose` through `SetPose` covers a cutscene today.

**Left out, and why:** effects per view of a split screen (one `FxStage` shakes all views; a game with separate shakes
keeps one stage per view). A dynamic (Voronoi) split. Soft zones, platform snapping and cue focus (Keren's terms) wait for
the platformer kit. Rotation-aware limits and fitting. A camera panel in the editor.

### D36. Graphics own their look
Status: implemented (2026-11-05). Usage: [Asset library](AssetLibrary.md#authoring-rule).

**Decision:** a ready-made graphic lives in `Assets/Graphics` and carries its own look. Calling it with no look draws it
as intended; a caller overrides only what it needs. `.asset.cpp` keeps registration and passes no colors, so the
Assets panel shows the original. `Core/Graphics` keeps drawing primitives, helpers and what a Core system calls.

**Why:** the panel drew icons in one near-white color because the only colors lived at the game's call sites. A
registration that chose colors was a second opinion on the drawing, and the engine's own icons broke the location rule
that games already followed.

**Decided:**
1. *What moves.* Anything no Core system calls: `DefaultIcons`, `Backgrounds/Backdrops`. Core never depends on Assets, so
   `Neon` (used by `Ui/Looks/Neon`), `IrisTransition`, `SpeedLines`, `ParticleDraw` and `DrawVignette` stay. Moving `Neon`
   needs a look-pack design of its own.
2. *One signature.* `DrawX(paint, ..., const XLook& look = {})`. A single drawing's defaults are field initializers. In a
   set whose items differ, fields are `std::optional` and an empty one takes that item's value (`IconLook`).
3. *Fading.* `Painter::Opacity` fades a drawing without choosing its colors. A translucent `color` still works, and an
   icon's own detail fades with it.
4. *Non-color defaults too.* Stroke width follows `size`, so previews and small icons keep their proportions.
5. *Themed graphics stay themed.* Backdrops and UI widgets take colors from the active theme and looks. A project on
   `PlainTheme` sees grey widgets in the panel, as it does in the game.
6. *Target and includes.* `EngineAssets` links above `EngineHeadless`; games and workers link it. Its include root is
   `Assets/`, so the path reads `Graphics/DefaultIcons.h`. `Assets/Graphics/...` would resolve to a project's own file
   of the same name, silently.
7. *Registration stays apart.* Prebuilt libraries compile once, so an `#ifdef` inside `X.cpp` cannot keep the catalog
   out of a game for engine graphics. The item-name table stays hand-written: C++20 cannot list enum names.
8. *Folders stay free.* A graphic may sit directly under `Graphics/`, as `DefaultIcons` does, have its own folder, or
   share one such as `Enemies/`.

### D37. Unused sounds stay out of the package
Status: implemented (2026-11-05). Usage: [Release](../Release.md#only-the-sounds-a-game-uses),
[Audio](../Audio.md#ready-made-sounds).

**Decision:** `release` packages only the sounds a built game names. A project can keep any number of candidates, and the
engine can offer ready-made sounds that no game pays for unless it plays them.

**Why:** files were packaged by location, so an unused sound shipped silently, and an engine sound had to be copied into
each game to stay out of the others.

**Decided:**
1. *The game answers.* Settings are C++ values and may be computed, so reading source would guess. The built game prints
   its asset manifest when `ENGINE_ASSET_REPORT` is set, before opening a window.
2. *Release only.* Development builds copy everything, so a new sound works after one new line.
3. *Sounds only.* A game may read its other files by itself, and nothing lists them. Fonts stay too: `settings.font` need
   not be in the manifest.
4. *`keepAssets` for the gap.* A sound loaded by code after startup is invisible to the report. This is the remaining
   risk: it works in development and fails in the package, so the summary names every sound left out.
5. *A hook wins.* `package_assets` already selects every asset, and `keepAssets` beside it is an error.
6. *Engine sounds by path.* `Engine/Audio/...` is copied beside every game and `Engine` is reserved under `Game/Assets`, so
   the path means one thing. The editor and `audio` tools read such a path from `Engine/Assets`.

### D38. Lighting
Status: implemented (2026-11-05). Usage: [Lighting](../Graphics/Lighting.md).

**Decision:** `Graphics/Light/LightLayer` darkens what was drawn so far to ambient plus the lights. The lights add up in an
image of their own, which is multiplied over the target. It is view code and decides nothing.

**Why:** a game could build this from `Surface`, `Painter::Glow` and `Blend::Multiply`, and would have met three traps: a
glow disappears when the player lowers the quality, leaving the world dark; an opaque multiply erased the bloom source; and
the surface does not inherit the camera.

**Decided:**
1. *In `Graphics`, not a folder of its own.* Light draws and stands on `Painter`, `Surface` and the camera's view, and the
   rule that keeps `Graphics/` out of simulations covers it for free. `check` keeps it from reaching any further, and
   `Paint` from reaching it.
2. *Lights are data, described every frame.* `Draw` methods are `const`, and the UI already works this way. There is no list
   to clear and no handle to keep, and a split screen is two calls with one frame.
3. *`Painter::Light`, not `Glow`.* A glow is decoration and follows `RenderQuality::glow`. A light decides what can be seen, so
   no setting removes it. `LightStyle` became `RadialGlowStyle`, so that the word means one thing.
4. *One primitive for lit and shadowed.* `Painter::Light` takes a polygon of what the light reaches. The shader computes the
   falloff per pixel from the offset to the light, so a shadowed light is a fan of a few triangles in the same batch, with
   no pass per light and no stencil.
5. *`Blend::Multiply` changes colour only, for everyone.* The blend also ran on the bloom attachment, where the source's
   emission of 0 erased it: a 50% grey multiply halved the colour and removed all bloom. No new blend state: one rule for
   every use.
6. *Emissive drawing goes after the layer.* The halo survives anyway; the colour does not. A mask to exempt shapes would
   mean drawing them twice. The price, that they lie over everything before them, is written in the guide.
7. *Half resolution by default.* Lights are soft, it costs a quarter of the pixels, and filtering softens hard edges.
8. *No quality setting, normal maps, or brightening past a pixel's colour.* A setting needs a measured cost first. Normal
   maps have no source in code-drawn art. Going past 1 needs a floating-point canvas, which changes every game's cost.
9. *`Painter::CurrentTransform`.* A layer called inside a camera stamps its image in the wrong place, which an agent cannot
   see in a test. Now it is an error at the call.

**Shadows:**
10. *Colliders do not stop light by themselves.* A hitbox answers "what does it hit", an occluder "what shadow falls". A
    forgiving hitbox would cast a shadow smaller than its sprite, and glass or a trigger would cast one at all. Nor is there
    a flag on a collider: `layers` goes into `State`, so a look would change the replay hash.
11. *A one-way bridge, in one file.* With no bridge, a game defines each wall twice, and light leaks through the difference,
    which no test catches. `AddOccluders` reads a `CollisionWorld` and writes nothing back. `check` lets only
    `CollisionOccluders` see `Physics`, so the rest of `Light` knows plain outlines.
12. *What moves gives the outline the view draws.* A collider stands at the last step and the view draws between steps. A
    shadow from it would lag behind its sprite, so the bridge is for what stands still.
13. *Rays, not a sorted sweep.* Rays go to every corner and every crossing of two edges, and just past each. A sweep is faster
    but fails on edges that touch, cross or line up with the light, and those are ordinary here: a tile map's boxes touch, and
    colliders overlap. A test compares the result with a sight line to thousands of points; it found the crossings, which
    the first version missed. The inside can change without the API.
14. *The outline stops at a box past the radius.* Light has faded to zero there, so the outline needs no arcs, and a long wall
    is cut at the box to mark where it leaves.
15. *Near side lit by default.* Only the edges facing away from the light stop it, so a shape is lit and its shadow starts
    behind it, which reads right from above. Touching tiles light only the first. A light inside a closed shape ignores it, or
    a lantern would be trapped in its carrier and a torch in its wall.
16. *Geometry in `Utilities`.* `VisibilityPolygon` knows no light, so a guard's view can use it. It is view code: a simulation
    does not link it, and rules ask `CollisionWorld::Raycast`.
17. *Hard edges for now.* Soft shadows would cost a fan per sample. The half-resolution image softens the edges a little; up
    close the steps of the image show. Left to a game that asks.

**Found on the way:** a new `Surface`'s texture stayed bound from its creation while the first pass drew into it, a feedback
loop that GL leaves undefined. An AMD driver then showed the next sampling an image without that drawing, for one frame,
when the surface was as large as its target. `GlDevice::BeginPass` now unbinds a target's attachments.

**Checking:** the `NullDevice` sees vertices and commands, not shaders or GL state, and both traps above lived there.
`EngineGpuPixelCheck` renders with the real device in a hidden window and reads pixels back. It needs a desktop and a GPU, so
it is not a CTest.

### D39. Spatial and layered audio
Status: implemented (2026-11-05). Usage: [Audio](../Audio.md#sounds-in-the-world).

**Decision:** sound is miniaudio's engine, with a stop of the engine's own on every voice, bus and the master. Sounds in the
world, sounds a view keeps changing and ambience layers all start from rows of the game's sound tables. raylib's audio
module is gone from the build.

**Why:** raylib's `Sound` cannot loop, its `Music` refills on the main thread from a buffer of about 33 to 66 ms, so a long
frame or a seek through steps leaves a hole, nothing glides, and a low-pass for one sound cannot be done cleanly: its
stream processor callback takes no user data. `raudio` is itself a thin layer over miniaudio, built with miniaudio's
engine, node graph and resource manager turned off, so going to miniaudio removes a layer rather than trading libraries,
and nothing raudio does is lost but three tracker formats.

**Decided:**
1. *miniaudio, not our own mixer or another library (the user's).* Measured on 0.11.26 before choosing: a loop seam as clean as
   the sine, a streamed loop through 1.5 s of a sleeping main thread with no silent block, 128 voices with pitch and pan at
   38 ms of CPU per second of audio. SoLoud's last release found was 2020; OpenAL Soft is LGPL; FMOD and Wwise cannot ship
   in an MIT engine. Public domain or MIT-0, one file, vendored unmodified, so it keeps working if left alone.
2. *One shaper on every stop.* `AudioShaper` gives a voice, a bus or the master a gain per ear, a volume with fades and a
   low-pass. The main thread states where things should go; the audio thread moves them there sample by sample. It knows no
   library, so its math is tested on plain samples. miniaudio's own pan is not used: it does not glide (a jump of 0.5 per
   change, measured) and its default law is not constant power.
3. *The file keeps its own rate.* Converted at load, a file not at the device's rate begins with a few zero frames that come
   back at every loop (measured on 44.1 kHz files). Kept at its rate, the voice resamples as it plays and the seam is clean.
4. *Constant power everywhere.* Pan gives each ear `√2·cos` and `√2·sin`, so a sound moving across keeps its loudness, and
   fades cross on a sine so that two layers trading places do not dip. The master keeps 1/√2 of headroom, which puts a
   sound in the middle at 0.707: raylib's mix gave 0.6875 there, so games sound as they did.
5. *A state variable filter for muffling.* The usual biquad clicked at 7% of full scale when its cutoff moved under a sine;
   the topology-preserving form (Simper) stays under 0.6%. The cutoff moves from 20 kHz to 300 Hz on a logarithmic scale,
   and the filter is out of the path, bit for bit, at 0.
6. *A view's sound, never a simulation's.* `LiveSound` and `SoundLayers` follow what a view reads from the state each frame;
   `check` keeps `Audio/` out of simulations. Fades run in real seconds on the audio thread, so a hitstop, slow motion or a
   catch-up does not stretch or break them, and the state hash cannot change.
7. *A handle owns its sound.* Dropping a `LiveSound` fades it out: a forgotten loop cannot play forever, and a row with
   `.loop` cannot go through `Play`. The handle holds a weak link to the player, so it may outlive it, and a voice's ID is
   never given out twice, so a stale handle does nothing.
8. *The listener is plain data.* A camera feeds it, so does a character; `Audio` includes nothing but Utilities, Animation
   and itself (`check`). Several listeners serve split screen, and the one who hears a sound best decides.
9. *Distance with a row's `SoundFalloff`.* Full, then a `Curve`, then silent: the same curves as everything else, and a sound
   past `silentBeyond` costs nothing to hear. miniaudio's 3D spatializer is not used; its inverse law never reaches silence.
10. *Three buses, set on purpose.* World, Ui and Music, each with a level and muffling under the volume options. The world's
    live sounds pause with the game's time; menus and music go on. More buses wait for a game that asks.
11. *Loops and layers are rows.* Their files reach `GameAssetManifest`, which `release` uses, so nothing started late falls out
    of the package (D37). A test holds it.
12. *Voices are kept and reused.* A voice stopped part-way is rebuilt before its next start, because miniaudio's seek keeps
    the frames it has read ahead (about 1.3 ms of the old place played first, measured).
13. *Offline output and capture.* `AudioOutput::Offline` runs the real mix with no device for tests; `StartCapture` and
    `ctl audio capture` write what the device plays, so an agent judges by numbers what it cannot hear.

**Left out, and why:** HRTF and real 3D, reverb, music sequencing and a mixing editor: outside this record's scope.
miniaudio has a reverb and a delay among its nodes, so reverb would be a node on a bus. A limiter: no game has clipped yet.
Doppler and occlusion from colliders: a game can set muffle from its own ray, as the recipe's indoors flag does.

### D40. Light polish: flicker, short flashes, soft shadows
Status: implemented (2026-11-05). Usage: [Lighting](../Graphics/Lighting.md).

**Decision:** three additions to D38. `Flicker` computes a lamp's strength from the time; `FxLightFlash` is a cue whose light
reaches a `LightFrame` as data; `LightLayer` blurs its image when a shadow falls. Light stays view code: no hash or replay
changes.

**Flicker:**
1. *A function in `Animation`, with `flashes` a required argument.* It keeps nothing, so `LightSource` stays plain data.
   `Animation` cannot see `FxQuality`, so something has to carry the choice. A check left to each game could be forgotten, and
   then someone who turned flashes off keeps flickering where no test looks. Going through `FxStage` would apply it by
   itself, but a candle should not need a stage. A required argument cannot be forgotten and needs nothing; the price is one
   argument per call, as with the two clocks of D30.
2. *Without flashes, slow rather than still.* Nothing cuts out, and the wander slows to one change a second. A candle's sway is
   what makes it a candle and does not flash. A tube's wander is so shallow that it then burns steadily. With flashes on, cuts
   are not rationed: the player has allowed them, and `ScreenGlitch` keeps its fast pattern behind the same setting (D30).

**Short flashes:**
3. *The game connects them in one line: `.flashes = _fx.LightFlashes()`.* `Light` and `Fx` do not include each other, as
   `LightSource` is in `Paint`, which both see. `_fx.WithFlashes(frame)` would make `Fx` depend on `Light` and keep a merged list
   inside it. `Draw(paint, frame, view, _fx)` would make `Light` depend on `Fx`, and a game with no stage would meet it too.
   Forgetting the line shows no flash, which is visible and risks nothing.
4. *No radius means the whole scene, by raising the ambient.* The same meaning as `FxShockwave::reach`. Lightning lights what
   is behind a wall, so no occluder stops it. A flash with a radius is an ordinary light, so its shadows come free.
5. *Clocks and scale as for `FxFlash`.* Game time, so a hitstop holds a hit's parts together. `intensity` scales the strength and
   not the time.
6. *One limit, shared.* `ScreenFlash`'s rule became `FlashLimit`, and `FlashLights` uses the same one, or the two taking turns
   would make six starts a second. A start of each kind since the last `Update` counts as one, since a cue with both is one event.
   A consequence that was accepted: a double strike loses its second half, and of ten explosions in a second, three give light.
7. *Turning flashes off also ends one showing.* The player has just asked for this.

**Soft shadows:** the steps are the image's pixels, drawn without anti-aliasing and doubled by half resolution, so they stay
two screen pixels wide at every zoom.

| Way | Good | Bad |
| --- | --- | --- |
| Blur the image (chosen) | Every edge, including where light stops on an object; cost does not grow with lights; no special cases | Same spread near and far; a thin wall leaks that much |
| Several samples per light | Spreads with distance, as in life | Cost times samples per light; steps between samples; each sample's share of 8 bits loses the dark range, where a dark game lives |
| Fans of a soft wedge at each silhouette | No extra pass, spreads with distance | Leaks through the next wall unless each wedge is cast again; leaves steps where light stops on an object |
| Distance field marched per pixel | The best look | A new occluder image and a heavy shader for each light, for a look few 2D games need |
| Higher resolution | Nothing to write | Steps remain, and it costs four times the pixels |

8. *The chosen blur stands on public `Painter`.* `BlurredImage` is a new paint kind in the existing shader, so `LightLayer` keeps
   to `Painter` and `Surface`, and any game can use it.
9. *In world units, scaled by the zoom.* A close-up is softer on the screen and the same in the world. At least one image unit,
   or the steps return when zoomed out; at most 24.
10. *A wide spread passes twice.* Nine samples spread over many pixels show as bands, so past about eight device pixels a
    narrower pass goes over them. A pixel check compares neighbouring rows.
11. *No cost without a shadow.* The passes run only in a frame where a light was actually cut, and the second image gets no
    memory before the first such frame. Measured with `EngineGpuPixelCheck` on one desktop GPU, with six shadowed lights at
    1280 x 720 and half resolution: about 0.04 ms a frame with hard edges and 0.07 ms with soft ones. Other GPUs are not measured.

**Left out:** normal maps, brightening past a pixel's colour, a quality screen, and colliders stopping light by themselves (D38).
A penumbra that grows with distance can come later behind the same option.

### D41. Analog screen finish
Status: implemented (2026-11-05). Usage: [Analog finish](../Graphics/Painter.md#analog-finish).

**Decision:** film grain, colour fade, tint, wobble and tape are fields of `PostSettings`, computed in the existing compose
pass. `Mix(PostSettings, PostSettings, t)` moves between two finishes, so a single number can drive them all.

**Why:** the look covers the whole image and needs only what compose already reads, so one more pass or shader per effect
would add cost and code for nothing. Games get values and no shader access, as before.

**Decided:**
1. *Fields, not a system.* There is no "fear" object. A game writes `paint.Post() = Mix(calm, disturbed, danger)`. The number
   comes from the game; the engine does not know what it means.
2. *`Mix` is how it joins Animation.* `MixValue` already looks for a `Mix(a, b, t)` beside a type, so a `SecondsTween<float>`
   driving the number, a `SecondsTween<PostSettings>`, clips and timelines all work, and Animation stays unaware of Graphics.
   `t` is clamped, because an overshooting curve would make strengths negative.
3. *An effect that is off has no say.* Mixing from a calm finish without a tint would otherwise pull the colour from
   white, and likewise size and speed. Strengths move linearly; the rest takes the side that is on, and blends only when both
   are on. A scanline count of 0 means "automatic", which has no number to blend, so it changes halfway.
4. *Rhythms are added up, in `PostChain`.* `sin(time * speed)` jumps when `speed` changes: at second 1000, a change of 0.01
   moves the phase 10 radians, and a mix changes it every frame. So wobble and tape keep a phase, advanced by elapsed time
   times speed. It is view state; the price is that rewinding in the editor does not restore it, so a second can look
   different the second time. Grain is a pure function of the clock.
5. *Noise is an integer hash in the shader* (pcg3d), seeded by position and a frame number at film's 24 per second. It shows
   no pattern, and needs neither `<random>` nor a simulation's `Rng`. The frame number comes from a `double` clock and wraps
   at 2^24, the whole numbers a float holds exactly: 8 days. The clock used to be cast to `float` before `PostChain`.
6. *The player's switch is `RenderQuality::screenMotion`.* Not a game setting, which a game can forget, and not
   `FxQuality`, which belongs to `Fx` and would tie it to flashes. It stops what moves (grain, wobble, tape). What holds still
   stays, so the mood survives. `Active` honours it: with nothing else asked, compose does not run.
7. *Readable by construction.* No field has a non-zero default. Wobble fades to nothing over the outer 22% of the image, so
   a HUD along the edges stays still and clamped edges do not smear. Grain is weakest on bright pixels. Most tape lines barely
   move; a few jump the whole way. Drawing a HUD above the finish was not added: nothing has asked for it yet.
8. *Order in compose:* wobble and line shifts choose where to read, then colour with aberration, bloom, fade, tint, the band's
   dashes, scanlines, vignette, grain. Grain comes last because it belongs to the film, not to the scene; bloom reads at the
   bent position so a glow follows its source.
9. *Not `ScreenGlitch`.* It stays a timed burst drawn on the canvas by `Fx`. The tape is a lasting value.

**Measured (2026-11-05, RX 9070 XT, 1920 x 1080, GPU timer, three runs of 180 frames):** compose with only a vignette, 22 to
25 µs. Fade and tint add nothing measurable. Grain stays within the timer's noise of about 3 µs. Wobble adds 5 to 6 µs, tape
3 to 10 µs, and everything together with aberration 8 to 15 µs, about 0.1% of a 60 Hz frame. A weak integrated GPU will
cost many times this, not yet measured. With nothing asked, compose does not run. `EngineGpuPixelCheck --compose-cost`
repeats the measurement on any machine.

**Out of scope:** shaders written by a game, CRT curvature, palette reduction, a separate low-resolution mode.

### D42. Atmosphere and weather
Status: implemented (2026-11-05). Usage: [Atmosphere](../Graphics/Atmosphere.md).

**Decision:** `Weather` is a plain pose in `Core/Weather`. `Graphics/Atmosphere` shows it, from fields that repeat across the
world and fog computed over the view. Lightning is a cue. There is no weather system.

**Why:** a scene with nothing on it comes alive with little code, and the particle system could not do it. It emits at a point,
each particle carries its own copy of its look and a `std::function`, and a camera that moves finds the edge of what was
emitted.

**Decided:**
1. *One concept in two places, not two modules.* "What is the weather" is data and "what does it look like" draws, as with
   `Camera2D` and `CameraView`. `Weather` draws nothing, so a simulation may keep it and a rule may read it; `check` holds it to
   `Utilities` and `Animation`. If it sat beside the drawing, a rule asking "is it raining" could not compile, or would read a view
   and break replay.
2. *No weather class.* A change is `SecondsTween<Weather>`, a sequence is a clip, and blending comes from `PoseFields`. A
   forecast has no second user yet. Nothing here stops one being added on top of the same value.
3. *One field for everything that drifts.* Rain, snow, dust, ash and leaves differ only in data, so `DriftField` takes a look. A
   game's own kind is the done test: the recipe's ash uses only the public API.
4. *A repeating tile, not spawning.* Pieces live in a tile of the world that repeats without end. No view finds an edge, however
   the camera moves, zooms or turns, so nothing has to be born at a border, and a split screen shares one field because `Draw` is
   `const`. The tile is larger than a usual view, so its repeat does not show.
5. *The amount is a count.* The first `amount x capacity` pieces show, in an order the seed made random, and the last fades in by
   degrees. A tween then adds rain drop by drop rather than fading a full sheet of it.
6. *Fog is a mesh, not a texture or a shader.* Value noise at a coarse grid over the view, blended by `Painter::Mesh`. It has no
   seam or repeat, no `Surface` to redraw, and a side view's band or a camera's turn costs one line, since each point is mapped
   to the world. `Glow` puffs would vanish with `RenderQuality::glow`, the trap of D38, and a shader would be a new paint kind for
   what a mesh already does. If it ever looks too soft, the API stays and the inside can change. Octaves are turned against each
   other: the first version showed the noise grid as stripes.
7. *Behind and in front.* Fog under a character's feet is much of the effect, and one `Draw` could not give it. Two calls leave
   the order with the game, as a `LightLayer` does.
8. *`parallax` instead of "moves with the camera".* A layer stuck to the screen makes the world slide under the rain. `1`
   stands in the world, `0` stays on the screen, and above `1` is nearer. `CameraView::Parallaxed` holds this, for later depth work.
9. *Above and from the side, from one set of classes.* `TopDownAtmosphere` and `SideViewAtmosphere` are data. Ripples appear on a
   floor seen from above; a side view's splashes need to know where the ground is, and are left until a game asks.
10. *Lightning is a cue.* A flash through `FlashLights` (D40), a bolt and a shake already are what a cue is for, and they follow
    the `flashes` choice. `FxBolt` joins `FlashLimit` as a third kind. The `Atmosphere` does not know lightning, and when to
    strike is the game's. `Fx` and `Graphics/Atmosphere` do not include each other.
11. *Air is view code.* Its own `Rng`, its own clock, and never in `State`.
12. *`backgroundEffects` is the player's switch.* Half the pieces, a grid twice as coarse. Weather stays on: it can say something
    the game means. A view wider than a tile thins out, so that the cost has a ceiling.

**Measured (2026-11-05, Debug build, 1280 x 720, CPU time in the air's draw calls):** a storm with both fogs
about 1.8 ms; mist about 1.0 ms; snow 0.2 ms; a storm zoomed out to 0.5 and turned about 7 ms before the ceiling was added. A
release build and the GPU's share were not measured. One `Painter::Line` per drop is the first thing to look at if it is too much.

**Found on the way:** a tween of a pose could not reach `State`: `StateVisitor::Field` took only plain values, so `SecondsTween<Weather>` or `<CameraPose>` failed to compile at its `State` call. `Field` now takes a pose, as a group of its fields.

**Checking:** `NullDevice` tests cover amounts, parallax, wrapping, the band and the flash rules. Looks were checked by
screenshots from a work scene: rain, snow, mist, side view, night with lamps, zoom and turn, split screen, lightning. Motion has
not been judged by a person.

**Out of scope:** splashes on a side view's ground, snow that settles, wet floors, wind gusts, fog that lamps scatter in, a
forecast.

### D43. Several agents in one editor
Status: implemented (2026-11-05); the Agents panel is the next step. Usage: [Several agents](../Editor/Automation.md#several-agents).

**Decision:** every control request names its Console session, and one session at a time may change or capture the
editor (`ControlLease`). The pipe answers 8 clients side by side. The Console's sessions live in `ConsoleSessions`, apart
from the panel that draws them, so a second panel can show them.

**Why:** the editor has one scene, one playhead and one Audio Preview. With two agents, one agent's screenshot could show
the scene the other had just opened, and nothing failed. The pipe took one client at a time, so another agent's status
or hook note waited, then failed, while a build ran. Activity named no session, so the strip could mix one agent's
state with another's last action.

**Decided:**
1. *A hold, not a queue.* A refused agent hears it at once and does other work. A queue would keep it waiting and paying
   for nothing. The hold lasts while the holder's command is answered and 60 s after its last one, about one agent turn.
2. *Only what changes or captures needs it.* Looking (`status`, `inspect`, `logs`) and notes never wait.
3. *A lost hold is said once.* When the developer used the editor, or another session took it while this one was idle,
   the next command is refused once and asks the agent to look first. This catches the silent case, a screenshot of state
   someone else made, without a version number on every answer.
4. *The developer always wins.* The UI actions that change the scene, its parameters, playback, tuning or sounds end the
   hold. The editor never locks the developer out.
5. *Identity is the Console session.* `ENGINE_EDITOR_SESSION` already reached every Console program; now every request
   sends it. Clients outside the Console share one identity: the CLIs give no id a tool could read reliably. Parallel
   agents belong in Console sessions.
6. *A session that closes or exits frees its hold.* The host tells the service which sessions live every frame.
7. *Concurrency stays small.* Each connection has one request at a time, and one command that needs the hold runs at a
   time. A wait for the worker accepts a later client's sync, since the worker applies messages in order.
8. *Sessions by id.* `ConsoleSessions` owns the terminals and is addressed by a stable id. `ConsolePanel` keeps only the
   views and the list width. Events (`EditorControlEvent::session`, `kind`) and hook tool lines
   (`EditorAgentReport::sessions[...].tools`, 100 per session) are kept per session for the Agents panel.

**Checking:** unit tests cover the hold (idle, release, developer, takeover, ended session) and two pipe clients at once.
An open editor was driven by two session ids. The second one's `open` was refused, naming the first, while its `status`
and `note` answered. It answered within 0.2 s during the first one's build, and `release` handed over at once.

**Out of scope:** an editor rendering without a window for private checks (proposed; would let agents in worktrees verify
looks), separate identities outside the Console, CLI session ids, token use, resume.

### D44. The Agents drawer
Status: implemented (2026-11-05), awaiting the developer's look. Usage: [Agent workspace](../Editor/Usage.md#agent-workspace).

**Decision:** agents are worked with in a compact drawer that hangs under the toolbar's agent strip, not in a dock panel.
It has a chip per agent session and the chosen one's live terminal or activity. The strip gains a dot per session.

**Why:** agents are one of the editor's main things, not one panel among equals. A dock panel competed with the Console
for room, and every action in it sent the developer to the Console, where the conversation is. A dropdown from the strip
would be prominent, but it closes on the first click outside, and a terminal you type into cannot live in one. A first
drawer covered half the workspace; the developer wanted about a quarter.

**Decided:**
1. *A drawer from the strip.* About a quarter of the screen at the workspace's top right, above the panels, so the layout
   never changes (the Git drawer's way, D25). Its default is capped at 45 % × 65 % of the workspace so a large UI scale
   does not make it huge. It stays open until closed; Esc closes it only when its terminal does not have the keyboard,
   since agent CLIs use Esc. Its bottom-left corner resizes it.
2. *The terminal is in it.* The conversation is the agent work, so the drawer draws the chosen session's terminal and
   does not reinvent a prompt box. `ConsoleViews` holds one view per session for both places, and a terminal is drawn in
   one place per frame: the Console shows "open in Agents" with *Show here* for it.
3. *Chips, not a list; tabs, not columns.* At this size a list and an activity column would leave the terminal no room.
   Chips keep the Console's order, since states change every few seconds and a sorted chip would move from under the
   pointer; a waiting one glows instead. *Terminal | Activity* share the body.
4. *Agent requests come here.* Opening an agent, Send scene, marks and build failures show the agent session in the
   drawer.
   Shells and the Console's own add button stay in the Console.
5. *The strip speaks of the waiting session first,* then of the agent target, and opens the drawer on it. With several
   sessions it shows a dot each (a ring on the holder), and a narrow strip keeps only the dots.
6. *One chip for every agent outside the Console.* It has no terminal, so it shows its activity.
7. *Keys that work while you type to the agent.* Ctrl+Shift+A opens and closes it (as Ctrl+Shift+G does the Git
   drawer), Ctrl+PageDown/PageUp and Ctrl+1…9 choose an agent, Ctrl+Shift+T switches the tab, Ctrl+Shift+S sends the
   scene, Ctrl+Shift+N starts one. The terminal reserves them. Alt+letter belongs to terminal programs (word moves),
   and `[` `]` need AltGr on some layouts, so neither is used. Esc stays the agent's: CLIs stop on it, so it closes the
   drawer only when the terminal does not have the keys. Ctrl+Shift+A no longer starts an agent by itself; with none,
   the drawer opens on its empty state and Enter starts the usual one.

**Replaced:** the dock panel an earlier pass built (`AgentsPanel`, `EditorPanel::Agents`), and the strip's popup list.

**Checking:** ImGui tests cover the order, a choice that leaves the Console's selection alone, the one-terminal rule, the
outside chip and the empty state. The smoke run draws the drawer where the workspace puts it, with a real terminal, at
1600×900 and 1280×800 at 150 %, and the strip with six sessions. The developer has not yet looked at it in the editor.

**Out of scope:** remembering the drawer's size between runs, reordering chips, per-agent notifications.

### D45. Polygon geometry: earcut and Clipper2
Status: implemented (2026-11-06), awaiting the developer's look. Usage: [Painter](../Graphics/Painter.md#holes-and-fill-rules).

**Decision:** Paint triangulates with earcut.hpp 3.2.4 and resolves areas with Clipper2 2.0.1, both vendored under
`ThirdParty/` and private to `EngineHeadless`. Our ear clipping and the per-corner halo are gone.

**Why:** the old fill moved a polygon's corners half a pixel inwards for its anti-aliased edge and then triangulated
the moved outline. A part thinner than that turned inside out, ear clipping failed, and its fan drew a wedge far from
the shape; backgrounds had to keep every polygon thick. `FillPath` filled each contour on its own, so a path could not
have a hole. Glows were miter offsets clamped at sharp corners, and `StrokeAlign` was silently ignored for polygons
and paths.

**Decided:**
1. *earcut triangulates, Clipper2 decides areas.* Clipper2 2.0 has a triangulation of its own, but it is marked beta and
   refuses intersecting paths. earcut 3.x (not 2.2.4 as first surveyed) is newer and faster.
2. *The triangles come from the real outline.* The fill is triangulated on the points as given; only the coloured
   corners then move half a fringe inwards. Normal shapes draw exactly as before (the edge stays on the outline); a
   sliver folds over within a pixel instead of breaking the triangulation. The other option, fill to the outline with
   an outward-only fringe, would have grown every polygon by half a pixel.
3. *`FillRule` lives on `Path`, `NonZero` by default,* as in SVG, Canvas and Skia. Overlapping contours keep looking as
   they did; `EvenOdd` cuts holes by nesting. The rule travels with the shape, so `FillPath` keeps its signature.
4. *A filled path's border and glow follow the resolved area,* so overlapping contours get one outline. `StrokePath`
   still draws contours as built.
5. *`Polygon` stays the fast path:* it fills its points directly and allocates nothing once warm. Clipper2 runs only
   for paths, glows and inside or outside borders. Its integer grid is 1/256 of a device pixel.
6. *Glow = Clipper2's round offset minus the shape,* coloured by distance from the edge. No miter spikes; narrow
   notches inside the glow's width fill with near-full glow, which reads as glow.
7. *`StrokeAlign::Inside` / `Outside` work for polygons and paths:* the band between the area and its offset, filled.
   A part thinner than an inside border is covered whole.
8. *No public shape operations yet.* `Union` / `Difference` / `Intersect` / `Offset` over contours wait for a game or
   background that needs them (root `SYSTEMS.md` TODO). They would need a target simulations can link, built with
   `/fp:precise`.

**Removed:** `Triangulate`, `InsideTriangle`, `PolygonHalo`. The snow caps in `Mountains` keep their minimum
thickness: without it a small view shows them as thin broken lines (compared on thumbnails); only its reason changed.

**Checking:** paint geometry tests cover the snow-cap sliver (it fails on the old tessellator), holes under both rules,
an island in a hole, a self-intersecting star, overlapping contours, the glow band's round corners and fade, and inside
and outside bands. `EngineGpuPixelCheck` reads a polygon's edge pixels, the sliver, a ring's hole and aligned borders.
The developer has not yet looked at backgrounds (snow caps), glowing polygons and path drawings in the editor.

**Out of scope:** the public shape operations above, Clipper2 in simulations, gradient borders.

## v1'de kalan işler

[v1 finalization](v1finalization.md).
