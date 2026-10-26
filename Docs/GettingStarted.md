# İlk oyun

Projeyi önce [Proje kurulumu](ProjectSetup.md)'ndaki gibi açın: Engine `Engine/` submodule'ü, kökte
`Engine/Templates/NewProject/` şablonunun kopyası. Bu rehber şablondaki küçük oyunu dosya dosya anlatır;
kod şablonda durur, burada tekrar yazılmaz.

## Dosya yapısı

```text
MyProject/
  CMakeLists.txt  CMakePresets.json  Project.json  p.py  editor.py
  Engine/                   (submodule)
  Game/
    Assets/
    CreateGame.cpp
    MyGame.h  MyGame.cpp
    MyScene.h  MyScene.cpp
  Editor/                   (önizlemeler; bkz. Editor/GettingStarted.md)
```

## CMakeLists.txt

Şablonun `CMakeLists.txt`'i üç seçenekle (`MYPROJECT_BUILD_GAME`, `_EDITOR`, `_TESTS`) Engine'in
`ENGINE_BUILD_*` seçeneklerini `add_subdirectory(Engine)`'dan önce ayarlar, sonra oyunu kurar:

```cmake
engine_add_game(MyProject SOURCES Game/CreateGame.cpp Game/MyGame.cpp Game/MyScene.cpp
    ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Game/Assets")
target_include_directories(MyProject PRIVATE Game)
```

`engine_add_game` Engine'in `Main.cpp` dosyasını ve Engine link'ini ekler.
Desteklenen argümanlar: `SOURCES`, ek hedefler için `LIBRARIES` (ör. `EngineLocalization`), masaüstünde exe
yanına kopyalanacak klasör için `ASSETS`. Kendi `main` fonksiyonunuzu eklemeyin.
Android'de aynı yardımcı executable yerine shared library oluşturur; Gradle paketlemesi projeye aittir.
Engine'i iki kez `add_subdirectory` yapmayın.

## İlk sahne: Game/MyScene.h, Game/MyScene.cpp

`MyScene` oyun saatiyle (`TimeDomain::Game`) çalışan bir `Scene`'dir: `Enter` başlangıç konumunu kurar,
`HandleInput` yönü okur, `Update` daireyi hareket ettirir, `Draw` çizer. Destructor `Shutdown()` çağırır.

`InputFrame(input).Horizontal()` bu frame basılan A/D veya ok tuşunu bildirir; tuş tekrarı olmayan tek basış
hareketi gösterir. Basılı tutarak hareket için `DeviceInput::Down` sorgularını kullanın.
Bu küçük örnek lifecycle'ı göstermek için state ve çizimi aynı sahnede tutar.
Büyüyen oyunda saf model ve onu const okuyan View ayırın; görsel sabitleri View'da tutun.

## Oyun ve giriş fabrikası: Game/MyGame.h, Game/MyGame.cpp, Game/CreateGame.cpp

`MyGame` `Game`'den türer; `GameSettings` (uygulama kimliği, pencere başlığı, canvas boyu) ctor'da verilir.
Sahne oyunun değer üyesidir: ctor'da `RegisterScene`, `Start`'ta `SetScene`. `HandleInput` Escape/Back ile
`RequestQuit` çağırır, gerisini `Game::HandleInput`'a bırakır. `CreateGame.cpp` Engine'in giriş noktasının
çağırdığı `CreateGame()` fabrikasını tanımlar (`Runtime/EntryPoint.h`).

`Shutdown()` somut sahne destructor'ında çağrılır; çocuk nesneler eklediğinizde de aynı sahiplik kuralı geçerlidir.

## Geliştiricinin derleyip denemesi

```powershell
python p.py
```

`p.py` MSVC ortamını kurar, `desktop` preset'iyle yapılandırır, Debug derler ve `build/Debug/MyProject.exe`'yi
açar. Elle: Developer PowerShell'de `cmake --preset desktop` ve `cmake --build --preset debug`.

Beklenen: canvas ortasında turuncu daire; A/D veya sol/sağ basışı hareket ettirir,
Escape kapatır. Şablon bu belge güncellemesinde derlenmemiştir.

Özel font/ses eklerken [asset rehberini](AssetsAndPlatforms.md), kalıcı veri eklerken
[ayar ve kayıt rehberini](OptionsAndSaves.md) izleyin. Editör entegrasyonu
[ayrı başlangıç rehberindedir](Editor/GettingStarted.md).
