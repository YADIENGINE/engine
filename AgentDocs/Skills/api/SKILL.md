---
name: api
description: Write game or editor-scene code against the engine API - find the right public declaration, compiled recipe and concept doc for a task (drawing, scenes, input, audio, saves, effects, UI, editor scenes, tools).
---

# Find the engine API for a task

1. Ask instead of reading or searching the engine:
   - `python Engine api fade between scenes` (words): the best task routes (headers, recipe, concept doc), then the
     declarations that share the most words.
   - `python Engine api CollisionWorld` or `python Engine api CollisionWorld::Sweep` (a name): the declaration as
     `path:line` with its comment; a type lists its members with their lines.
2. Open the route's recipe when there is one (`AgentDocs/Recipes/`): its code compiles in the engine's tests. Open a
   header only at the printed lines.
3. Read the route's `Docs/` section only when you need the concept.
4. Includes omit `Core/` and `Assets/` (`#include "Scene/Scene.h"`, `#include "Graphics/DefaultIcons.h"`); editor
   includes start at the engine root (`#include "Editor/Api/EditorScene.h"`).
5. Keep the hard invariants in `AgentDocs/Agent.md`. Game code includes only public headers, never `Internals/`.
6. Missing API is added to the engine, not worked around in the game; say so when a task needs it.
