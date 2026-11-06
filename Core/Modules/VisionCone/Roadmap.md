Planned work, not implemented. The supported API is README.md.

# VisionCone roadmap

These are possible extensions, not supported API or promises:

- Corner-aware contours for exact edge-based occluders, reusing the engine's existing visibility infrastructure where
  appropriate. Keep their dependency separate from simulation queries and preserve the single module facade.
- Composable vision regions such as a narrow long-range focus plus a wide short-range peripheral sector, when a real
  consumer establishes the combination semantics.
- Batch-query scheduling, spatial prefilters and trace budgets after profiling many observers. Keep ordering and
  partial-work behavior explicit; never silently treat untested points as invisible.

AI awareness, memory, target selection, fog history and renderer resources remain responsibilities of their consumers.
