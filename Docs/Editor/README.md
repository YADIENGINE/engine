# Engine Editor

Windows geliştirme aracı, gerçek üretim çizimini ayrı bir worker sürecinde önizler.
Genel altyapı `Engine/Editor/` içindedir; oyun kataloğu ve worker adaptörü tüketici projededir.
Normal oyun ve headless hedefler editör/ImGui bağımlılığı almaz.

1. [Projeye bağlamak ve ilk önizleme](GettingStarted.md).
2. [Arayüz ve playback kullanımı](Usage.md).
3. [Önizleme tasarlamak](CreatingPreviews.md).
4. [Public API ve proje dosyası](Api.md).
5. [Mimari](Architecture.md) ve [kontroller](Testing.md).
6. [Profiler: canlı ölçüm, benchmark ve önce/sonra karşılaştırma](Profiler.md).
7. [Editör UI'sının sonraki yetenekleri ve ön koşulları](TODO.md).

`ENGINE_BUILD_EDITOR=ON` ve `ENGINE_BUILD_RUNTIME=ON` Windows editör hedeflerini açar;
varsayılanı kapalıdır. `ENGINE_BUILD_EDITOR_TESTS` bağımsız headless altyapı testlerini açar.
Build & Refresh yalnız proje tarifindeki worker hedefini derler. Engine Editor/API değişikliklerinde
editör ve worker birlikte yeniden derlenmelidir.

Mevcut sınırlar [geliştirme notlarında](Development/Roadmap.md), gerekçeler
[tarihçede](Development/History.md) bulunur.
