# İlk editör önizlemesi

Önce [proje kurulumunu](../ProjectSetup.md) tamamlayın; [ilk oyun](../GettingStarted.md) gerekmez.
Editör Windows masaüstünde çalışır. Projenin `Editor/` klasörü Engine ve Game klasörlerinin kardeşidir;
include'larda `Editor/Api/...` Engine public başlıklarını ifade eder. Kurulumun açtığı `Editor/` klasörü boştur;
`python Engine editor` o hâlde Engine'in starter worker'ıyla açılır (Engine → Starter önizlemesi).

## CMake hedefi

Projenin `CMakeLists.txt`'i `APP_BUILD_EDITOR` açıkken (`python Engine editor` bunu kendisi verir) `Editor/` altındaki her
`.cpp`'yi bulur ve worker'ı kurar:

```cmake
engine_add_editor_worker(${PROJECT_NAME}Preview SOURCES ${APP_EDITOR_SOURCES})
target_include_directories(${PROJECT_NAME}Preview PRIVATE Editor)
```

Yardımcı worker exe'sini EngineEditor'ün yanına (`<build>/Editor/<Config>/`) kurar, `EnginePreviewRuntime`'ı
link'ler ve aynı klasöre `EngineEditor.project.json` tarifini üretir: worker'ın yolu ve Build & Refresh'in
çalıştıracağı `cmake --build` komutu. Argümanlar: `NAME` (katalog başlığı; varsayılan `<proje> previews`),
`SOURCES` (boşsa starter worker), `LIBRARIES`, `ASSETS` (worker yanına kopyalanacak klasör). Bir build klasöründe
tek worker olur.

Önizlemeler oyunun View'larını çizecekse oyun kodunu ayrı bir kütüphaneye (`add_library(... Game/...)`) alın ve hem
oyuna hem worker'a `LIBRARIES` ile verin; worker oyunun `CreateGame.cpp`'sini derlemez.

EngineEditor kendi `main`'ine sahiptir; worker da `RunPreviewHost` çağıran kendi `main`'ini kullanır.
Worker'ı `engine_add_game` ile kurmayın, çünkü o yardımcı oyunun giriş noktasını ekler.

## Editor/FirstPreview.h

```cpp
#pragma once
#include "Editor/Api/EditorPreview.h"
#include "Editor/Api/PreviewRegistry.h"

class FirstPreview final : public EditorPreview {
public:
    explicit FirstPreview(const PreviewParameters& parameters);
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const PreviewValue& value) override;

private:
    void Draw() const override;
    float _radius;
};

[[nodiscard]] PreviewDefinition FirstPreviewDefinition();
```

## Editor/FirstPreview.cpp

```cpp
#include "FirstPreview.h"
#include <raylib.h>
#include <cmath>
#include <memory>
#include <variant>
#include "Graphics/Draw/DrawTime.h"

FirstPreview::FirstPreview(const PreviewParameters& parameters)
    : _radius(static_cast<float>(std::get<double>(parameters.at("radius")))) {}

std::string FirstPreview::ApplyParameter(const std::string& id, const PreviewValue& value) {
    if (id != "radius") return "Unknown parameter";
    _radius = static_cast<float>(std::get<double>(value));
    return {};
}

void FirstPreview::Draw() const {
    const float phase = static_cast<float>(DrawTime());
    DrawCircleV({640.0f, 360.0f}, _radius, Fade(ORANGE, 0.6f + 0.4f * std::sin(phase)));
}

PreviewDefinition FirstPreviewDefinition() {
    PreviewDefinition definition;
    definition.id = "myproject.first";
    definition.name = "First preview";
    definition.category = "Drawing";
    definition.parameters = {{"radius", "Radius", PreviewParameterKind::Float, 80.0, 8.0, 200.0}};
    definition.create = [](const PreviewServices&, const PreviewParameters& parameters) {
        return std::make_unique<FirstPreview>(parameters);
    };
    return definition;
}
```

Örneğin sabit merkez ve renkleri yerine gerçek oyunun View'ını çağırabilirsiniz.
Metadata kaydı yalnız fabrika tanımlar; çizim/asset yükleme yapmaz.

## Editor/Main.cpp

```cpp
#include "FirstPreview.h"
#include "Editor/Preview/PreviewHost.h"

int main(int argc, char** argv) {
    GameSettings settings;
    settings.applicationId = "MyProjectPreview";
    settings.canvasWidth = 1280;
    settings.canvasHeight = 720;
    settings.persistentOptions = false;
    return RunPreviewHost(
        argc, argv, [](PreviewRegistry& registry) { registry.Add(FirstPreviewDefinition()); }, settings);
}
```

`RunPreviewHost` varsayılan `PreviewGame` ile tek Game döngüsü kurar.
Pencere gizli/sabit olur; ölçü canvas'tan gelir, worker 30 FPS hedefler.
Canvas pozitif ve en fazla 1920×1080 olmalıdır.
Örnekler gerçek font/ses kullanıyorsa GameSettings manifest/katalog sağlayın ve
asset klasörünü `ASSETS` ile worker exe yanına kopyalayın. Bu kopyalama oyun hedefinden bağımsızdır.

## Geliştiricinin çalıştırması

```powershell
python Engine editor
```

`python Engine editor` `Project.json` "editor" bölümündeki ayrı build klasörünü (şablonda `build/editor`) yapılandırır,
`EngineEditor` ve worker'ı derler, editörü `EngineEditor.project.json` ile açar.

MyProject → Drawing → First preview açılır; Radius değişir; Pause parlama saatini dondurur;
Step tek adım ilerletir; Reset saati sıfırlar.

Worker'ı doğrudan başlatmayın: session/IPC argümanlarını editör sağlar.
Eksik exe için önce iki hedefi derleyin; Build & Refresh hatası Output'ta görünür ve worker açılmaz.
Üretim sahneleri için özel Game adaptörü gerekirse [CreatingPreviews](CreatingPreviews.md)
ve [API](Api.md) üzerindeki PreviewSession sözleşmesini izleyin.
