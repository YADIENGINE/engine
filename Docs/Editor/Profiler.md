# Profiler

Open **Windows > Profiler**. The panel docks with Output by default and saves visibility/layout with the
project's editor preferences. Opening it does not automatically enable recording.

## Live

Open any preview or application flow, then select Record live. Worker FPS measures actual worker frames;
Stream FPS measures image publication. The worker normally runs at 30 FPS. Live data includes image
transport, frame limiting and profiler communication; use Benchmarks for controlled comparisons.

The panel shows frame/CPU/GPU statistics, glow counters, a frame graph and selected-frame inclusive/self
CPU times. The live history holds at most 600 frames, with at most ten publications per second. Host upload
measures editor texture upload; the viewport transfer metric additionally contains transport latency.
CPU and GPU execute concurrently and their timings must not be added together.

## Benchmarks

Select a Scenario, Open scenario, adjust Inspector parameters, then Compare Individual / Batched. Staged
OnReset values are applied to each new instance. Glow Full/Reduced/Off affects only the worker, is recorded
in metadata and does not touch player settings.

Defaults: seed 1729 where used, fixed 1/60 s simulation, 120 warmup frames, 600 measured frames and three
AB/BA/AB pairs. Counts are bounded editable controls. Every run reconstructs content from the same recipe
and resets the presentation clock. Warmup is excluded; measured outliers are retained.

During measurement the worker is uncapped and VSync is off. Readback, image publication and inspection
stop; input, parameter changes and playback are locked. The editor renders at 10 Hz with the last image
frozen. Cancel remains available. GPU collection polls for at most two seconds without blocking reads.
Results are transferred afterwards in bounded, backpressured chunks.

Completion/cancel restores FPS, VSync, rendering and playback, and recreates the previous preview recipe.
The old transient simulation state is reset. A worker crash leaves an incomplete capture; it cannot become
a successful baseline. The host returns to its prior frame limit when the operation ends. Use a Release
worker for conclusions; reports label Debug explicitly and reject Debug/Release automatic comparisons.
Build & Refresh uses the configuration of the current editor recipe. Building/launching remains manual.

Results show FPS, mean/median/P95/P99/max durations, CPU stages, canvas GPU time, coverage, helper counters,
absolute/percentage A/B changes and per-repeat results. Fewer blend requests may improve CPU cost while FPS
stays unchanged under another bottleneck. No fixed FPS gain is an acceptance condition. Unsupported GPU
queries show Unavailable. Glow Off should submit zero glow requests in both variants.

## Baselines and exports

After a completed benchmark use Compare: Save Baseline, Compare with Baseline, Export JSON or Export Markdown.
Enter a filename such as baseline.json. Files live in the user's EngineEditor/Profiler directory partitioned
by project key; the resulting full path appears in the panel. Player saves and the repo are not touched.
Save Baseline replaces the named file. JSON export writes capture.json; Markdown uses the given name with
.md extension. Different build fingerprints are allowed when comparing the same workload across code changes.

Scenario/version, parameters, resolution, graphics, execution policy, build configuration and machine/driver
must match for automatic labels. Mismatches still display raw values and reasons.

## Registering workloads

PreviewDefinition has optional benchmark metadata: stable ID, contract version and two named variants,
each with RenderExecutionOptions and optional authored parameter overrides. Default variants select
Individual/Batched glows. Registry validation checks identity and overrides. Increment the workload contract
version when its meaning changes, retaining the version for ordinary implementation optimizations.

A benchmark opts into fixed-step operation explicitly. Its factory must reconstruct repeatable state from
parameters and fixed seeds, use DrawTime for decorative time and call production models/Views. Other
previews still support Live. PreviewServices exposes a borrowed Profiler pointer for optional custom CPU
scopes/counters; register names during setup and never retain the pointer past Stop/destruction.

Engine supplies Glow Batching (20/100/500 glows, grid/cluster, radius, seed) without any Game dependency.
Projects add production workloads through their normal preview registry.

Protocol v5 associates profiler messages with generation/capture IDs. Stale messages are ignored. Ordered
chunks and expected run/frame counts are validated before making results visible. The existing 1 MiB IPC
message limit remains. Rebuild host and worker together after the protocol/API change.

## Manual acceptance

1. Dock/close/reopen Profiler; enable/disable Live on a preview and a real application flow.
2. In Release, compare all presets and inspect per-repeat counts and frame details.
3. Compare Full/Reduced/Off appearance, including alpha overlays after glow groups.
4. Cancel warmup, measurement, GPU collection and transfer; stop/reset/reopen and simulate worker failure.
5. Save/load/export; compare mismatched settings, resolution and build/machine metadata.
6. Verify CPU-only behavior without GPU queries and no query activity while profiling is disabled.
