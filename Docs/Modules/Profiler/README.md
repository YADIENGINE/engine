# Profiler

Opt-in main-thread CPU profiling, bounded frame recording, statistics, benchmark comparison and versioned reports.
EngineProfiler is headless and depends on EngineJsonIO for report IO. EngineProfilerRuntime adds a Game observer
and Windows/OpenGL 3.3 timestamp backend; it links Engine and, on Windows, opengl32/advapi32. Neither target
depends on Editor or Game sources. Normal games do not link these targets.

The sole supported entry point is `Modules/Profiler/Profiler.h`. Internals are private.

## Recording

Own one Profiler per session. Register scope/counter names during setup. Names are interned once, limited to
256 of each. Recording, scopes and runtime callbacks belong to the owning main thread; the object is not
thread-safe. Nesting is limited to 64 scopes. Capacity defaults to 4096 frames (maximum 120000); overflow
discards the oldest sample and increments DroppedFrames.

```cpp
Profiler profiler;
const auto update = profiler.RegisterScope("Update");
const auto items = profiler.RegisterCounter("Items");
profiler.BeginFrame();
{
    auto measurement = profiler.Measure(update);
    // Application work.
    profiler.RecordCounter(items, 1);
}
profiler.EndFrame();
auto frames = profiler.TakeFrames();
```

Measure returns a noncopyable RAII measurement; nested scopes record inclusive and self time. Measurements
outside an open frame do nothing. BeginScope/EndScope support explicit callback adapters. EndFrame closes
outstanding regions. Frame interval spans consecutive completed frames; the first frame after ClearFrames
uses its own start/end. CPU regions measure elapsed wall time, including driver waits. ClearFrames drops
buffered samples and resets the interval anchor. TakeFrames moves completed samples out without resetting it.

RecordFrame imports samples from other clocks and validates finite nonnegative timings and registered IDs.
SetGpuTime associates delayed GPU timing by frame ID; expired frames are ignored. ScopeNames/CounterNames
are the dictionaries for captures. Counter values are nonnegative quantities, accumulated within each frame.

## Runtime attachment

Link EngineProfilerRuntime to use Attach(Game&), Detach, PollGpu, PendingGpu, GpuSupported and RuntimeMetadata.
Game and its GL context must outlive the attachment. Attach before dispatch starts; detach after dispatch has
stopped. A Game accepts one observer. Toggle SetEnabled between frames instead of destroying an observer
during dispatch. Runtime recording is disabled initially; explicit BeginFrame recording is independent of
the runtime enable switch.

Game::PrepareFrame is the general host boundary for commands/resets before an instrumented frame starts.
The engine only notifies a borrowed FrameObserver; it does not depend on this module. Disabled callbacks do
not read clocks, allocate samples or issue GPU queries.

Automatic regions cover input, audio, application/scene updates, canvas CPU, worker bridge, present and
EndDrawing/wait. EndDrawing includes swap, frame limiting and event polling; it is not pure idle time.
GPU timestamps surround canvas submission at existing render boundaries. Eight query pairs are allocated
on first enabled use; results are read only after availability. No glFinish or blocking query retrieval is
used. A saturated query ring leaves that frame's GPU sample missing. Unsupported backends retain CPU
recording. Missing GPU values never contribute zeros to statistics. CPU/GPU timings overlap.

RuntimeMetadata reads build configuration, executable fingerprint (FNV-1a, not a security hash), CPU,
machine name and GPU/vendor/GL-driver details. The editor requests it once during worker setup. Reports
stay local; exported reports include this machine information.

## Execution policy and counters

Game::ConfigureFrameExecution changes FPS and VSync transiently without writing player options.
Painter batches lights automatically. Runtime observation records stage timing; custom counters remain
available through the Profiler API. `Game::RenderStatistics()` reports renderer submissions and resource work.
No global Profiler instance is used.

## Statistics and reports

Describe sorts samples and linearly interpolates at `(n - 1) * percentile`. Summarize computes mean, median,
P95, P99 and maximum for frame/GPU/CPU samples. FPS is `1000 / mean frame ms`, not the mean of per-frame FPS.
GPU coverage/missing count is explicit. An absent registered counter contributes zero for that frame.

Capture holds metadata, scope/counter/variant dictionaries and per-variant/repeat runs. Recipe defaults:
120 warmup frames, 600 measured frames, three pairs, 1/60 s simulation. RunOrder alternates AB, BA, AB.
Bounds: 0..10000 warmup, 2..10000 measured frames, 1..6 pairs, step greater than zero and no more than 1 s.

Compare defaults to variant B in both recordings. Scenario/version, common parameters, graphics, execution
policy, machine/driver, build and recipe must match; executable fingerprints may differ. Incomplete captures
get no automatic success label. Changes below 3%, fewer than three repeats or mixed repeat directions are
Uncertain. Faster median with more than 3% worse P95 is identified separately. Labels describe observations,
not statistical significance.

Encode/Decode exchange owned JsonIO documents. Save/Load use atomic JSON IO, schema version 1, 128 MiB limit.
Markdown produces summary, CPU/counter and repeat tables. Invalid samples, recipes and formats throw standard
exceptions or JsonIO errors. Observer callbacks do not propagate exceptions into the loop. Serializing and
writing files belong outside measured work. [Editor workflow](../../Editor/Profiler.md).
