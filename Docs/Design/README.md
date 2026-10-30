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

**Açık ayrıntılar:** developer modunun nasıl açıldığı (erişim kontrolü); kullanıcı modundaki launcher'ın
submodule'lü bir projeyi açınca ne yapacağı; manifest'in adı ve biçimi.

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

### D7. Python: launcher gömülü Python taşır, araçlar projenin engine'inden çalışır
**Karar:** Launcher resmi gömülebilir (embeddable) Python'u yanında taşır; kullanıcı Python kurmaz. Derleme,
test, release ve editör komutları launcher'ın içinde yeniden yazılmaz: launcher **projenin**
`Engine/Tools/` giriş noktalarını bu Python ile çalıştırır. Launcher ile proje arasındaki sözleşme:
`Project.json`'u oku, `Engine/Tools/`'taki giriş noktasını çağır.
- Engine araçları sadece standart kütüphane kullanır (bugün de böyle); bu kural korunur.
- AudioGeneration (numpy / scipy) varsayılan Python'a gelmez; isteyen geliştirici için launcher "ses araçlarını
  kur" seçeneği sunar. Oyunun derlenmesi / çalışması bunu gerektirmez.

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

**Açık ayrıntılar:** engine reposunun kendi `CLAUDE.md`'si engine geliştirme kurallarıdır (engine'de commit,
push ...); kullanıcı modundaki kopyaya girerse oradaki AI'ı yanıltır. Kopyadan çıkarılmalı. Developer modunda
şablonun AI dosyası üretip üretmeyeceği. Durum: kabul edildi.

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
- **Developer modu kapısı:** engine reposuna GitHub erişimi. Submodule için bu izin zaten şarttır; ayrı şifre /
  anahtar sistemi yok.

**Karar (dağıtım ve kurulum):**
- **Tek exe dağıtılır** (`<Engine>-<sürüm>.exe`, ~100–150 MB): launcher + gömülü paket (Python, CMake, Ninja,
  `Engines/`).
- **İlk çalıştırma kurulumdur:** paketi `%LOCALAPPDATA%\<Engine>\`'e çıkarır (yönetici izni istemez), masaüstüne
  kısayol koyar (exe kopyası değil: tek exe, güncellemede ayrışmaz), isteğe bağlı `<engine>`'yi PATH'e ekler (varsayılan
  açık), Windows "Uygulamalar" listesine kaydolur (kaldırma oradan), sonra launcher'ı ve Sistem kontrolünü açar.
- **Kurulum klasörü:** `<engine>.exe`, `Tools/` (python, cmake, ninja), `Engines/`, `Settings.json` (proje listesi, mod,
  tercihler). Ayarlar ekranında "Kurulum klasörünü aç".
- **Projeler** varsayılan `Belgeler\<Engine> Projects\` altına; yeni proje ekranında değiştirilebilir.
- **Güncelleme:** yeni sürümün exe'si var olan kurulumun üzerine kurar; proje listesi ve ayarlar korunur, `Engines/`
  yeni sürümün desteklediklerle değişir. Desteği düşen sürümü kullanan projeler etkilenmez (kendi kopyaları var);
  sadece o sürümle yeni proje kurulamaz. Otomatik güncelleme v1 sonrası.
- Kod imzalama yok; SmartScreen uyarısı kabul edilir.

Durum: kabul edildi.

### D16. Sürümleme ve launcher reposu
**Karar:**
- **Numara `ANA.ARA.YAMA`:** YAMA hata düzeltmesi, ARA geriye uyumlu ekleme, ANA kırıcı değişiklik. Launcher'daki
  klasörler `Engine<ANA.ARA>` adını taşır ve o serinin son yamasını içerir.
- **Ayrı repolar:** engine ve launcher ayrı repolarda, ayrı geliştirilir. Bir engine sürümü stabil sayıldığında
  launcher reposuna kopyalanır (`Engines/Engine<ANA.ARA>/`); launcher reposunda 2–3 sürüm durur. Engine launcher
  reposunda geliştirilmez, kopyalar orada düzenlenmez. Exe'ler istendiğinde release olarak yayınlanır.
- **Güncelleme notları engine manifest'indedir** (sürümün yanında); launcher güncellemeden önce gösterir. Ayrıntılı
  tasarım sonra.
- Developer modundaki projeler sürüm takip etmez, engine reposunun dalını takip eder.

**Ertelenen:** sözleşmenin (kırıcı sayılan değişikliğin) tanımı; ilk yayının 1.0.0 mı 0.x mi olacağı.

**Öneriler (uygulamada):**
- **İçe aktarma betiği:** launcher reposunda tek bir betik engine reposundaki bir tag'i kopyalar: tag ve temiz
  çalışma ağacı denetimi, engine geliştirme dosyalarının ayıklanması (`.git`, engine `CLAUDE.md` ...; D14),
  manifest'in yazılması (sürüm, kaynak commit, dosya özetleri, notlar). Kopyanın kaynağı her zaman izlenebilir.
- **Yama akışı:** eski bir seride hata düzeltmesi engine reposunda o sürümün tag'inden açılan bir dalda yapılır,
  yeni yama tag'lenir ve tekrar içe aktarılır.
- **Atlanan sürümlerin notları:** 1.1'den 1.3'e geçen kullanıcı 1.2'nin notlarını da görmeli; manifest notları
  sürüm geçmişini birikimli taşır.
- **Launcher'ın kendi derlemesi:** launcher UI'ı editörün ImGui katmanını kullandığı için (D15) kendini repodaki en
  yeni engine kopyasına karşı derler; ayrı bir engine bağımlılığı gerekmez.
- **Repo boyutu:** git aynı içerikli dosyaları bir kez saklar; sürümler arasında değişmeyen `ThirdParty/` ve
  dosyalar yer kaplamaz, sadece değişen dosyalar büyütür.

Durum: kabul edildi.

## v1 iş sırası

Kabul edildi: [Plan](Plan.md) (Aşama 0 hazırlık → 1 editör döngüsü → 2 tuning → 3 launcher MVP → 4 dağıtım;
5 görsel asset'ler ayrı oturumda; 6 debug en son).

## Açık sorular

Şu an açık soru yok. Kapanan ilk liste:

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
