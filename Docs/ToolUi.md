# ToolUi: araçların ortak arayüz katmanı

`ToolUi/` (CMake `EngineToolUi`) engine'in araçlarının (editör, launcher) ortak ImGui katmanıdır. Oyunlara girmez;
oyun arayüzü `Core/Graphics/Ui`'dır. Include'lar engine köküne göredir (`#include "ToolUi/ToolTheme.h"`). Hedef
`ENGINE_BUILD_RUNTIME` açık Windows build'lerinde tanımlıdır ve yalnız link'leyen bir araç varsa derlenir.

| Dosya | İçerik |
| --- | --- |
| `ToolUiContext` | ImGui bağlamı (rlImGui), fontlar, tema, ölçek. `ToolUiOptions { docking, keyboardNavigation }`. Pencereden (Game) sonra kurulur, ondan önce yıkılır; her karede `BeginFrame(ölçekYüzdesi)` / `EndFrame()` |
| `ToolTheme` | Koyu tema (`ApplyToolTheme`), font rolleri (`ToolFont(ToolFontRole::Ui / Technical)`), ölçek (`ToolUiScale()`), işlev renkleri (`ToolToneColor(ToolTone::Accent / Success / Warning / Error / Information)`) |
| `ToolWidgets` | `ToolTooltip`, `ToolSectionHeader`, `ToolEmptyState`, `ToolStatusBadge`, `ToolSegment`, `ToolIconButton`, `ContinueToolToolbar`, `ToolSingleLineButton`, `ToolWrappedSelectable` |
| `ToolWindow` | `ToolGameSettings(uygulamaKimliği, WindowSettings)`: ayar dosyası, canvas içeriği, ses ve oyun fontu olmayan araç penceresi |

- **Fontlar:** arayüzde Windows Segoe UI + Font Awesome (ikonlar `ICON_FA_*`, `<extras/IconsFontAwesome6.h>`),
  teknik değerlerde Consolas; sistem fontu yoksa gömülü font.
- **Ölçek:** kullanıcı yüzdesi × varsayılan ölçek (1.25) × DPI hem fontlara hem taban stile uygulanır; %100 varsayılan.
- **Renkler:** lacivert alt tonlu koyu yüzeyler, turkuaz vurgu; başarı yeşil, uyarı amber, hata mercan, bilgi mavi.
  Araç kendi durum renklerini `ToolTone`'dan türetir (editör: `EditorPhaseColor`).
- **Pencere:** araç `Game`'den türer, `ToolGameSettings` ile kurulur ve bütün pencereyi `DrawWindow` içinde
  `ToolUiContext` ile ImGui olarak çizer.
- Yeni ortak widget yalnız birden fazla aracın işine yarıyorsa buraya girer; bir araca özgü olan o aracın
  klasöründe kalır (editörün parametre / Inspector widget'ları `Editor/Ui/Widgets`).
