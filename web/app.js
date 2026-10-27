/* Page controller: starts audio, hosts the Remote UI, and feeds the engine
 * from WebMIDI, the computer keyboard and the on-screen keys. */
import { Engine } from "./engine.js";
import { Bridge } from "./bridge.js";
import { route, defaultAssign, isPassthrough, LAYERS, ENGINE_CHANNEL, LAYER_MODE_CHAN } from "./router.js";

const $ = (id) => document.getElementById(id);
const BASE = "./";
const ASSIGN_KEY = "amino.routing.v1";

let engine = null;
let bridge = null;
let assign = defaultAssign();
let routingOn = false;
let kbChannel = 1;
let savedLayerMode = null;

try {
  const saved = JSON.parse(localStorage.getItem(ASSIGN_KEY) || "null");
  if (saved && Array.isArray(saved.assign) && saved.assign.length === LAYERS) {
    assign = saved.assign;
    routingOn = !!saved.on;
    kbChannel = saved.kb || 1;
  }
} catch (_) { /* ignore */ }

function persistRouting() {
  try { localStorage.setItem(ASSIGN_KEY, JSON.stringify({ assign, on: routingOn, kb: kbChannel })); } catch (_) { /* ignore */ }
}

/* ------------------------------------------------------------- MIDI path */

const activity = new Array(16).fill(0);
function send(msg) {
  if (!engine) return;
  if (msg[0] >= 0x80 && msg[0] < 0xf0) activity[msg[0] & 15] = performance.now();
  if (!routingOn) { if (msg[0] < 0xf0) engine.midi(msg); return; }
  for (const m of route(assign, msg)) engine.midi(m);
}

function allNotesOff() {
  if (!engine) return;
  for (let c = 0; c < 16; c++) engine.midi([0xb0 | c, 123, 0]);
}

/* ------------------------------------------------------ routing -> engine */

/** Make the engine's layer mode and channels match the routing table. */
function applyRouting() {
  if (!engine || !bridge) return;
  if (routingOn) {
    if (savedLayerMode === null) savedLayerMode = bridge.values.layer_mode;
    for (let i = 0; i < LAYERS; i++) bridge.set("l" + (i + 1) + "_midi_ch", String(ENGINE_CHANNEL(i)));
    if (Number(bridge.values.layer_mode) !== LAYER_MODE_CHAN) bridge.set("layer_mode", String(LAYER_MODE_CHAN));
  } else if (savedLayerMode !== null) {
    bridge.set("layer_mode", String(savedLayerMode));
    savedLayerMode = null;
  }
}

function watchPatchChanges() {
  // A preset load brings its own layer mode; routing wins while it is on.
  bridge.subscribe((changed) => {
    if (!routingOn) return;
    if ("layer_mode" in changed && Number(changed.layer_mode) !== LAYER_MODE_CHAN) {
      savedLayerMode = changed.layer_mode;
      bridge.set("layer_mode", String(LAYER_MODE_CHAN));
    }
    for (let i = 0; i < LAYERS; i++) {
      const k = "l" + (i + 1) + "_midi_ch";
      if (k in changed && Number(changed[k]) !== ENGINE_CHANNEL(i)) bridge.set(k, String(ENGINE_CHANNEL(i)));
    }
  });
}

function buildRoutingPanel() {
  const rows = $("layers");
  rows.textContent = "";
  const opts = ["omni", "off", ...Array.from({ length: 16 }, (_, i) => String(i + 1))];
  for (let i = 0; i < LAYERS; i++) {
    const label = document.createElement("label");
    label.textContent = "Layer " + (i + 1);
    const sel = document.createElement("select");
    sel.setAttribute("aria-label", "MIDI channel for layer " + (i + 1));
    for (const o of opts) {
      const op = document.createElement("option");
      op.value = o;
      op.textContent = o === "omni" ? "Omni" : o === "off" ? "Off" : "Ch " + o;
      sel.appendChild(op);
    }
    sel.value = String(assign[i]);
    sel.onchange = () => {
      allNotesOff();
      assign[i] = sel.value === "omni" || sel.value === "off" ? sel.value : Number(sel.value);
      persistRouting();
      updateRoutingHint();
    };
    label.appendChild(sel);
    rows.appendChild(label);
  }
  const on = $("route-on");
  on.checked = routingOn;
  on.onchange = () => {
    allNotesOff();
    routingOn = on.checked;
    persistRouting();
    applyRouting();
    updateRoutingHint();
  };
  const kb = $("kb-channel");
  kb.value = String(kbChannel);
  kb.onchange = () => { allNotesOff(); kbChannel = Number(kb.value); persistRouting(); };
  updateRoutingHint();
}

function updateRoutingHint() {
  // The summary line says whether routing is on, so a forced Layer Mode =
  // Chan in the synth never comes as a surprise.
  const state = $("route-state");
  state.classList.toggle("on", routingOn);
  state.textContent = routingOn ? " · on" : " · off";
  if (routingOn) {
    const detail = document.createElement("span");
    detail.className = "route-detail";
    detail.textContent = ": " + assign.map((a, i) => "L" + (i + 1) + " " + (a === "omni" ? "omni" : a === "off" ? "off" : "ch " + a)).join(", ");
    state.appendChild(detail);
  }
  const hint = $("route-hint");
  if (!routingOn) hint.textContent = "Off: the patch's own Layer Mode decides which layers play.";
  else if (isPassthrough(assign)) hint.textContent = "On, all layers Omni: every layer hears every channel.";
  else hint.textContent = "On: forces Layer Mode = Chan. Omni layers hear all channels; Off layers are silent.";
}

/* ------------------------------------------------------------------ MIDI */

let midiAccess = null;
let midiInputId = "all";
function onMidiMessage(e) {
  if (midiInputId !== "all" && e.target.id !== midiInputId) return;
  const d = e.data;
  if (d.length >= 1 && d[0] >= 0x80 && d[0] < 0xf0) send([d[0], d[1] || 0, d[2] || 0]);
}

function refreshMidiInputs() {
  const sel = $("midi-in");
  const cur = midiInputId;
  sel.textContent = "";
  const all = new Option("All inputs", "all");
  sel.appendChild(all);
  if (midiAccess) {
    for (const input of midiAccess.inputs.values()) {
      sel.appendChild(new Option(input.name || input.id, input.id));
      input.onmidimessage = onMidiMessage;
    }
  }
  sel.value = [...sel.options].some((o) => o.value === cur) ? cur : "all";
  midiInputId = sel.value;
  const n = midiAccess ? midiAccess.inputs.size : 0;
  $("midi-in-label").hidden = !midiAccess;
  if (midiAccess) $("midi-status").textContent = n ? n + " MIDI input" + (n > 1 ? "s" : "") : "no MIDI inputs";
}

/** Short status in the bar; the full reason on hover. */
function midiStatus(text, detail) {
  const s = $("midi-status");
  s.textContent = text;
  s.title = detail || "";
}

async function startMidi() {
  if (!navigator.requestMIDIAccess) {
    $("midi-in-label").hidden = true;
    midiStatus("No WebMIDI", "This browser has no WebMIDI: use Chrome, Edge or Opera for a MIDI keyboard. The on-screen and computer keys still play.");
    return;
  }
  try {
    midiAccess = await navigator.requestMIDIAccess({ sysex: false });
    midiAccess.onstatechange = refreshMidiInputs;
  } catch (err) {
    midiStatus("MIDI blocked", "MIDI access was denied; allow it in the browser's site settings and reload.");
  }
  refreshMidiInputs();
}

/* --------------------------------------------------------- on-screen keys */

const down = new Set();
const baseNote = () => 12 * (Number($("octave").value) + 1);
function noteOn(n, vel = 100) { if (down.has(n)) return; down.add(n); send([0x90 | (kbChannel - 1), n, vel]); paint(); }
function noteOff(n) { if (!down.delete(n)) return; send([0x80 | (kbChannel - 1), n, 0]); paint(); }
const keyEls = [];
const KEYMAP = "awsedftgyhujk";
function paint() {
  const oct = Number($("octave").value);
  keyEls.forEach((el, i) => {
    el.classList.toggle("down", down.has(baseNote() + i));
    if (i % 12 === 0) el.querySelector(".note").textContent = "C" + (oct + i / 12);
  });
}

function releaseAll() { [...down].forEach(noteOff); }

function setOctave(o) {
  if (o < 1 || o > 6) return;
  releaseAll();
  $("octave").value = String(o);
  paint();
}

function typingIn(e) {
  const t = e.target;
  return t && (t.tagName === "INPUT" || t.tagName === "SELECT" || t.tagName === "TEXTAREA");
}

function onKeyDown(e) {
  if (e.repeat || e.metaKey || e.ctrlKey || e.altKey || typingIn(e)) return;
  const k = e.key.toLowerCase();
  if (k === "z" || k === "x") { setOctave(Number($("octave").value) + (k === "z" ? -1 : 1)); return; }
  const i = KEYMAP.indexOf(k);
  if (i >= 0) noteOn(baseNote() + i);
}

function onKeyUp(e) {
  const i = KEYMAP.indexOf(e.key.toLowerCase());
  if (i >= 0) noteOff(baseNote() + i);
}

/** The synth UI is an iframe: keys typed while it has focus play too. */
function listenInside(frame) {
  frame.addEventListener("load", () => {
    try {
      const w = frame.contentWindow;
      w.addEventListener("keydown", onKeyDown);
      w.addEventListener("keyup", onKeyUp);
      w.addEventListener("blur", releaseAll);
    } catch (_) { /* cross-origin host: the page keys still work */ }
  });
}

function buildKeys() {
  const NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  for (let i = 0; i < 25; i++) {
    const el = document.createElement("div");
    const black = [1, 3, 6, 8, 10].includes(i % 12);
    el.className = "key" + (black ? " black" : "") + (i % 12 === 0 ? " c" : "");
    if (i < KEYMAP.length) {
      const hint = document.createElement("span");
      hint.className = "hint";
      hint.textContent = KEYMAP[i];
      el.appendChild(hint);
    }
    const note = document.createElement("span");
    note.className = "note";
    note.textContent = black ? "" : NAMES[i % 12];
    el.appendChild(note);
    el.addEventListener("pointerdown", (e) => {
      el.setPointerCapture(e.pointerId);
      noteOn(baseNote() + i, Math.round(40 + 87 * (e.offsetY / el.clientHeight)));
    });
    el.addEventListener("pointerup", () => noteOff(baseNote() + i));
    el.addEventListener("pointercancel", () => noteOff(baseNote() + i));
    $("keys").appendChild(el);
    keyEls.push(el);
  }
  window.addEventListener("keydown", onKeyDown);
  window.addEventListener("keyup", onKeyUp);
  window.addEventListener("blur", releaseAll);
  listenInside($("ui"));
  paint();
}

/* ------------------------------------------------------------------ boot */

function tickActivity() {
  const now = performance.now();
  const lit = [];
  for (let c = 0; c < 16; c++) if (now - activity[c] < 250) lit.push(c + 1);
  $("activity").textContent = lit.length ? "ch " + lit.join(", ") : "";
  requestAnimationFrame(tickActivity);
}

async function start() {
  const btn = $("start");
  btn.disabled = true;
  btn.textContent = "Starting…";
  try {
    engine = await Engine.start(BASE);
    engine.onPeak = (v) => { $("meter").style.width = Math.min(100, v * 100) + "%"; };
    bridge = new Bridge(engine, BASE);
    window.aminoBridge = bridge;      // read by the iframe's schwungRemote
    await bridge.init();
    watchPatchChanges();
    applyRouting();
    const sendTempo = () => engine.tempo(Number($("bpm").value) || 120, $("run").checked);
    $("bpm").onchange = sendTempo;
    $("run").onchange = sendTempo;
    sendTempo();
    if (globalThis.AMINO_EMBED) $("ui").srcdoc = AMINO_EMBED.uiDoc();
    else $("ui").src = BASE + "remote-ui/amino/web_ui.html?component=synth";
    document.body.classList.add("running");
    startMidi();
  } catch (err) {
    console.error(err);
    btn.disabled = false;
    btn.textContent = "Start Amino";
    $("error").textContent = String(err && err.message || err);
  }
}

buildKeys();
buildRoutingPanel();
tickActivity();
$("start").onclick = start;
document.addEventListener("visibilitychange", () => {
  if (engine && !document.hidden && engine.ctx.state === "suspended") engine.ctx.resume();
});
$("midi-in").onchange = () => { midiInputId = $("midi-in").value; };
$("octave").onchange = () => { releaseAll(); paint(); };
