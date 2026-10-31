# Editor validation

Commands are for developers; documentation-only changes do not require builds/tests. Headless infrastructure test
commands and scope are in [API](Api.md#headless-editor-tests). The user performs real window/input/visual checks; select
only the changed section.

## Preview contract

1. Catalog retrieval must work without creating GL/fonts/audio during registration.
2. The factory must receive the complete initial recipe; invalid values must be rejected atomically.
3. Immediate / NextAction / OnReset, rejection, Default, and capture must appear correctly in Inspector.
4. Pause must freeze decorative time and Update; Step must be 1/60 s, Reset clock 0.
5. Reset/Stop/reopening must release resources before the old context closes.
6. Player settings/progress/telemetry files belonging to another project must not change.

## Editor UI

1. Initial preferences use Develop: Project / Viewport / Inspector open, Output / Profiler closed. Dock panels / combine
   tabs; close and reopen from Window. Window → Layouts manages preset / custom layouts; Window → Toggle last bottom
   tool and Ctrl+J manage the last bottom panel. Fit / Focus / 100% and zoom work in Viewport's local View menu; the top
   View menu changes Focus preview / Restore workspace, UI scale, and performance metrics.
2. 75% / 100% / 125% / 150% UI scale and monitor DPI changes; long names wrap or truncate with tooltips in narrow
   panels. Secondary toolbar actions are in overflow; no horizontal toolbar scrolling.
3. Ctrl+P opens and focuses Project search, even when hidden. The favorite star does not open content; Recent lists only
   successful opens. Empty search / favorites / history states show meaningful explanations.
4. Inspector Search / Modified counter, `?` description, changed-value marker, Pending / rejection, and single-field
   reset; choice groups use full width. Narrowing the panel reduces columns for three-way choices such as Phase; normal
   controls move below labels, checkboxes stay on the same line when they fit. Button text does not become vertical
   letter columns. Reset for OnReset. Application time appears once per group or in `*` tooltips for mixed groups.
   Unchanged-field reset appears on hover / keyboard focus; layout stays still. Advanced groups start collapsed; search
   / Modified expand matches. Authored group, primary, and disabled metadata remain intact.
5. Output Last Build Try / Full separation, scope-specific counters / search / Copy Visible; click / Ctrl selection /
   Shift range / Ctrl+A / Ctrl+C / right-click copy / double-click text details. Text copying in search and details
   remains available; scrolling up or selecting stops Follow. Clear clears only Full; Open Log opens the whole session
   or attempt output. Long output preserves the first error. A failed build opens the popup once; Show Output returns
   from closed or maximized layout to Output and shows the first error, unaffected by old search or a disabled Error
   filter. No popup on cancellation; worker startup failure after successful build is explained separately. New attempts
   do not mix with previous attempts.
6. Switching Develop / Inspect / Profile does not erase the last custom layout; Last custom layout restores it. Output /
   Console bottom buttons and Ctrl+J toggle only the target panel, preserving other open panels. Profiler is absent from
   the bottom bar; it opens from View and the palette. Errors / warnings while Output is closed flash the button in
   error / warning color; the notice stays until Output is actually viewed and restarts on the next warning. Reopening
   restores layout; View starts open and in front even if saved closed / collapsed. Closing while maximized preserves
   the previous layout.
7. Product input does not arrive when the preview tab is hidden, behind another panel, or a menu is open.
8. The primary action is full-width and fixed at the panel bottom while scrolling; secondary actions use two columns
   when wide enough, one in narrow panels. Long label / option tooltips, number / color editing, and section toggles do
   not overflow. Live numbers are right-aligned, short statuses badged, long explanations wrapped; no horizontal
   overflow in the child area.
9. Open the palette with Ctrl+Shift+P: search, up / down, Enter, Escape, disabled-command reason, panel / layout
   selection. F5 / F6 / F7 / Ctrl+R / Ctrl+Shift+B follow the same availability as toolbar and menu; they do not run
   during text / popups or leak into the product.

## Infrastructure

1. Build Debug; no warnings / errors. No second visible worker window / console.
2. Image center / corner / margin clicks; drag out and release; panel, window, and DPI changes.
3. Typing in Inspector does not send keyboard input to the preview; alt-tab clears held input.
4. The main button shows Open when closed, Stop when ready / paused, Cancel during startup / catalog / build /
   benchmark, disabled Stop while closing. Repeated Stop / Open / Reset keeps the editor PID fixed; old images are not
   considered live.
5. Build errors, missing executables, worker crashes, build cancellation, and editor exit leave no orphan processes.
6. View performance-metric preference is saved; FPS and readback / publish / transfer values are visible.

## Profiler

Headless module tests with `[profiler]` verify statistics, scope nesting, missing GPU samples, limits, report IO, and
comparison conditions. Editor tests check warmup/measurement separation, A/B repetition order, cancellation,
execution-setting snapshot/restoration, benchmark metadata round-trip, and stale generation/capture messages. Tests do
not fix actual GL/FPS results. Building/running these tests belongs to the developer. For runtime setting restoration
and GPU backend, use the [Profiler manual acceptance list](Profiler.md#manual-acceptance). UI checks also select
Advanced benchmark settings, Glow override, the single Export menu, and benchmark cancellation.