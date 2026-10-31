# Graphics

Yeni çizim [Painter](Painter.md) ile yapılır. Aşağıdaki diğer rehberler raylib tabanlı eski yolu anlatır; o kod
geçiş bitene kadar Painter'ın köprüsünden çalışır.

Bütün çizimler oyunun seçtiği mantıksal canvas koordinatlarındadır; pencere boyutu canvas boyutu değildir.
Çizim yalnız canlı renderer frame'i içinde yapılır. Oyunun sahne/View'ları gerçek üretim çizimini sahiplenir.

| Konu | Rehber |
| --- | --- |
| **Çizim API'si: şekil, metin, yüzey, kapsam, post-process** | [Painter](Painter.md) |
| Eski (raylib) canvas, render texture, kamera ve clipping | [Rendering](Rendering.md) |
| Font, ölçüm ve text layout | [Text](Text.md) |
| Tema, widget ve arka plan | [Themes and UI](ThemesAndUi.md) |
| Particles, kamera, overlay ve sunum saati | [Effects](Effects.md) |

Alt katmanlar stil ve zamanı parametre alır; temalı widget'lar `Graphics/Ui/` içindedir.
İkonlar `Graphics/Icons/Icons.h` içindeki ortak, şekil isimli `Icon` ve `DrawIcon` API'sini kullanır.
İkonun oyundaki anlamını model yerine View eşler. Yeni şekil `size` ölçüsüne oranlı çizilir.
