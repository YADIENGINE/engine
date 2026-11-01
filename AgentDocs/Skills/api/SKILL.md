---
name: api
description: Write game or editor-scene code against the engine API - find the right public header, compiled recipe and concept doc for a task (drawing, scenes, input, audio, saves, effects, UI, editor scenes, tools).
---

# Find the engine API for a task

1. Find the task in the table of `AgentDocs/Index.md`.
2. Open its recipe when present (`AgentDocs/Recipes/`): the code there compiles in the engine's tests.
   Otherwise open the listed header; `AgentDocs/Api/` pages list each header's declarations with their comments.
3. Read the linked `Docs/` section only when you need the concept.
4. Keep the hard invariants in `AgentDocs/Agent.md`. Game code includes only public headers, never `Internals/`.
5. Missing API is added to the engine, not worked around in the game; say so when a task needs it.
