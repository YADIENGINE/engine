---
name: visual-change
description: Make a visual or gameplay-facing change (drawing, animation, effect, layout, character, UI) and check it in the open editor with a focused work scene and screenshots before reporting.
---

# Change something visual and check it

1. **Editor?** `python Engine ctl status`. Exit 2: no editor. Make the change, build with `python Engine build`,
   and tell the developer what to open; do not launch the editor unless asked.
2. **Work scene:** create or reuse one focused scene for this request in the project's work-scene folder
   ([recipe](../../Recipes/WorkScene.md)). Show only the subject: no HUD, menus, unrelated objects or randomness unless
   asked. Expose values the developer will likely tweak as parameters. An existing scene that already isolates the
   subject (`python Engine ctl find WORDS`) can be used instead.
3. **Change:** implement in production code (the work scene only displays it), then `python Engine refresh`. A failed
   refresh prints `Errors (N):`; fix them (`AgentDocs/Skills/build-fix/SKILL.md`).
4. **Look:** `python Engine ctl open ID --screenshot --scale 0.5`, or `python Engine ctl frames --from A --to B` for
   motion. Read the images and fix obvious defects (broken, clipped, invisible, wrong timing) before reporting.
   `python Engine ctl set` and `python Engine ctl seek` reach specific states.
5. **Hand over:** leave the scene open and give the developer a short checklist; aesthetic judgment is theirs.
6. **Finish:** when the developer says the task is done, ask: delete the work scene, or move it to a permanent category?
