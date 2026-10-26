# İlk editör önizlemesi

Önce [proje kurulumunu](../ProjectSetup.md) ve [ilk oyunu](../GettingStarted.md) tamamlayın.
Editör Windows masaüstünde çalışır. Projenin `Editor/` klasörü Engine ve Game klasörlerinin kardeşidir;
include'larda `Editor/Api/...` Engine public başlıklarını ifade eder. Örnek kod
`Engine/Templates/NewProject/Editor/` altındadır.

## CMake hedefi

`ENGINE_BUILD_EDITOR` `add_subdirectory(Engine)` satırından önce açılır (şablonda `MYPROJECT_BUILD_EDITOR`).
Worker `engine_add_editor_worker` ile kurulur:

```cmake
if(MYPROJECT_BUILD_EDITOR)
    engine_add_editor_worker(MyProjectPreview NAME "MyProject previews"
        SOURCES Editor/Main.cpp Editor/FirstPreview.cpp)
    target_include_directories(MyProjectPreview PRIVATE Editor)
endif()
```

Yardımcı worker exe'sini EngineEditor'ün yanına (`<build>/Editor/<Config>/`) kurar, `EnginePreviewRuntime`'ı
link'ler ve aynı klasöre `EngineEditor.project.json` tarifini üretir: worker'ın yolu ve Build & Refresh'in
çalıştıracağı `cmake --build` komutu. Argümanlar: `NAME` (katalog başlığı; varsayılan `<proje> previews`),
`SOURCES`, `LIBRARIES` (ör. oyunun headless/View kütüphaneleri), `ASSETS` (worker yanına kopyalanacak klasör).
Bir build klasöründe tek worker olur.

EngineEditor kendi `main`'ine sahiptir; worker da `RunPreviewHost` çağıran kendi `main`'ini kullanır.
Worker'ı `engine_add_game` ile kurmayın, çünkü o yardımcı oyunun giriş noktasını ekler.

## Editor/FirstPreview.h, Editor/FirstPreview.cpp

`FirstPreview` bir `EditorPreview`'dır: ctor parametre haritasından yarıçapı okur, `ApplyParameter` değişeni
uygular (bilinmeyen id için hata metni döner), `Draw` sunum saatiyle (`DrawTime()`) parlayan bir daire çizer.
`FirstPreviewDefinition()` kimlik, ad, kategori, parametreler (`radius`: Float, 80, 8–200) ve fabrikayı verir.

Örneğin sabit merkez ve renkleri yerine gerçek oyunun View'ını çağırabilirsiniz.
Metadata kaydı yalnız fabrika tanımlar; çizim/asset yükleme yapmaz.

## Editor/Main.cpp

`RunPreviewHost(argc, argv, kayıt, settings)` varsayılan `PreviewGame` ile tek Game döngüsü kurar;
kayıt callback'i `registry.Add(FirstPreviewDefinition())` yapar. `settings.persistentOptions = false`
önizlemenin oyuncu ayarlarını okuyup yazmasını engeller.
Pencere gizli/sabit olur; ölçü canvas'tan gelir, worker 30 FPS hedefler.
Canvas pozitif ve en fazla 1920×1080 olmalıdır.
Örnekler gerçek font/ses kullanıyorsa GameSettings manifest/katalog sağlayın ve
asset klasörünü `ASSETS` ile worker exe yanına kopyalayın. Bu kopyalama oyun hedefinden bağımsızdır.

## Geliştiricinin çalıştırması

```powershell
python editor.py
```

`editor.py` `Project.json` "editor" bölümündeki ayrı build klasörünü yapılandırır, `EngineEditor` ve worker'ı
derler, editörü `EngineEditor.project.json` ile açar. Elle karşılığı:

```powershell
cmake -S . -B build/editor -G "Ninja Multi-Config" -DMYPROJECT_BUILD_EDITOR=ON -DMYPROJECT_BUILD_GAME=OFF
cmake --build build/editor --config Debug --target EngineEditor MyProjectPreview
.\build\editor\Editor\Debug\EngineEditor.exe --project .\build\editor\Editor\Debug\EngineEditor.project.json
```

MyProject → Drawing → First preview açılır; Radius değişir; Pause parlama saatini dondurur;
Step tek adım ilerletir; Reset saati sıfırlar. Şablon bu belge değişikliğinde derlenmemiştir.

Worker'ı doğrudan başlatmayın: session/IPC argümanlarını editör sağlar.
Eksik exe için önce iki hedefi derleyin; Build & Refresh hatası Output'ta görünür ve worker açılmaz.
Üretim sahneleri için özel Game adaptörü gerekirse [CreatingPreviews](CreatingPreviews.md)
ve [API](Api.md) üzerindeki PreviewSession sözleşmesini izleyin.
