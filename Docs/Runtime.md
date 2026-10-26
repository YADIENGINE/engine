# Runtime and lifecycle

## Lifecycle

`GameObject` supplies overridable `Start`, `Update(float)`, `Draw() const`, and `Stop` hooks.
`ObjectList` calls `Start` once before an enabled object's first update/draw, skips disabled objects,
and calls `Stop` once on removal or shutdown. A stopped object is finished; create a new object to
replace it. Zero time starts new objects but skips updates, while drawing remains available.

An `ObjectList` can register existing value members by reference or own dynamic objects passed as
`std::unique_ptr<GameObject>`. Each object belongs to one list. Additions take effect on the next start/update dispatch. Removal skips
further callbacks immediately; stopping and destruction happen at the next dispatch or shutdown.
Callbacks may add/remove objects, but must not recursively dispatch or shut down their own list.

`Scene` is a `GameObject` with its own object list, input handler, and `Enter`/`Exit` hooks. Register
value members with `AddObject`. The scene's `Start` runs once; `Enter`/`Exit` run on every visit.
Children update before the scene's update and draw before its draw, allowing presentation to advance
before the scene emits new simulation events. Scenes choose `TimeDomain::Real` or `TimeDomain::Game`.
Disabled scenes skip input/update/draw for the entire scene. `SetScene` queues a change; activation
happens between callbacks, before the next scene update.

Registered references must outlive their list. A concrete scene's destructor must call `Shutdown()`
while its value members and virtual hooks are still alive (see the scene in [GettingStarted](GettingStarted.md)).
Shutdown is idempotent. `Game::Run` also shuts down registered scenes before returning or rethrowing.
Scenes are value members of the derived game; base engine services outlive those members.

The frame order is device sampling → game input → audio/game real-time update → scene change → real-time
scene update → scaled-time scene update → scene draw → game overlay draw → completed canvas → window presentation.
`ReadInput()` defaults to normal device/canvas sampling and can supply external `DeviceInput` snapshots.
`InputReader::Read()` samples window coordinates; its mapper overload and `Read(renderer)` share the same sampling.
`CanvasReady(const Texture2D&)` receives a borrowed completed canvas before presentation; retain no texture ownership.
`DrawWindow()` draws after canvas presentation, inside the window drawing pass. `Renderer::Frame::Complete` invokes
these callbacks explicitly; scoped-frame cleanup never invokes user callbacks. Neither hook changes scene ownership.
`SetPaused` freezes game time; `Time()` handles hitstop and slow motion. Real-time screens and game
overlays keep advancing. Android focus loss pauses music, calls `Suspend`, processes activity events
without simulation, and discards the resumed frame's elapsed time before calling `Resume`.


## Başlangıç, yükleme ve hata sınırı

`Game` kurucusu renderer/asset/font servislerini kurar, temayı etkinleştirir ve ses kataloğunu
arka planda yüklemeye başlar. Kalıcı seçenekler açıksa `Start()` öncesinde yüklenir ve uygulanır.
`Start()` ilk sahneyi seçer; sesin hazır olduğunu varsayarak burada playback çağrısı yapmayın.
Yükleme ekranı ilk sahneyi simülasyon/input ilerletmeden çizer; `DrawLoading(seconds, outro)`
ve `LoadingOutroSeconds(loadSeconds)` ile sunum özelleştirilir. Normal sahne akışı yükleme sonrasında başlar.
Varsayılan giriş noktası başlangıç istisnalarını raporlar; içerik hook'ları kendi hata politikasını belirler.

## Input ve zaman seçimi

`Game::HandleInput` varsayılan olarak aktif sahneye yönlendirir. Override ediyorsanız
sahne girdisini korumak için uygun noktada `Game::HandleInput(input)` çağırın.
`Game::Update` gerçek zaman alır; sahne `TimeDomain::Real` veya `Game` seçer.
`SetPaused` oyun zamanını durdurur; gerçek zamanlı menüleri durdurmak için ayrı politika gerekir.
`Time()` hitstop/slow motion uygular. `FrameSeconds` host'un frame süresini, `DrawSeconds`
sunum saatini özelleştirir. Dekoratif View çizimleri [DrawTime](Graphics/Effects.md) kullanır.

`SetScene` doğrudan callback ortasında sahne yıkmaz; geçişi kuyruğa alır.
`SetScene(scene, true)` aynı sahneye tekrar giriş sağlar. Reset gereken model/View durumu
`Enter` içinde açıkça temizlenmelidir; `Start` tekrar çağrılmaz.
