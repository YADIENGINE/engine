---
name: editor
description: Show, open, look at, inspect, preview or explain something in the project's open editor or on screen (a character, enemy, card, map, menu, effect, game flow), or answer an "[editor] scene=..." line pasted from the editor. Drives the editor with `python Engine ctl`.
---

# Show something in the editor

Commands run from the project root. Reference: `Docs/Editor/Automation.md`.

1. **State:** `python Engine ctl status`.
   - Exit 2: no editor is open. Say so and suggest `python Engine editor`; do not launch it unless asked. Stop.
   - `"scene": null`: the editor is open and idle. Continue.
   - A scene runs: reuse it when it already shows the subject.
2. **Find:** `python Engine ctl find WORDS` with the subject in the scenes' language (usually English). It searches
   names, ids, descriptions, parameters and parameter choices, and returns ready `commands`. A subject is often a
   choice of a shared scene (one enemy of a fight scene), not a scene of its own. Use `python Engine ctl describe ID`
   for all parameters of one scene. Avoid `python Engine ctl list` without `--brief`: it can be very large.
3. **Open:** run the returned command, adding values the request asks for, e.g.
   `python Engine ctl open ID --set enemy=Dragon stage=Finale --action start --screenshot --scale 0.5`.
   Quote values with spaces: `--set "enemy=Ice Dragon"`.
4. **Verify:** read the screenshot path from the answer. For motion use `python Engine ctl frames --from A --to B`;
   for one moment `python Engine ctl seek SECONDS` then `python Engine ctl screenshot --scale 0.5`. If the frame is
   wrong (other subject, black, error text), fix the parameters and capture again. On errors read
   `python Engine ctl logs --level warning`.
5. **Report:** what is on screen now, in one to three lines, and what the developer can change in the Inspector.
   Never claim a result you have not seen in a capture.

## Pasted `[editor]` lines

`[editor] scene=ID t=SECONDS region=X,Y WxH ... image="PATH" params: k=v` comes from the editor's Mark tool: read
the image first; it is the region the developer means. Reproduce more frames with `python Engine ctl open ID --set ...`
and `python Engine ctl seek SECONDS`.

## Limits

- No keyboard, mouse or gamepad input can be sent: you cannot play. Offer seek, frames or a scene parameter instead.
- The developer can use the editor at the same time; your changes show as viewport notices. If a command is refused
  with "agent control is off", the developer has locked the editor: ask before continuing.
- To change what you see, use `AgentDocs/Skills/visual-change/SKILL.md`.
