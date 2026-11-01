---
name: audio
description: Check, compare or clean up the project's sounds without hearing them (length, loudness, silent edges, clipping, waveform image), find unused sound files or missing catalog files, see which catalog ID plays a file, or play a sound for the developer in the editor's Audio Preview.
---

# Work with sounds

You cannot hear. Judge sounds by numbers and waveform images; let the developer listen before calling a sound good.
Commands run from the project root. Reference: `Docs/AudioTools.md`.

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
4. **Change:** edit or regenerate the sound (`Docs/AudioGeneration.md` for generated sets), then repeat steps 1–2 and
   compare with the numbers from before.
5. **Let the developer listen:** `python Engine ctl status` first; exit 2 means no editor is open, so say how to listen
   instead. Otherwise `python Engine ctl audio play FILE` (add `--at SECONDS` for a moment, `--loop` for music), and
   `python Engine ctl audio` to see what plays. If Agent control is off, ask the developer to turn it on or to click the
   sound in Assets.
6. **Report:** the numbers that changed (before → after), the waveform path, and that it is playing in Audio Preview
   if you started it. Ask the developer how it sounds; do not claim it sounds right.
