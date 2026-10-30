"use strict";
// Player contract tests in an in-memory DOM/media double; no browser or network.
const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

function session(empty = false) {
  function clip(id, name, onset, peak, encodingGain) {
    const duration = empty && id === "reference" ? 0 : 2;
    const marks = {onset_seconds: duration ? onset : null, segments: {all: [0, 2000], attack: [0, 50], body: [50, 1500], tail: [1500, 2000]}};
    const raw = {sample_rate: 1000, peak_amplitude: peak, peak_dbfs: -6, rms_dbfs: -18, warnings: []};
    return {id, name, duration, features: {raw, landmarks: marks, random: {seed: "1234567890123456789012345"}, parameters: {}},
      matching: {rms: {gain: 0.5, limited_by_peak: false, limited_by_gain: false}, peak: {gain: 1, limited_by_peak: false}},
      tracks: {[id]: {label: "Mix", peak, encoding_gain: encodingGain, data_uri: "data:audio/wav;base64," + id, raw_path: "raw/" + id + ".wav"},
        [id + "-layer-001"]: {label: "Body", peak: peak / 2, encoding_gain: encodingGain * 2,
          data_uri: "data:audio/wav;base64," + id + "-stem", raw_path: "raw/stem.wav", metadata: {gain: 0.5}}},
      plots: {waveform: [[-0.5, 0.5], [-0.1, 0.1]], envelope: [0.3, 0.1], spectrogram: [[-40, -60], [-60, -80]], frequencies: [20, 500]}};
  }
  const metric = {label: "RMS", reference: -18, candidate: -20, delta: -2, unit: "dB", notice_threshold: 0.5};
  return {title: "Test review", settings: {align: true, match: "rms", focus: "all", raw_gain: 1, raw_gain_db: 0},
    clips: [clip("reference", "Reference", 0.1, 0.5, 1.78), clip("variant-001", "Quiet variant", 0.4, 0.25, 3.56)],
    comparisons: {"variant-001": {metrics: Object.fromEntries(["all", "attack", "body", "tail"].map(key => [key, {rms: metric}]))}}};
}

async function player(data = session()) {
  class Element {
    constructor(tagName = "DIV") { this.tagName = tagName; this.children = []; this.dataset = {}; this.listeners = {};
      this.value = "0"; this.hidden = false; this.checked = false; this.clientWidth = 800; this.height = 180; this.style = {}; this.attributes = {}; }
    append(...values) { this.children.push(...values); }
    replaceChildren(...values) { this.children = [...values]; }
    setAttribute(name, value) { this.attributes[name] = value; }
    getAttribute(name) { return name === "height" ? String(this.height) : this.attributes[name]; }
    addEventListener(name, listener) { (this.listeners[name] ||= []).push(listener); }
    dispatch(name) { for (const listener of this.listeners[name] || []) listener(); }
    click() { if (this.onclick) this.onclick({target: this, currentTarget: this}); }
    getBoundingClientRect() { return {left: 0, width: this.clientWidth}; }
    getContext() { return {setTransform() {}, clearRect() {}, beginPath() {}, moveTo() {}, lineTo() {}, stroke() {}, fillRect() {}, fillText() {}}; }
  }
  const ids = [...fs.readFileSync(path.join(__dirname, "../Internals/review_page.html"), "utf8").matchAll(/\bid="([^"]+)"/g)].map(match => match[1]);
  const elements = Object.fromEntries(ids.map(id => [id, new Element()]));
  elements["review-data"].textContent = JSON.stringify(data);
  elements.volume.value = "0.65"; elements.spectrogram.height = 190;
  const audio = elements.audio;
  audio.paused = true; audio.readyState = 0; audio.currentTime = 0; audio.ended = false;
  audio.pause = () => { audio.paused = true; audio.dispatch("pause"); };
  audio.play = () => { audio.paused = false; audio.ended = false; audio.dispatch("play"); return Promise.resolve(); };
  audio.load = () => { audio.readyState = 0; queueMicrotask(() => { audio.readyState = 1; if (audio.onloadedmetadata) audio.onloadedmetadata(); }); };
  const document = {getElementById: id => elements[id], createElement: tag => new Element(tag.toUpperCase()), addEventListener() {}};
  const window = {devicePixelRatio: 2, addEventListener() {}};
  const downloads = [];
  const URL = {createObjectURL(blob) { downloads.push(blob); return "blob:local-test"; }, revokeObjectURL() {}};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, "../Internals/review_page.js"), "utf8"),
    {document, window, URL, Blob, setTimeout(callback) { callback(); }});
  await Promise.resolve();
  return {elements, audio, downloads};
}

test("A/B retains aligned timeline and repeated draws keep canvas dimensions bounded", async () => {
  const {elements: el, audio, downloads} = await player();
  assert.equal(audio.currentTime, 0.1);
  audio.currentTime = 0.35;
  await audio.play();
  el["pick-b"].click(); await Promise.resolve();
  assert.ok(Math.abs(audio.currentTime - 0.65) < 1e-12);
  assert.equal(audio.paused, false);
  for (let i = 0; i < 10; i++) audio.dispatch("timeupdate");
  assert.equal(el.waveform.height, 360);
  assert.equal(el.spectrogram.height, 380);
  assert.match(el["now-playing"].textContent, /Quiet variant/);
  assert.match(el.provenance.textContent, /1234567890123456789012345/);
  el["save-selection"].click();
  const selection = JSON.parse(await downloads[0].text());
  assert.equal(selection.variant.id, "variant-001");
  assert.equal(selection.variant.random.seed, "1234567890123456789012345");
  el["pick-a"].click(); await Promise.resolve();
  assert.ok(Math.abs(audio.currentTime - 0.35) < 1e-12);
});

test("gain modes, layer solo, seek and looping preserve player contracts", async () => {
  const {elements: el, audio} = await player();
  el["pick-b"].click(); await Promise.resolve();
  el.match.value = "raw"; el.match.onchange({target: el.match});
  assert.ok(Math.abs(audio.volume - 0.65 / 3.56) < 1e-12);
  el.match.value = "rms"; el.match.onchange({target: el.match});
  assert.ok(Math.abs(audio.volume - 0.65 * 0.5 / 3.56) < 1e-12);
  el.solo.value = "variant-001-layer-001"; el.solo.onchange({target: el.solo}); await Promise.resolve();
  assert.match(audio.src, /-stem$/);
  assert.ok(Math.abs(audio.volume - 0.65 * 0.5 / 7.12) < 1e-12);
  el.seek.value = "0.3"; el.seek.oninput({target: el.seek});
  assert.ok(Math.abs(audio.currentTime - 0.7) < 1e-12);
  el.loop.checked = true; audio.ended = true; audio.dispatch("ended");
  assert.equal(audio.currentTime, 0.4); assert.equal(audio.paused, false);
  el.focus.value = "tail"; el.focus.onchange({target: el.focus});
  assert.ok(el["metric-rows"].children.length > 0);
  assert.equal(el.error.hidden, true);
});

test("empty references remain inspectable without attempting playback", async () => {
  const {elements: el, audio} = await player(session(true));
  el.play.click(); assert.equal(audio.paused, true);
  assert.equal(el.error.hidden, true);
  el["pick-b"].click(); await Promise.resolve();
  el.play.click(); assert.equal(audio.paused, false);
});
