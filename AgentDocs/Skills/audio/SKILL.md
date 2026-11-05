---
name: audio
description: Check, compare or clean up the project's sounds without hearing them (length, loudness, silent edges, clipping, waveform image), find unused sound files or missing catalog files, see which catalog ID plays a file, record and measure what a running scene plays (pan, fades, loop seams), or play a sound for the developer in the editor's Audio Preview.
---

# Work with sounds

You cannot hear. Judge sounds by numbers and waveform images; let the developer listen before calling a sound good.
Commands run from the project root. Reference: `Docs/AudioTools.md`. To add a sound to the game, add a row to its sound
table: `Recipes/SoundTable.md`. The engine's ready-made menu sounds need no copying: `Docs/Audio.md#ready-made-sounds`. A
sound the settings do not name stays out of the package unless it is in `release.keepAssets`: `Docs/Release.md#only-the-sounds-a-game-uses`. Menu sounds are rows too, given once in `settings.uiSounds`: `Recipes/UiSounds.md`; write no play call on a screen. Table rows appear below as catalog entries, one per file, under the row's `label`
(`Sfx[0]` without one): give a row a label when you will need to find it here.

1. **Facts:** `python Engine audio info FILE...` (paths may be relative to the asset folder, e.g. `Audio/hit.ogg`).
   - Exit 2 with "No editor worker is built yet": run `python Engine editor --build-only` once (it builds, it does not
     open the editor), then retry.
   - Compare loudness by `rmsDb`; `clipped` > 0 with `peakDb` near 0 means distortion; a long `leadingSilence` makes a
     short effect feel late.
2. **Look:** `python Engine audio waveform FILE` and read the PNG at `path`. Faint lines mark every `tickSeconds`.
3. **Usage:** `python Engine audio usage --unused` lists files the catalog never plays; `--missing` lists catalog entries
   whose file is gone; `python Engine audio usage FOLDER` shows one folder with the entries that play each file. The
   catalog comes from the last editor build: after changing it in code, build again before trusting the answer. Never
   delete a sound only because it is unused; list it and ask.
4. **What a scene plays:** for pan, distance, fades, layers or loops, `python Engine ctl audio capture SECONDS` while the
   scene runs, then `audio info` and `audio waveform` on the `path` it prints. Left and right are channels 1 and 2: a
   sound on the right is louder in 2. A loop seam is a spike in the waveform at the loop's length. Spatial sound and
   live loops: `Recipes/SpatialSound.md`.
5. **Change:** edit or regenerate the sound (`Docs/AudioGeneration.md` for generated sets), then repeat steps 1–2 and
   compare with the numbers from before.
6. **Let the developer listen:** only with an open editor (the developer said so, or you run in its Console);
   otherwise say how to listen instead. Then `python Engine ctl audio play FILE` (add `--at SECONDS` for a moment, `--loop` for music), and
   `python Engine ctl audio` to see what plays. If Agent control is off, ask the developer to turn it on or to click the
   sound in Assets.
7. **Report:** the numbers that changed (before → after), the waveform path, and that it is playing in Audio Preview
   if you started it. Ask the developer how it sounds; do not claim it sounds right.
