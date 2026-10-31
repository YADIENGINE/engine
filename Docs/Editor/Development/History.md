# Editor design history

In the October 2026 structure, the general editor and project content are separate. Catalog/worker processes separate UI
crashes from the lifetime of game resources. Production Game/View drawing is reused in previews.

Presentation time comes through Painter::Time and Game::DrawSeconds instead of per-View wiring. Pause/Step therefore do
not need separate wiring for each View. PreviewSession separates control and frame transfer from Game; default
PreviewGame or the consumer's real Game adapter uses the same runtime contract.

General documentation stays under Docs/Editor; project-specific catalog/development history stays in the consumer
project's own documentation.