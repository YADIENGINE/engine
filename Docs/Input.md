# Input and navigation

## Public API ve sahiplik

Input, Engine runtime altyapısıdır. Desteklenen public header'lar:

| Header | Sözleşme |
| --- | --- |
| `Input/Input.hpp` | `DeviceInput`, `InputKey`, `InputTouch`; raylib'siz snapshot |
| `Input/InputNavigation.hpp` | Snapshot'ı referansla okuyan `InputNavigation` |
| `Input/DragPointer.hpp` | Serbest sürükleme ve bırakışta tap |
| `Input/CollectionPointer.hpp` | Koleksiyon tap, hover ve satır kaydırma |

Oyun ve projeye ait editör kodu yalnız bu header'ları kullanır. `Input/Internals/` desteklenen API değildir.
Cihaz okuması, pencere koordinatları, normalizasyon ve ortak temas takipçisi burada kalır.
Gesture sınıfları kendi durumlarını sahiplenir; kopyalanamazlar. Snapshot sade bir değer tipidir.
Headless implementasyon `EngineHeadless`'dadır; yalnız raylib cihaz okuması `Engine` hedefine girer.

`Game::ReadInput()` her host frame'inde cihazları bir kez okur, runtime'ın sağladığı canvas dönüşümüyle
snapshot üretir. Geçiş sırasında da örnekleme sürer. Engine editörü aynı normalizasyonu viewport dönüşümü
ve kendi odak/sahiplik kurallarıyla kullanır. Dış host'lar `ReadInput()` override'ından `DeviceInput` sağlayabilir.
Public konumların tamamı mantıksal canvas koordinatıdır; canvas dışındaki touch'ın `id`'si korunur,
`position` değeri boş olur. Sahne cihazları veya raylib input'unu yeniden okumaz.

## Fiziksel snapshot ve anlamsal sorgular

`DeviceInput` key pressed/down durumlarını, fareyi, tekerleği ve kimlikli touch örneklerini taşır.
`ShiftDown()` tuşlardan hesaplanır. `mousePosition` mevcut konumdur; `mousePress` uygun fare basışıdır;
`mouseHover` yalnız fare hareketinde doludur ve sabit fare klavye seçimini geri çalmaz.
`pressPoints` yeni başlayan temasları, `heldPoints` halen basılı temasları taşır. Touch varken ve onu izleyen
sentetik fare olayları boşalana kadar bu birleşik listelere fare eklenmez; `mousePress`/`mouseHover` bastırılır.
Canvas dışında başlayan parmağın içeri girmesi yeni basış oluşturmaz.

`InputNavigation(input)` snapshot'ı kopyalamayan anlamsal adaptördür. Snapshot adaptörden uzun yaşamalıdır;
callback'ler arasında adaptörü saklamayın. Geçici snapshot'tan adaptör oluşturmak derleme hatasıdır.
`Device()` alttaki snapshot'ı const referansla verir; `PressPoints()`/`HeldPoints()` ödünç span döndürür.

| Sorgu | Varsayılan |
| --- | --- |
| `Confirm()` | Enter / Space basışı |
| `Back()` | Escape |
| `Horizontal()`, `Vertical()` | WASD / ok basışı, -1 / 0 / 1 |
| `TabStep()` | Q önceki; E / Tab sonraki |
| `Pressed(Direction)` | O yöne ait WASD / ok basışı |
| `PressPoint()` | İlk uygun temas başlangıcı; bırakışta tap değildir |
| `AnyPressed()`, `AnyHeld()` | Herhangi bir tuş basışı / basılı tuş veya temas |
| `HoverPoint()`, `MousePosition()`, `Wheel()` | Fare hareketi / mevcut konum / tekerlek |

Sürekli hareket için `Down(InputKey)`, bir kez tetiklenen eylem için `Pressed(InputKey)`
kullanın. Oyuncu başına bağlar, rebind, onay sesleri ve oyun eylemleri oyunda tanımlanır.
Scene `HandleInput` override'ı snapshot alır; sahneler raylib'i tekrar okumaz.

`DeviceInput::cancelled` normal bir bırakma değildir: bekleyen temas/eylem iptal edilir.
Runtime bu snapshot'ı Scene `InputCancelled()` hook'una yönlendirir. Hook içinde pointer yardımcısına
boş bir `DeviceInput{.cancelled = true}` verin ve yerel basış/sürükleme görünüşünü temizleyin.
`Game::CancelInput()` sahneyi bildirir ve runtime'ın yerel normalizasyonunu yeniden eşitler.
Touch kimlikleri örneklenmeye devam eder; tüm basılı girdiler bırakılana ve nötr frame tüketilene kadar
yalnız cancelled snapshot üretilir. Dış host aynı odak/iptal sözleşmesini sağlamalıdır.

## Koleksiyon ve sürükle-bırak

`CollectionPointer` and `HeaderGridNavigation` live in EngineHeadless. They contain no rendering or content policy.
Create a pointer helper with the grid bounds and row-drag threshold, then call `Update(DeviceInput)` once per input
frame. Its result contains a release `tap`, eligible `hover`, and signed `rows` (positive moves down the collection).
Touch movement emits rows while held. Crossing the movement threshold latches drag detection until release, including
when the contact returns to its origin. The first touch ID owns the gesture; later contacts cannot inherit it.
Touch also suppresses synthetic mouse events until all contacts and mouse buttons are quiet. Mouse taps activate on
release; wheel input applies only over the grid. Call `Cancel()` for a local gesture cancellation, then continue supplying
input frames on return so the cancelled contact can drain. Keyboard bindings, audio and confirmation remain in Game.

`HeaderGridNavigation(count, {headerRowSize, ...})` owns `GridMenuSelection` plus focus across header rows, grid,
empty-state action and footer. `Move(dx, dy, columns)` reports a focus/selection change. During filtering, use
`SetItemCount` followed by `SelectIndex` to retain header focus; preserve stable content identity in the caller.
`Focus` and `HeaderIndex` support hover/click adapters without coupling Engine to category enums. `ResetHeaders`
returns every header cursor to its first item without moving focus. Every header row must have at least one item
(asserted).



`Input/DragPointer.hpp` serbest sürükleme için `DragPointer(threshold)` sağlar.
`Update(input)` sonucunda `pressed`, `position`, `origin`, `released`, `hover`,
`dragging`, `tap` ve `cancelled` bulunur. Pozisyonlar canvas koordinatındadır.
Eşik aşıldıktan sonra başlangıca dönmek hareketi tap'e dönüştürmez; birincil touch kimliği
başka parmaklara devredilmez. Touch kaynaklı sentetik mouse olayları bastırılır.
Yerel iptalde `Cancel()` çağırın ve iptal edilen temasın boşalması için sonraki
snapshot'ları vermeye devam edin. `released` tek başına tap anlamına gelmez.
Canvas dışında başlayan veya dışına çıkan temas iptal edilir; son içerideki konumda tap/drop üretilmez.
`cancelled` sonucunda tüketici bekleyen basışını ve sürükleme görünüşünü temizlemelidir.

İki gesture aynı private temas takipçisini kendi instance'larında kullanır: birincil kimlik devredilmez,
canvas dışına çıkış iptal olur ve sentetik fare bastırılır. `CollectionPointerResult` de `cancelled` bildirir.
Eşik ve scroll politikası ilgili gesture'a aittir. `InputCancelled()` hook'undaki boş cancelled snapshot
yerel teması temizleyip iptal sınırını aynı çağrıda tüketir; sonraki yeni basış kaybolmaz.

## Editör aktarımı

Editör protokolü v4 snapshot'ın tuşlarını, fare alanlarını, birleşik temas listelerini ve kimlikli touch'larını
taşır; boş touch pozisyonu canvas dışı kimliği korur. Tekrarlı touch kimlikleri ve kapasite aşımı reddedilir.
Worker biriktirmesinde basış/bırakış bayrakları korunur, `pressPoints` birikir, down/held/touch durumu en son
snapshot'tan alınır. İptal eski eylemleri siler ve yeni eylemlerden önce ayrı bir sınır olarak teslim edilir.
Bu aktarım bir snapshot birleştirmesidir; bütün fiziksel olayların sıralı günlüğü değildir.
Protokol veya `InputKey` sırası değiştiğinde sürüm yükseltilir, editör ve worker birlikte yeniden derlenir.
Backend tuş tablosu kimlikli satırlar ve `RowsInIdOrder` ile enum'a karşı denetlenir.

## Menü seçimi ve viewport

`Graphics/Draw/GridViewport.h` complements grid selection with reusable scrolling and tile geometry.
Each View owns a value member initialized with `GridViewportSettings`: canvas `bounds`, `columns`,
`visibleRows`, `tileWidth`/`tileHeight`, independent `gapX`/`gapY`, and `scrollSpeed`. Bounds, dimensions,
columns and visible rows must be positive; gaps and finite speed must be nonnegative. Zero speed snaps
immediately. The component draws nothing and does not manage selection, focus, item IDs or input.

```cpp
GridViewport viewport({.bounds = gridBounds, .columns = 4, .visibleRows = 3,
                       .tileWidth = 120, .tileHeight = 100, .gapX = 12, .gapY = 16,
                       .scrollSpeed = 10.0f});
viewport.Reset(itemCount, selectedIndex); // scene entry or a content/filter reset
viewport.Update(itemCount, selectedIndex, deltaSeconds);
const Rect tile = viewport.TileArea(index);
const auto hit = viewport.TileAt(pointer, itemCount);
```

`Reset` bottom-aligns the selected row where possible. `Update` retains scroll while selection is
visible and otherwise interpolates toward its visible range, using `min(1, dt * scrollSpeed)`.
Invalid update deltas skip animation; count shrinkage still clamps scroll and an empty count resets
it to zero. Negative counts are empty and selection is clamped for calculations without changing
the caller's selection. `TileArea` rounds animated vertical positions with `std::lround`; negative
indices return an empty rectangle. `TileAt` rejects points outside bounds, gaps and missing items.
It uses the supplied current count, even when input changed content before the next View update.
`RowCount(count)` and `ScrollRows()` allow Game to draw its own scrollbar and entrance animations.
Keep drawing order, expanded scissor margins, colors, and filter/category reset decisions in the View.

`Menus/GridMenuSelection.h` owns a grid's selected index and `Grid`/`Footer` focus. Construct it with
an item count and call `Move(dx, dy, columns)`. Horizontal movement follows the linear item order;
vertical movement clamps incomplete rows. `GridMenuMove::ExitAbove` lets the game focus controls
above the grid. Down from the last row focuses the footer; up restores the selected item.
`SetItemCount` clamps selection when results change. An empty grid selects `-1` and only allows
footer focus. Invalid column counts produce no movement. Item IDs, filtering, ordering, and additional
focus regions belong to the game.



`MenuSelection` ve `MenuInput` tek boyutlu menüler için daha küçük seçeneklerdir.
Input seçimi değiştirir; View scroll/hover/animasyonu sahiplenir.
Hit test için çizimde kullandığınız aynı viewport geometrisini kullanın.
