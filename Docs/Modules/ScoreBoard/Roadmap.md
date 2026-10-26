# Planned extensions

These are extension points, not supported features or scheduled commitments:

- Game-specific validation policies and evidence contracts in consuming games.
- Optional history retention, paging and query filters if real usage requires them.
- Explicit board-schema migrations when a future version changes the stored contract.
- A separate design for remote storage/authentication if an online board is needed.

Consumer submission/UI integration and anti-cheat checks are outside this module's
initial infrastructure work. Duplicate submissions are deliberately retained.
