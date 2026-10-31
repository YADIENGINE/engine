# ToolUi: araçların ortak arayüz katmanı

`ToolUi/` (CMake `EngineToolUi`) engine'in araçlarının (editör, launcher) ortak ImGui katmanıdır. Oyunlara girmez;
oyun arayüzü `Core/Graphics/Ui`'dır. Include'lar engine köküne göredir (`#include "ToolUi/ToolTheme.h"`). Hedef
`ENGINE_BUILD_RUNTIME` açık Windows build'lerinde tanımlıdır ve yalnız link'leyen bir araç varsa derlenir.

| Dosya | İçerik |
| --- | --- |
| `ToolUiContext` | ImGui bağlamı (rlImGui), fontlar, tema, ölçek. `ToolUiOptions { docking, keyboardNavigation }`. Pencereden (Game) sonra kurulur, ondan önce yıkılır; her karede `BeginFrame(ölçekYüzdesi)` / `EndFrame()` |
| `ToolTheme` | Arcade Cabinet teması (`ApplyToolTheme`), merkezi `ToolPalette` renkleri / `ToolMetrics` ölçüleri, font rolleri (`Ui / Medium / Heading / Technical`), fonttan bağımsız ölçek (`ToolUiScale()`), işlev renkleri (`ToolToneColor`), panel yüzeyleri (`ToolSurfaceColor`) |
| `ToolWidgets` | `BeginToolPanel` / `EndToolPanel`, `ToolTooltip`, `ToolSectionHeader`, `ToolEmptyState`, `ToolStatusBadge`, `ToolButton`, `ToolSegment`, `ToolIconButton`, `ContinueToolToolbar`, `ToolSingleLineButton`, `ToolWrappedSelectable` |
| `ToolWindow` | `ToolGameSettings(uygulamaKimliği, WindowSettings)`: ayar dosyası, canvas içeriği, ses ve oyun fontu olmayan araç penceresi |

- **Fontlar:** Windows'un kurulu Segoe UI Regular / Semibold fontları; font dosyaları paketlenmez. Bulunamazsa
  gömülü Noto Sans Regular / Medium / SemiBold kullanılır. Her ağırlığa Font Awesome ikonları birleşir
  (`ICON_FA_*`, `<extras/IconsFontAwesome6.h>`). Teknik değerler Consolas (sistem fontu yoksa gömülü ImGui fontu).
  Noto OFL lisansı `ToolFontLicense()` ile erişilir; yedek fontlar ve lisans CMake tarafından exe içine gömülür.
- **Tipografi:** uygulama kimliği 16, gövde / düğme / bölüm 14, ana başlık 18, meta bilgi 12 mantıksal piksel.
  Başlık ve düğme Segoe UI Semibold, gövde Regular. `PushFont(ToolFont(rol), ToolMetrics::...Size)` kullanılır.
- **Çizim:** ImGui 1.92 dinamik atlası metni istenen son boyutta rasterize eder; sabit 14 px bitmap büyütülmez.
  Yatay 2× oversampling, kesirli advance, ImGui'nin piksele oturan glif kökenleri ve atlas yüklenince bilinear
  filtre kullanılır. UI canvas/render texture üzerinden yeniden ölçeklenmez; doğrudan pencereye çizilir.
  Noto'nun hhea yüksekliği 1362/1000 em, Segoe UI'ın 2724/2048 em'dir; `ExtraSizeScale` ile em ölçüsü normalleştirilir;
  bu düzeltme kontrol/yerleşim ölçeğini büyütmez. Aksi halde 14 px görünen ayar yaklaşık 10 px em üretir.
- **Ölçek:** kullanıcı yüzdesi × monitör DPI hem fontlara hem taban stile bir kez uygulanır. %100 / 96 DPI'da
  gövde 14 px, kontrol 30 px'dir; gizli çarpan yoktur. `ToolUiScale()` etkin başlık/font boyutundan etkilenmez.
- **Renkler:** Arcade Cabinet; pencere `#1E1A22`, panel `#2C2633`, girintili içerik `#16131B`, kontroller
  `#3D3547`, seçili yüzey `#493B50`, amber vurgu `#F4AF65`. Vurgu üzerindeki metin `#302013`'tür.
  Metin / ikincil / sönük / pasif metin, hover / basılı ve sınır / ince ayırıcı renkleri `ToolPalette`'tedir.
  Pasif düğme metni `#746C7E`'dir; yüzeyin pasiflik saydamlığı ayrıca korunur. Başarı `#89C9A4`,
  uyarı vurgu rengi, hata `#DB818D`, bilgi ikincil metin rengidir. Geometri ve fontlar paletten bağımsızdır.
  `ToolSurface` rolleri: `Header`, `Toolbar`, `Workspace`, `Content`, `Utility`, `Status`, `PanelHeader`. Ana bölgeler kendi
  opak yüzeyi ve sınırıyla ayrılır; sıradan yerleşim alt bölgeleri saydam kalır. `BeginToolPanel` çocuk bölgesine
  yüzey, 1 px sınır ve ölçekli iç boşluk uygular; kırpılmış olsa bile `EndToolPanel` ile kapatılır. İsteğe bağlı
  `padding` mantıksal px'dir (varsayılan 12); yoğun araç çubukları 8 kullanabilir. Köşe yarıçapı 3 px'dir.
  %100 altındaki ölçeklerde panel, pencere, popup ve kontrol sınırları en az 1 fiziksel px olarak korunur.
  Araç kendi durum renklerini `ToolTone`'dan türetir (editör: `EditorPhaseColor`).
- **Pencere:** araç `Game`'den türer, `ToolGameSettings` ile kurulur ve bütün pencereyi `DrawWindow` içinde
  `ToolUiContext` ile ImGui olarak çizer.
- Yeni ortak widget yalnız birden fazla aracın işine yarıyorsa buraya girer; bir araca özgü olan o aracın
  klasöründe kalır (editörün parametre / Inspector widget'ları `Editor/Ui/Widgets`).
