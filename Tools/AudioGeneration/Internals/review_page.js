"use strict";
(() => {
  const data = JSON.parse(document.getElementById("review-data").textContent);
  const byId = Object.fromEntries(data.clips.map(clip => [clip.id, clip]));
  const $ = id => document.getElementById(id);
  const audio = $("audio");
  const state = {selected: "reference", variant: data.clips[1].id, solo: {}, fine: {}, position: 0,
    align: data.settings.align, match: data.settings.match, focus: data.settings.focus, serial: 0};
  const finite = value => value !== null && Number.isFinite(value);
  const number = (value, digits = 2) => finite(value) ? value.toFixed(digits) : "n/a";
  const selected = () => byId[state.selected];
  const track = () => selected().tracks[state.solo[state.selected] || state.selected];
  const offset = clip => state.align ? (clip.features.landmarks.onset_seconds || 0) : 0;
  const duration = clip => Math.max(0, clip.duration - offset(clip));
  const extent = () => Math.max(0.001, duration(byId.reference), duration(byId[state.variant]));
  function message(text) { $("error").textContent = text; $("error").hidden = !text; }
  function currentTime() { return audio.readyState ? Math.max(0, audio.currentTime - offset(selected())) : state.position; }
  function play() {
    if (!selected().duration) return;
    if (audio.ended || currentTime() >= duration(selected()) - 0.001) seek(0);
    audio.play().catch(error => message("Playback could not start: " + error.message));
  }
  function seek(time) {
    state.position = Math.min(duration(selected()), Math.max(0, time));
    if (audio.readyState) audio.currentTime = Math.min(selected().duration, state.position + offset(selected()));
    updateTime(); draw();
  }
  function applyGain() {
    const clip = selected(), source = track();
    const base = state.match === "raw" ? data.settings.raw_gain : clip.matching[state.match].gain;
    const fine = state.fine[clip.id] || 0;
    const desired = base * Math.pow(10, fine / 20);
    const maxPeak = Math.max(...Object.values(clip.tracks).map(item => item.peak));
    const applied = Math.min(desired, maxPeak ? 0.89 / maxPeak : desired);
    audio.volume = Math.min(1, Math.max(0, applied / source.encoding_gain)) * Number($("volume").value);
    const profile = state.match === "raw" ? null : clip.matching[state.match];
    const capped = applied < desired * 0.999999 || (profile && profile.limited_by_peak);
    $("gain-status").textContent = (state.match === "raw" ? "Common gain " + number(data.settings.raw_gain_db) + " dB" :
      "Playback gain " + number(20 * Math.log10(Math.max(applied, 1e-30))) + " dB") +
      (capped ? " · headroom cap" : "") + (profile && profile.limited_by_gain ? " · boost limit" : "");
    $("gain").value = fine; $("gain-label").textContent = number(fine, 1) + " dB";
    return applied;
  }
  function plotGain(clip) {
    const base = state.match === "raw" ? data.settings.raw_gain : clip.matching[state.match].gain;
    const desired = base * Math.pow(10, (state.fine[clip.id] || 0) / 20);
    const maxPeak = Math.max(...Object.values(clip.tracks).map(item => item.peak));
    return Math.min(desired, maxPeak ? 0.89 / maxPeak : desired);
  }
  function choose(id, soloId = null) {
    const wasPlaying = !audio.paused, time = currentTime();
    audio.pause(); state.selected = id;
    if (soloId !== null) state.solo[id] = soloId;
    if (id !== "reference") state.variant = id;
    const token = ++state.serial, source = track();
    state.position = Math.min(time, duration(selected()));
    audio.onloadedmetadata = () => {
      if (token !== state.serial) return;
      seek(state.position);
      if (wasPlaying && state.position < duration(selected())) play();
    };
    audio.src = source.data_uri; audio.load(); applyGain(); render();
    message("");
  }
  function updateTime() {
    $("seek").max = extent(); $("seek").value = currentTime();
    $("time").textContent = number(currentTime(), 3) + " s";
    $("duration").textContent = number(extent(), 3) + " s";
    $("play").textContent = audio.paused ? "▶ Play" : "Ⅱ Pause";
  }
  function renderCards() {
    const list = $("clip-list"); list.replaceChildren();
    for (const clip of data.clips) {
      const button = document.createElement("button");
      button.className = "clip-card" + (clip.id === state.selected ? " active" : "") +
        (clip.id === state.variant ? " variant-choice" : "");
      button.setAttribute("aria-pressed", String(clip.id === state.selected));
      const tag = document.createElement("span"); tag.className = "clip-tag";
      tag.textContent = clip.id === "reference" ? "A / REFERENCE" : "B / VARIANT";
      const name = document.createElement("strong"); name.textContent = clip.name;
      const meta = document.createElement("span"); meta.className = "clip-meta";
      const raw = clip.features.raw;
      meta.textContent = number(clip.duration, 3) + " s · peak " + number(raw.peak_dbfs, 1) + " dBFS · " +
        Object.keys(clip.tracks).length + " track(s)";
      button.append(tag, name, meta); button.onclick = () => choose(clip.id); list.append(button);
    }
  }
  function renderDetails() {
    const clip = selected(), source = track(), raw = source.analysis || clip.features.raw;
    $("selected-name").textContent = clip.name;
    $("now-playing").textContent = (clip.id === "reference" ? "A" : "B") + " · " + clip.name + " · " + source.label;
    $("pick-a").setAttribute("aria-pressed", String(clip.id === "reference"));
    $("pick-b").setAttribute("aria-pressed", String(clip.id !== "reference"));
    const solo = $("solo"); solo.replaceChildren();
    for (const [id, item] of Object.entries(clip.tracks)) {
      const option = document.createElement("option"); option.value = id; option.textContent = item.label; solo.append(option);
    }
    solo.value = state.solo[clip.id] || clip.id;
    const stats = $("stats"); stats.replaceChildren();
    for (const [label, value] of [["Original duration", number(clip.duration, 3) + " s"],
      ["Raw peak", number(raw.peak_dbfs, 1) + " dBFS"], ["Raw RMS", number(raw.rms_dbfs, 1) + " dBFS"],
      ["Mix onset", number(clip.features.landmarks.onset_seconds, 3) + " s"]]) {
      const box = document.createElement("div"); box.className = "stat";
      const caption = document.createElement("span"), strong = document.createElement("strong");
      caption.textContent = label; strong.textContent = value; box.append(caption, strong); stats.append(box);
    }
    const warnings = $("warnings"); warnings.replaceChildren();
    for (const warning of raw.warnings) {
      const row = document.createElement("p"); row.className = "warning"; row.textContent = warning.message; warnings.append(row);
    }
    $("provenance").textContent = JSON.stringify({random: clip.features.random, parameters: clip.features.parameters,
      source: clip.features.source, selected_layer: source.metadata || null}, null, 2);
    $("raw-download").href = source.raw_path;
  }
  function renderComparison() {
    const variant = byId[state.variant], report = data.comparisons[state.variant];
    $("compare-title").textContent = variant.name + " vs Reference";
    const rows = $("metric-rows"); rows.replaceChildren();
    const findings = $("findings"); findings.replaceChildren();
    for (const metric of Object.values(report.metrics[state.focus])) {
      const row = document.createElement("tr");
      const notice = metric.delta !== null && Math.abs(metric.delta) >= metric.notice_threshold;
      for (const [index, value] of [metric.label, number(metric.reference, 3), number(metric.candidate, 3),
        (finite(metric.delta) && metric.delta > 0 ? "+" : "") + number(metric.delta, 3) + " " + metric.unit].entries()) {
        const cell = document.createElement("td"); cell.textContent = value;
        if (index === 3 && notice) cell.className = "notice";
        row.append(cell);
      }
      rows.append(row);
      if (notice) {
        const item = document.createElement("li"); item.textContent = metric.label + ": " +
          (metric.delta > 0 ? "+" : "") + number(metric.delta, 3) + " " + metric.unit + " relative to reference.";
        findings.append(item);
      }
    }
    if (!findings.children.length) {
      const item = document.createElement("li"); item.textContent = "No available metric crosses its reporting threshold in this stage."; findings.append(item);
    }
  }
  function canvas(id) {
    const element = $(id), width = Math.max(1, element.clientWidth);
    if (!element.dataset.logicalHeight) element.dataset.logicalHeight = element.getAttribute("height");
    const height = Number(element.dataset.logicalHeight);
    const ratio = window.devicePixelRatio || 1;
    if (element.width !== Math.round(width * ratio) || element.height !== Math.round(height * ratio)) {
      element.width = Math.round(width * ratio); element.height = Math.round(height * ratio);
    }
    element.style.height = height + "px";
    const context = element.getContext("2d"); context.setTransform(ratio, 0, 0, ratio, 0, 0); context.clearRect(0, 0, width, height);
    return {context, width, height};
  }
  function drawWave() {
    const {context: ctx, width, height} = canvas("waveform"), end = extent();
    const clip = selected(), gain = plotGain(clip), referenceGain = plotGain(byId.reference);
    const maxAmplitude = Math.max(0.01, clip.features.raw.peak_amplitude * gain,
      byId.reference.features.raw.peak_amplitude * referenceGain) * 1.05;
    ctx.strokeStyle = "#263341"; ctx.lineWidth = 1;
    for (let line = 1; line < 4; line++) { ctx.beginPath(); ctx.moveTo(0, height * line / 4); ctx.lineTo(width, height * line / 4); ctx.stroke(); }
    if (state.focus !== "all") {
      const span = clip.features.landmarks.segments[state.focus], rate = clip.features.raw.sample_rate;
      const x = (span[0] / rate - offset(clip)) / end * width;
      ctx.fillStyle = "#b8ef8711"; ctx.fillRect(x, 0, (span[1] - span[0]) / rate / end * width, height);
    }
    function waveform(item, color, opacity, level) {
      ctx.strokeStyle = color; ctx.globalAlpha = opacity; ctx.beginPath();
      item.plots.waveform.forEach((pair, index) => {
        const x = (index / item.plots.waveform.length * item.duration - offset(item)) / end * width;
        ctx.moveTo(x, height / 2 - pair[0] * level / maxAmplitude * height * 0.45);
        ctx.lineTo(x, height / 2 - pair[1] * level / maxAmplitude * height * 0.45);
      }); ctx.stroke(); ctx.globalAlpha = 1;
    }
    waveform(byId.reference, "#78cfe1", 0.35, referenceGain); waveform(clip, "#b8ef87", 0.9, gain);
    ctx.strokeStyle = "#eabf82"; ctx.beginPath();
    clip.plots.envelope.forEach((value, index) => {
      const x = (index / clip.plots.envelope.length * clip.duration - offset(clip)) / end * width;
      const y = height / 2 - value * gain / maxAmplitude * height * 0.45;
      if (index) ctx.lineTo(x, y); else ctx.moveTo(x, y);
    }); ctx.stroke();
    ctx.strokeStyle = "#edf1f5"; const x = currentTime() / end * width; ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke();
    ctx.fillStyle = "#9faebf"; ctx.font = "10px Segoe UI"; ctx.fillText("±" + number(maxAmplitude, 3), 8, 13);
    $("plot-end").textContent = number(end, 3) + " s";
  }
  function drawSpectrum() {
    const {context: ctx, width, height} = canvas("spectrogram"), clip = selected();
    const frames = clip.plots.spectrogram, gainDb = 20 * Math.log10(Math.max(plotGain(clip), 1e-30));
    const colors = [[17,24,39],[39,100,123],[142,191,120],[255,219,131]];
    frames.forEach((frame, index) => {
      const time = frames.length > 1 ? index / (frames.length - 1) * clip.duration : 0;
      const x = (time - offset(clip)) / extent() * width;
      const cellWidth = Math.max(1, clip.duration / Math.max(1, frames.length - 1) / extent() * width + 1);
      frame.forEach((value, band) => {
        const normalized = Math.max(0, Math.min(0.999999, (value + gainDb + 80) / 80)) * 3;
        const lower = Math.floor(normalized), fraction = normalized - lower;
        const rgb = colors[lower].map((component, k) => Math.round(component * (1 - fraction) + colors[lower + 1][k] * fraction));
        ctx.fillStyle = "rgb(" + rgb.join(",") + ")";
        ctx.fillRect(x, height - (band + 1) / frame.length * height, cellWidth, height / frame.length + 1);
      });
    });
    const x = currentTime() / extent() * width; ctx.strokeStyle = "#edf1f5"; ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke();
    const frequencies = clip.plots.frequencies;
    $("freq-range").textContent = frequencies.length ? number(frequencies[0], 0) + " Hz → " + number(frequencies.at(-1), 0) + " Hz" : "No signal";
  }
  function draw() { drawWave(); drawSpectrum(); }
  function render() { renderCards(); renderDetails(); renderComparison(); updateTime(); draw(); }
  $("title").textContent = data.title; document.title = data.title;
  $("save-selection").onclick = () => {
    const clip = byId[state.variant], trackId = state.solo[clip.id] || clip.id;
    const selection = {schema_version: 1, kind: "review_selection", title: data.title,
      variant: {id: clip.id, name: clip.name, random: clip.features.random, parameters: clip.features.parameters,
        source: clip.features.source, track: trackId, layer: clip.tracks[trackId].metadata || null},
      listening: {match: state.match, align: state.align, focus: state.focus,
        fine_gain_db: state.fine[clip.id] || 0, target_dbfs: data.settings.target_dbfs,
        max_gain_db: data.settings.max_gain_db, timeline_seconds: currentTime()}};
    const url = URL.createObjectURL(new Blob([JSON.stringify(selection, null, 2)], {type: "application/json"}));
    const link = document.createElement("a"); link.href = url; link.download = "listening-selection.json"; link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  };
  $("align").checked = state.align; $("match").value = state.match; $("focus").value = state.focus;
  $("play").onclick = () => audio.paused ? play() : audio.pause();
  $("restart").onclick = () => seek(0);
  $("pick-a").onclick = () => choose("reference"); $("pick-b").onclick = () => choose(state.variant);
  $("seek").oninput = event => seek(Number(event.target.value));
  $("align").onchange = event => { const time = currentTime(); state.align = event.target.checked;
    seek(time); render(); };
  $("match").onchange = event => { state.match = event.target.value; applyGain(); draw(); };
  $("volume").oninput = applyGain;
  $("gain").oninput = event => { state.fine[state.selected] = Number(event.target.value); applyGain(); draw(); };
  $("reset-gain").onclick = () => { state.fine[state.selected] = 0; applyGain(); draw(); };
  $("solo").onchange = event => choose(state.selected, event.target.value);
  $("focus").onchange = event => { state.focus = event.target.value; renderComparison(); draw(); };
  for (const id of ["waveform", "spectrogram"]) $(id).onclick = event => {
    const rect = event.currentTarget.getBoundingClientRect(); seek((event.clientX - rect.left) / rect.width * extent());
  };
  audio.addEventListener("timeupdate", () => { state.position = currentTime(); updateTime(); draw(); });
  audio.addEventListener("play", updateTime); audio.addEventListener("pause", updateTime);
  audio.addEventListener("ended", () => { if ($("loop").checked) { seek(0); play(); } else updateTime(); });
  audio.addEventListener("error", () => message("The browser could not decode this preview. Download its WAV to inspect it."));
  window.addEventListener("resize", draw);
  document.addEventListener("keydown", event => {
    if (["INPUT", "SELECT", "TEXTAREA", "BUTTON"].includes(event.target.tagName) || event.ctrlKey || event.metaKey || event.altKey) return;
    const key = event.key.toLowerCase();
    if (key === " ") { event.preventDefault(); $("play").click(); }
    else if (key === "a") choose("reference"); else if (key === "b") choose(state.variant);
    else if (key === "l") $("loop").checked = !$("loop").checked;
    else if (key === "arrowleft" || key === "arrowright") { event.preventDefault(); seek(currentTime() + (key === "arrowright" ? 0.1 : -0.1)); }
  });
  choose("reference");
})();
