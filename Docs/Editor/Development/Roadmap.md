# Editor development notes

This file is separate from the supported feature list. Current API: [Api](../Api.md); production editor scene guide:
[CreatingScenes](../CreatingScenes.md).

## Current limits

- UI and worker processes are Windows-desktop-only.
- Image transfer uses GPU readback and two shared buffers; it is not zero-copy. Maximum canvas is 1920×1080; busy
  buffers may skip old frames.
- Stateful Views have no arbitrary-time scrubbing; use Pause, Reset, and Step.
- No saving user recipes to separate preset files, reflection, or automatic source writing.
- Custom Game adapters must set up their own save/telemetry isolation.
- If UI/worker protocol or InputKey layout changes, both targets are built together.

## Criteria for adding work

A general need must serve multiple projects. Game catalogs, custom galleries, and event choreography do not belong in
the engine. New public behavior updates usage/API/architecture documentation and the smallest relevant validation
section.
