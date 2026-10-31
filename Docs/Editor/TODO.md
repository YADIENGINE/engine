# Editor UI development list

This list tracks future capabilities and the required infrastructure order, not current features. Use [Usage](Usage.md)
for implemented UI and [development notes](Development/Roadmap.md) for limits. Do not add empty buttons, services, or
unused public API before implementing a capability.

| Next capability | Required contract / infrastructure first |
| --- | --- |
| Savable parameter recipe presets | Persistent recipe format with preview identity and validated parameters; compatibility and load / save behavior. Then preset selection through Project / Inspector |
| Multiple preview tabs / contexts | Each context's own selection, parameters, inspection, input focus, and resource ownership; then assess concurrent worker needs, adding independent worker lifecycle and command targeting if needed |
| Visual A/B capture and comparison | Immutable captures with known source and preview context; then side-by-side image / comparison presentation. Existing Profiler A/B benchmarks are a separate feature |
| Scene / asset browser and gizmos | Project-supplied stable object / asset identity, selection, and editable property / transform contract; then shared selection model, panels, and viewport tools |
| Save, dirty state, Undo / Redo | Source document and save ownership, edit transaction boundaries, and reversible commands; then modified-document state, save, and history UI |
| Timeline scrubbing | Reconstructible deterministic state or timestamped snapshot / resimulation contract; then seek and timeline. Existing Pause / Reset / Step do not provide arbitrary-time jumps |
| Multiple OS windows | Separate window, GL context, monitor DPI, and input-focus lifecycle in platform / render backend; then docks movable outside the main window |

New capabilities connect to existing command and panel registrations. The engine owns general contracts; project content
and source-editing rules stay in the consumer adapter. Usage / architecture documentation and the smallest validation
scenario for changed behavior update with the capability.