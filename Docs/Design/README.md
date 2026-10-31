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
| Engine kaynağının kaynağı | Launcher'ın yanındaki `Engines/<sürüm>/`; internet / GitHub gerekmez | Engine reposu (erişim gerekir) |
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
Durum: kabul edildi.

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
  sadece o sürümle yeni proje kurulamaz. Otomatik güncelleme v1 sonrası.
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
- **Düzen:** Velopack uygulamayı `<id>\current\`'e koyar ve güncellemede onu bütünüyle değiştirir; bu yüzden
  `Settings.json` `current/`'in dışında, Velopack kökündedir: `%LOCALAPPDATA%\<id>\Settings.json` (kurulu launcher'ın
  veri klasörü exe klasörünün üstü). `Engines/` ve `Tools/` `current/`'in içindedir (sürümle gelir).
- **Kaldırma (2026-10-31):** iz bırakmaz: Velopack kökü (ayarlar dahil) ve kısayollar gider, PATH satırı kaldırma
  hook'unda silinir. Projelere hiç dokunulmaz; yeniden kurulumda liste Projects Folder taramasıyla geri gelir, ilk
  açılış (Sistem kontrolü) yeniden çalışır. MSVC (VS Build Tools) kalır: sistem geneli kurulumdur, başka uygulamalar
  da kullanabilir; kaldırmak isteyen Visual Studio Installer'dan kaldırır (belgede yazılı). Velopack'in kaldırmada
  kökü bütünüyle sildiği deneme paketinde doğrulanır. **Neden:** temiz kaldırma; kaybolan liste tek taramayla
  kurulur. `%APPDATA%` (Roaming) iz bırakır ve etki alanı hesaplarında var olmayan proje yollarını taşır.
- **Sürüm (2026-10-31):** paketin sürümü launcher'ınkidir (`LAUNCHER_VERSION`, kendi semver'i; Velopack'in
  güncelleme karşılaştırması, exe adı `<Engine>-Setup-<sürüm>.exe`, Uygulamalar listesi). Yeni engine kopyası,
  launcher düzeltmesi ya da araç yükseltmesi onu artırır. Taşınan engine sürümleri Engines sekmesinde ve release
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
  (Segoe UI, Consolas, Font Awesome; `ToolFont`), ImGui bağlamı + DPI / ölçek (`ToolUiContext`) ve genel widget'lar
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
  Üst gezinme (ad, sekmeler, sistem durumu noktası, Settings), kalıcı araç çubuğu, girintili proje çalışma alanı,
  ayrı sağ yardımcı panel, alt durum çubuğu (launcher sürümü, proje sayısı, durum, "Developer mode"). Her bölge
  kendi opak yüzeyi ve sınırıyla ayrılır; ortak tema `ToolUi/`'dadır. Hepsi tek uygulama modelinde.
  - **Sekme** (`LauncherTab`, tam ekran, her an bir tane): **Projects**: araç çubuğu (Create, Import = var olanı
    ekle, Projects Folder = klasör seç / varsayılan proje konumu olarak kaydet / otomatik tara, filtre: ad ya da
    yol, sıralama: Last opened / Name / Path, `Settings.json`'da saklanır), tek seçimli yoğun liste
    (çift tıklama = Open), sağ proje denetçisi. Boş listede aynı araçlar ve yapı
    görünür; küçük boş durum mesajı listenin içindedir. **Engines**: launcher'ın taşıdığı sürümler (sürüm, tarih,
    notlar; kırıcılar öne çıkar, kullanan proje sayısı); developer modunda "dev (main)" satırı; salt okunur.
  - **Sağ proje denetçisi**: yalnız seçili projenin bilgileri ve eylemleri. Dar pencerede Project details açılır
    panelinden erişilir. Launcher sekmesi, arayüz ölçeği ayarı ve tarama satırı yoktur. Varsayılan proje klasörü
    yalnız Projects Folder düğmesinden belirlenir; PATH, kurulum klasörü, developer modu ve lisanslar Settings'tedir.
    Seçim yokken aynı denetçi geçerli klasörü, Projects Folder / Create / Import kısayollarını ve kısa keşif
    açıklamasını gösterir. Başlıklar ortak araç temasının tool-window şeritleridir; güncel renk paleti Arcade Cabinet'tir.
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

**Aktif engine launcher reposunda submodule (2026-10-31):**
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

**Güncelleme notlarının biçimi (2026-10-31):** `EngineManifest.json` `"notes"`, birikimli, en yeni sürüm en üstte:
```json
"notes": [
  { "version": "0.3.0", "date": "2026-11-20",
    "changes": ["Tuning: color type"],
    "breaking": [ { "change": "DrawUiChip takes a style", "migrate": "Pass CurrentTheme().ui.chip first" } ] }
]
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

## v1 iş sırası

Kabul edildi: [Plan](Plan.md) (Aşama 0 hazırlık → 1 editör döngüsü → 2 tuning → 3 launcher MVP → 4 dağıtım;
5 görsel asset'ler ayrı oturumda; 6 debug en son).

## Açık sorular

Aşama 3 (launcher çekirdeği) için, tek tek konuşulur; karar alınınca ilgili D'ye yazılır:

1. ~~Launcher'ın dili; CLI ile UI aynı exe mi~~ → D15 (C++20 çekirdek; `.exe` pencere + `.com` CLI).
2. ~~Launcher reposunun adı ve yeri~~ → D16 (`YADIENGINE/yadien`).
3. ~~Gömülü araçların launcher'a nasıl girdiği~~ → D8 (kilit dosyası + özetli indirme betiği).
4. ~~Kopyadaki manifest'in adı ve biçimi~~ → D4 (`EngineManifest.json` aynen + üretilen `EngineCopy.json`).
5. ~~Developer modunun nasıl açıldığı~~ → D4 (Ayarlar'da şifre; repoda sadece PBKDF2 özeti).
6. ~~Kullanıcı modundaki launcher'ın submodule'lü bir projeyi açınca ne yapacağı~~ → D4 (her tür açılır;
   engine işlemleri türe göre: kopya / submodule / kayıtsız).
7. ~~Engine `CLAUDE.md`'sinin kopyadan çıkarılması; developer modunda AI dosyası~~ → D14 (`export-ignore` +
   `git archive`; AI dosyası yok).
8. ~~Kırıcı değişikliğin tanımı~~ → D16 (elle değişiklik isteyen her şey; birikimli notlar + `migrate`).
9. ~~İlk yayın 1.0.0 mı 0.x mi~~ → D16 (0.x; 0.x'te ARA kırıcı; 1.0.0 v1 bitince).
10. ~~Var olan oyunun launcher'a geçişi~~ → [Plan](Plan.md) Aşama 3 (belgeler `<engine>` komutuna, launcher
    kurulup kabul testi geçince).
11. ~~Launcher'ın kendi komutları ile devredilenlerin ayrımı~~ → D6 (`<engine> launcher <iş>`; gerisi aynen devir).

Aşama 4 (dağıtım) için, tek tek konuşulur:

1. ~~Paketin biçimi: kurulumu kim yapar~~ → D15 (Velopack; geri dönüş Inno Setup 6.6). ~~İmza~~ → D15
   (açık kaynak + SignPath Foundation; o güne kadar imzasız).
2. ~~Paketin sürümü~~ → D15 (launcher'ın kendi sürümü).
3. ~~`Settings.json`'un yeri ve kaldırmanın sildikleri~~ → D15 (Velopack kökü; kaldırma iz bırakmaz, projeler ve
   MSVC kalır).
4. ~~Güncelleme sırasında çalışan süreçler; aynı / eski sürümün tekrar kurulması~~ → D15 (elle kurulumda Velopack
   kapatır, kabul; otomatik güncelleme yalnız boşta uygulanır).
5. ~~"Ses araçlarını kur"~~ → D7 (istendiğinde, özeti sabit wheel'ler, `current/` dışına).
6. ~~Yayın yeri ve CI~~ → D19 (hemen public; `YADIENGINE` organizasyonu; yayın elle, CI/CD ve SignPath ertelendi).
7. ~~Lisans~~ → D19 (iki repo da MIT, "YADIEN contributors").
8. ~~Public belgelerin dili~~ → D19 (İngilizce; önce kullanıcıya dönük belgeler).
9. ~~Developer modu~~ → D4 / D19 (şifre kalkar, uyarılı düz anahtar).
10. ~~Açılmadan önceki temizlik~~ → D19 (gizli commit adresi, geçmiş yeniden yazılmaz, oyun adları genelleşir).

Kapanan ilk liste:

Kapananlar:

1. ~~Exe modeli~~ → [D3](#d3-exe-modeli-launcher--projeden-derlenen-editör).
2. ~~Engine dağıtımı~~ → [D4](#d4-engine-dağıtımı-kullanıcı-modu-ve-developer-modu).
3. ~~Ajan API'si~~ → [D5](#d5-ajan-apisi-v1-sonrası-stratejik-öncelik).
4. ~~CLI~~ → [D6](#d6-cli-launcher-exe-aynı-zamanda-komut-satırı-aracıdır-editör-açıksa-derlemenin-sahibi-editördür).
5. ~~Python~~ → [D7](#d7-python-launcher-gömülü-python-taşır-araçlar-projenin-engineinden-çalışır).
6. ~~Toolchain ve bağımlılıklar~~ → [D8](#d8-derleyici-msvc-launcher-otomatik-kurar).
7. ~~Döngü hızı~~ → [D9](#d9-döngü-hızı-hot-reload-yok-yanına-derle-sonra-değiştir).
8. ~~Kod asset'leri~~ → [D10](#d10-görsel-assetler-katalog-galeri-ve-pişirme).
9. ~~Uzamsal tasarım~~ → [D11](#d11-uzamsal-değerler-v1de-sadece-sayı).
10. ~~Debug~~ → [D12](#d12-hata-ayıklama-dış-debuggera-kolay-bağlanma--metin-çökme-raporu).

## Mevcut altyapı

Plan sıfırdan başlamıyor:
- Proje kurma: `EngineSetup.py` + `Tools/ProjectTemplate/` ([ProjectSetup](../ProjectSetup.md)).
- Editör: ayrı süreçte worker, IPC, Build & Refresh, parametre / inspection API'si
  ([Editor architecture](../Editor/Architecture.md)).
- Build / test: `Tools/build_cli.py`, `test_runner.py` (JSON sonuç), `project_config.py` (`Project.json`).
