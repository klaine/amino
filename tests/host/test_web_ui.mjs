import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import {
    envSeconds,
    formatValue,
    lfoHz,
    noteHz,
    optionIndex,
    parseValue,
    wireValue,
} from "../../src/web/format.js";
import { ECHO_HOLD_MS, expandMeta, Store, WRITE_MS } from "../../src/web/store.js";
import {
    attackLevel,
    cutoffHz,
    decayLevel,
    envStep,
    filterDb,
    lfoRateHz,
    lfoShape,
    LFO_DIV_BEATS,
    modulatedPositions,
    releaseLevel,
    ringSpan,
    waveReach,
    waveSpan,
} from "../../src/web/curves.js";
import { ModModel, sourceColor } from "../../src/web/mod.js";
import { History, snapshot } from "../../src/web/history.js";
import { parseLive } from "../../src/web/live.js";

const meta = expandMeta(JSON.parse(readFileSync(
    new URL("../../modules/sound_generators/amino/assets/ui_meta.json", import.meta.url))));
const P = meta.params;

/* ---------------------------------------------------------------- metadata */
assert.equal(meta.layerKeys.length, 126, "every layer key is listed");
for (const key of ["lfo1_pitch", "lfo2_pitch", "e4_amt1", "e4_amt2"])
  assert.deepEqual([P[key].min, P[key].max], [-1, 1], key + " is a bipolar dedicated route");
for (const key of ["e4_dst1", "e4_dst2"]) assert.ok(P[key], key + " is published");
for (const key of meta.layerKeys) assert.equal(P[key].scope, "layer", key);
assert.equal(P.cutoff.display.kind, "note_hz");
assert.equal(meta.modTargetKeys.length, P.m1_dst.options.length);
for (const key of meta.modTargetKeys) if (key) assert.ok(P[key], "mod target key " + key);
assert.equal(Object.keys(meta.fx).length, P.fx1_type.options.length - 1);
for (const [type, knobs] of Object.entries(meta.fx)) {
    assert.equal(knobs.length, 6, type);
    for (const k of knobs) assert.ok(k.default >= 0 && k.default <= 1, type + " " + k.name);
}
assert.equal(meta.presetCount, 324);
const library = JSON.parse(readFileSync(
    new URL("../../modules/sound_generators/amino/assets/ui_presets.json", import.meta.url)));
assert.equal(library.presets.length, 324, "preset list moved to ui_presets.json");
assert.ok(library.layerSources.length >= 324);
assert.equal(P.m3_via.options, P.m1_src.options, "shared option lists are restored");
assert.equal(P.osc2_table.options.length, 74);

/* ---------------------------------------------------------------- format */
assert.equal(Math.round(noteHz(69)), 440);
assert.equal(formatValue(P.cutoff, 69), "440 Hz");
assert.equal(formatValue(P.cutoff, 127), "12.5 kHz");
assert.equal(formatValue(P.e3_a, 0.5), "2.5 s");
assert.equal(formatValue(P.e3_a, 0.2), "160 ms");
assert.equal(formatValue(P.e3_s, 0.25), "25%");
assert.equal(formatValue(P.lfo1_rate, 0.4), formatValue({ display: { kind: "lfo_hz" } }, 0.4));
assert.equal(formatValue(P.lfo1_rate, 0), "0.01 Hz");
assert.equal(formatValue(P.glide, 0), "Off");
assert.equal(formatValue(P.osc1_semi, -12), "-12 st");
assert.equal(formatValue(P.osc1_semi, 7), "+7 st");
assert.equal(formatValue(P.fenv, 0.5), "+50%");
assert.equal(formatValue(P.fenv, -0.25), "-25%");
assert.equal(formatValue(P.vol, 0.8), "80%");
assert.equal(formatValue(P.key_lo, 60), "C4");
assert.equal(formatValue(P.ftype, 2), "Ladder 24dB");
assert.equal(formatValue(P.ftype, 3), "HP24");
assert.equal(formatValue(P.ftype, 6), "Off");
assert.equal(P.m1_src.options[26], "Aux Env");
assert.equal(formatValue(P.e4_d, 0.2), "160 ms");
assert.equal(formatValue(P.patch_gain, 0), "0 dB", "dB params show their unit");
assert.equal(formatValue(P.patch_gain, 3.5), "+3.5 dB");
assert.equal(formatValue(P.patch_gain, -6), "-6 dB");

const reverbDecay = { min: 0, max: 1, type: "float", ...meta.fx.Reverb[1] };
assert.equal(formatValue(reverbDecay, reverbDecay.default), "2.5 s", "fx.c default matches docs/11");
const delaySync = { min: 0, max: 1, ...meta.fx.Delay[1], normalized: true };
assert.equal(optionIndex(delaySync, delaySync.default), 7);
assert.equal(formatValue(delaySync, delaySync.default), "1/8d");

const near = (a, b, eps = 1e-3) => assert.ok(Math.abs(a - b) < eps, `${a} != ${b}`);
near(parseValue(P.cutoff, "440 Hz"), 69);
near(parseValue(P.cutoff, "2.5 kHz"), 69 + 12 * Math.log2(2500 / 440));
near(parseValue(P.cutoff, "100"), 100, 1e-9);
near(envSeconds(parseValue(P.e1_d, "350 ms")), 0.35);
near(envSeconds(parseValue(P.e1_d, "2 s")), 2);
near(lfoHz(parseValue(P.lfo1_rate, "4 Hz")), 4);
near(parseValue(P.fenv, "-30%"), -0.3);
assert.equal(parseValue(P.osc1_semi, "99"), 48, "clamped to range");
assert.equal(parseValue(P.osc1_fine, "12.6"), 13, "ints round");
assert.equal(parseValue(P.key_hi, "C#5"), 73);
assert.equal(parseValue(P.ftype, "bp12"), 5);
assert.equal(parseValue(P.cutoff, "loud"), null);
assert.equal(wireValue(P.cutoff, 79.35), "79.350");
assert.equal(wireValue(P.ftype, 2), "2");
assert.equal(wireValue(P.osc1_semi, -3.4), "-3");

assert.equal(formatValue(P.res, 0.25), "25%");
assert.equal(formatValue(P.osc1_wave, 12), "12");
assert.equal(formatValue(P.osc1_wave, 62), "Square");
assert.equal(parseValue(P.osc1_wave, "saw"), 63);

/* ---------------------------------------------------------------- curves */
// Previews: offset-binary, so classic Triangle (slot 61) starts at zero.
const preview = readFileSync(new URL(
    "../../modules/sound_generators/amino/assets/wave-previews/00.bin", import.meta.url));
const tri = preview.subarray(61 * 64, 62 * 64);
assert.ok(Math.abs(tri[0] - 128) <= 4, "triangle starts at the centre line");
assert.ok(tri[16] > 240 && tri[48] < 16, "triangle peaks up then down");
assert.ok(Math.abs(tri[32] - 128) <= 6, "the falling slope crosses zero mid-cycle");
for (let i = 17; i < 48; i++) assert.ok(tri[i] < tri[i - 1], "monotonic through zero at " + i);

near(cutoffHz(127), 12543.85, 0.1);
assert.equal(cutoffHz(0), 20, "cutoff clamps to 20 Hz like voice.c");
assert.equal(cutoffHz(140), 18000);
for (const t of [0, 1]) {
    near(filterDb(t, 69, 0, 20), 0, 0.05);                       // LP passes lows
    assert.ok(filterDb(t, 69, 0, 7040) < (t === 0 ? -60 : -30), "LP slope " + t);
}
near(filterDb(0, 69, 0, 440), 20 * Math.log10(0.25), 0.01);      // 4 poles: -12 dB at fc
near(filterDb(1, 69, 0, 440), 20 * Math.log10(0.5), 0.01);       // SVF R=1: -6 dB at fc
assert.ok(filterDb(0, 69, 0.9, 440) > filterDb(0, 69, 0, 440) + 12, "resonance peaks the ladder");
assert.ok(filterDb(1, 69, 0.9, 440) > 10, "resonance peaks the SVF");
for (const t of [3, 4]) {
    near(filterDb(t, 69, 0, 20000), 0, 0.05);                    // HP passes highs
    assert.ok(filterDb(t, 69, 0, 55) < -30, "HP slope " + t);
}
near(filterDb(5, 69, 0, 440), 0, 0.01);                           // BP unity at fc (2R scaling)
near(filterDb(5, 69, 0, 3520), 20 * Math.log10(16 / 65), 0.01);           // 6 dB/oct each side
near(filterDb(5, 69, 0, 55), filterDb(5, 69, 0, 3520), 0.01);
assert.equal(filterDb(6, 69, 1, 1000), 0, "Off is flat");
assert.ok(filterDb(2, 69, 0, 7040) < -60, "Ladder 24dB slope");

near(attackLevel(0), 0);
near(attackLevel(1), 1, 2e-3);
assert.ok(attackLevel(0.5) > 0.5, "attack is concave (chases 1.3)");
near(decayLevel(0, 0.4), 1);
near(decayLevel(1, 0.4), 0.4 + 0.6 * Math.exp(-4.6));
near(releaseLevel(1, 1), Math.exp(-4.6));

assert.deepEqual(waveReach(10, 0, 0), { lo: 10, hi: 10, fixed: false });
assert.deepEqual(waveReach(10, 0.5, 0), { lo: 10, hi: 41.5, fixed: false });
assert.deepEqual(waveReach(40, -1, 0), { lo: 0, hi: 40, fixed: false });
assert.deepEqual(waveReach(30, 1, 1), { lo: 30, hi: 60, fixed: false }, "clamped to morph slots");
assert.deepEqual(waveReach(62, 1, 0), { lo: 62, hi: 62, fixed: true }, "classic waves do not morph");
// Other sources add their spans: a bipolar LFO swings both ways, a negative
// unipolar amount pushes down, and matrix spans sum with the envelope's.
assert.deepEqual(waveSpan(0.5, true), [-31.5, 31.5]);
assert.deepEqual(waveSpan(-0.5, false), [-31.5, 0]);
assert.deepEqual(waveReach(30, 0, 0, [waveSpan(0.5, true)]), { lo: 0, hi: 60, fixed: false }, "bipolar swing clamps");
assert.deepEqual(waveReach(20, 0.5, 0, [waveSpan(-0.25, false)]), { lo: 4.25, hi: 51.5, fixed: false });

// Mod rings: the swept span in knob units.
assert.deepEqual(ringSpan(60, 0.25, 72, false, 0, 127), [60, 78]);
assert.deepEqual(ringSpan(60, -0.25, 72, false, 0, 127), [42, 60]);
assert.deepEqual(ringSpan(60, 0.25, 72, true, 0, 127), [42, 78], "bipolar swings both ways");
assert.deepEqual(ringSpan(120, 0.5, 72, false, 0, 127), [120, 127], "clamped to the knob");
assert.deepEqual(ringSpan(0.8, 0.5, "mul", false, 0, 1), [0.8, 1]);
assert.deepEqual(ringSpan(0.8, -0.5, "mul", false, 0, 1), [0.4, 0.8], "Amp Level scales the value");
assert.equal(meta.modTargetScale[P.m1_dst.options.indexOf("Cutoff")], 72);
assert.equal(meta.modTargetScale[P.m1_dst.options.indexOf("Amp Level")], "mul");
assert.equal(meta.modSources[P.m1_src.options.indexOf("LFO1")].bipolar, true);
assert.equal(meta.modSources[P.m1_src.options.indexOf("Velocity")].bipolar, false);
assert.notEqual(sourceColor(meta, 4), sourceColor(meta, 5), "LFO1 and LFO2 differ");

// LFO shapes as voice.c lfo_shape.
near(lfoShape(0, 0.25), 1);
near(lfoShape(1, 0.25), 1); near(lfoShape(1, 0.75), -1); near(lfoShape(1, 0.5), 0);
assert.equal(lfoShape(2, 0.49), 1); assert.equal(lfoShape(2, 0.5), -1);
near(lfoShape(3, 0), 1); near(lfoShape(4, 0), -1);
near(lfoShape(5, 0.5, -1, 1), 0, 1e-9);
assert.equal(lfoShape(6, 0.9, -1, 0.3), 0.3, "S&H holds");
assert.equal(LFO_DIV_BEATS.length, P.lfo1_div.options.length);
assert.equal(LFO_DIV_BEATS[P.lfo1_div.options.indexOf("1/4")], 1);
assert.equal(formatValue(P.lfo1_fade, 0), "Off");
assert.equal(parseValue(P.lfo1_fade, "off"), 0);

// Animated mod cues: envelope prediction matches env_step.
{
    // Attack reaches 1.0 exactly at am_env_time(a), then decays toward S.
    let e = { stage: 1, level: 0 };
    const a = 0.5;                          // 2.5 s
    for (let t = 0; t < 2.4; t += 0.01) e = envStep(e.stage, e.level, a, 0.5, 0.4, 0.3, 0.01);
    assert.equal(e.stage, 1, "still attacking before 2.5 s");
    for (let t = 0; t < 0.2; t += 0.01) e = envStep(e.stage, e.level, a, 0.5, 0.4, 0.3, 0.01);
    assert.equal(e.stage, 2, "attack hands over to decay at the peak");
    for (let t = 0; t < 20; t += 0.05) e = envStep(e.stage, e.level, a, 0.5, 0.4, 0.3, 0.05);
    near(e.level, 0.4, 1e-3);               // settles on sustain
    e = { stage: 3, level: 0.4 };
    for (let t = 0; t < 5; t += 0.05) e = envStep(e.stage, e.level, a, 0.5, 0.4, 0.3, 0.05);
    assert.deepEqual(e, { stage: 0, level: 0 }, "release ends in off");
    assert.deepEqual(envStep(0, 0, a, 0.5, 0.4, 0.3, 1), { stage: 0, level: 0 }, "off stays off");
}
near(lfoRateHz(0.4, false, 0, 120), 0.01 * Math.pow(5000, 0.4));
near(lfoRateHz(0.4, true, P.lfo1_div.options.indexOf("1/4"), 120), 2);     // a beat at 120 BPM
near(lfoRateHz(0.4, true, P.lfo1_div.options.indexOf("1 bar"), 120), 0.5);
{
    const rings = [{ amount: 0.25, scale: 72 }, { amount: -0.5, scale: "mul" }, { amount: 1, scale: 1 }];
    const p = modulatedPositions(60, rings, [1, null, -1], 0, 127);
    assert.deepEqual(p.each, [78, null, 59]);
    near(p.total, 60 + 18 - 1);
    assert.equal(modulatedPositions(120, [{ amount: 1, scale: 72 }], [1], 0, 127).total, 127, "clamped");
    assert.equal(modulatedPositions(60, rings, [null, null, null], 0, 127).total, null, "no live source, no ghost");
}

// Telemetry (amino.c amino_live).
const live = parseLive("v=3,2,1,0,0;l=0.250,-0.500,0.900,0.100;e=2,0.800,1,0.400,3,0.050;p=0.712;b=128.0;t=4242");
assert.equal(live.voices, 3);
assert.deepEqual(live.layerVoices, [2, 1, 0, 0]);
assert.deepEqual(live.lfo[1], { phase: 0.9, cur: 0.1 });
assert.deepEqual(live.env[2], { stage: 3, level: 0.05 });
assert.equal(live.peak, 0.712);
assert.equal(live.bpm, 128);
assert.equal(live.clockMs, 4242);
const idle = parseLive("v=0,0,0,0,0;l=-;e=-;p=0.000;b=120.0;t=1");
assert.equal(idle.lfo, null, "no voice, no LFO phase");
assert.equal(idle.env, null);
assert.equal(parseLive(""), null);
assert.equal(parseLive("garbage"), null);

/* ---------------------------------------------------------------- store */
function harness() {
    let now = 0;
    const timers = [];
    const sent = [];
    const frames = [];
    let listener = null;
    const remote = {
        component: "synth",
        setParam: (k, v) => sent.push([k, v]),
        onParamChange: (cb) => { listener = cb; },
    };
    const store = new Store(meta, remote, {
        now: () => now,
        schedule: (fn, ms) => { const t = { at: now + ms, fn }; timers.push(t); return t; },
        frame: (fn) => frames.push(fn),
    });
    const advance = (ms) => {
        now += ms;
        for (const t of timers.splice(0).sort((a, b) => a.at - b.at)) {
            if (t.at <= now) t.fn(); else timers.push(t);
        }
    };
    const paint = () => { for (const fn of frames.splice(0)) fn(); };
    store.connect();
    return { store, sent, advance, paint, push: (p) => listener(p) };
}

{
    const { store, push, paint } = harness();
    assert.equal(store.get("l3_cutoff"), "127", "defaults seed every layer");
    const seen = [];
    const b = store.bind("cutoff", (v, key) => seen.push([key, v]));
    assert.equal(b.key, "l1_cutoff");
    push({ "synth:l1_cutoff": "64.000", "synth:l2_cutoff": "30.000", "fx1:cutoff": "1" });
    assert.equal(seen.length, 1, "updates wait for the frame");
    paint();
    assert.deepEqual(seen.at(-1), ["l1_cutoff", "64.000"]);

    push({ "synth:edit_layer": "2" });
    assert.equal(b.key, "l2_cutoff", "layer bindings re-point with edit_layer");
    assert.deepEqual(seen.at(-1), ["l2_cutoff", "30.000"]);

    // A bare layer key from the device is the focused layer's alias.
    push({ "synth:cutoff": "12.000" });
    paint();
    assert.equal(store.get("l2_cutoff"), "12.000");
    assert.equal(store.get("l1_cutoff"), "64.000");
    assert.deepEqual(seen.at(-1), ["l2_cutoff", "12.000"]);

    const pinned = [];
    store.bind("cutoff", (v) => pinned.push(v), { layer: 1 });
    push({ "synth:edit_layer": "3" });
    paint();
    assert.equal(pinned.length, 1, "pinned bindings ignore layer changes");

    let painted = 0;
    store.bind("res", () => painted++);
    push({ "synth:l1_cutoff": "1.000" });
    paint();
    assert.equal(painted, 1, "only bindings of changed keys repaint");
}

{
    const { store, sent, advance, push, paint } = harness();
    const seen = [];
    const b = store.bind("cutoff", (v) => seen.push(v));
    b.set("50");
    assert.deepEqual(sent, [["synth:l1_cutoff", "50.000"]], "first write goes out at once");
    assert.equal(seen.length, 1, "the editing control is not echoed");
    b.set("51"); b.set("52"); b.set("53");
    assert.equal(sent.length, 1, "writes inside the window are coalesced");
    advance(WRITE_MS);
    assert.deepEqual(sent.at(-1), ["synth:l1_cutoff", "53.000"], "the trailing write sends the latest value");
    assert.equal(sent.length, 2);

    push({ "synth:l1_cutoff": "51.000" });
    paint();
    assert.equal(store.get("l1_cutoff"), "53", "stale echoes are ignored right after a write");
    advance(ECHO_HOLD_MS + 1);
    push({ "synth:l1_cutoff": "40.000" });
    paint();
    assert.equal(store.get("l1_cutoff"), "40.000", "device edits apply once the write settles");
    assert.equal(seen.at(-1), "40.000");

    store.set("edit_layer", "4");
    assert.equal(b.key, "l4_cutoff");
    assert.deepEqual(sent.at(-1), ["synth:edit_layer", "4"]);
}

{
    // Mod model: slots for the focused layer, rings per knob, assignment.
    const frames = [];
    globalThis.requestAnimationFrame = (fn) => frames.push(fn);
    const { store, sent, push, paint, advance } = harness();
    const model = new ModModel(store);
    const ringsSeen = [];
    const fakeKnob = { dataset: {}, setRings: (r) => ringsSeen.push(r) };
    model.register("cutoff", fakeKnob);
    assert.equal(fakeKnob.dataset.modKey, "cutoff");
    assert.deepEqual(ringsSeen.at(-1), []);
    model.register("fine_tune_not_a_target", { dataset: {}, setRings: () => assert.fail() });

    const lfo1 = P.m1_src.options.indexOf("LFO1");
    const cut = P.m1_dst.options.indexOf("Cutoff");
    const slot = model.slotFor(lfo1, "cutoff", true);
    assert.equal(slot, 1, "first empty slot");
    assert.deepEqual(sent.slice(-5).map(([k]) => k),
        ["synth:l1_m1_src", "synth:l1_m1_dst", "synth:l1_m1_amt", "synth:l1_m1_via", "synth:l1_m1_via_depth"]);
    assert.equal(store.get("l1_m1_dst"), String(cut));
    const r = ringsSeen.at(-1);
    assert.equal(r.length, 1);
    assert.equal(r[0].scale, 72);
    assert.equal(r[0].bipolar, true);
    assert.equal(r[0].amount, 0.25);
    assert.equal(model.slotFor(lfo1, "cutoff", true), 1, "an existing route is reused");
    assert.equal(model.slotFor(lfo1, "res", false), 0);

    // Device edits to slots reach the rings on the next frame (once our own
    // write has settled; see the echo hold above).
    advance(ECHO_HOLD_MS + 1);
    push({ "synth:l1_m1_amt": "-0.600" });
    paint();
    for (const fn of frames.splice(0)) fn();
    assert.equal(ringsSeen.at(-1)[0].amount, -0.6);

    // Another layer has its own slots.
    push({ "synth:edit_layer": "2" });
    paint();
    for (const fn of frames.splice(0)) fn();
    assert.deepEqual(ringsSeen.at(-1), []);

    // A full matrix refuses new routes.
    for (let m = 1; m <= 8; m++) push({ ["synth:l2_m" + m + "_src"]: "8", ["synth:l2_m" + m + "_dst"]: "1" });
    paint();
    for (const fn of frames.splice(0)) fn();
    assert.equal(model.slotFor(lfo1, "cutoff", true), 0);
}

{
    // Bulk apply: unthrottled, only changed keys, repaints, no history.
    const { store, sent } = harness();
    const seen = [];
    store.bind("cutoff", (v) => seen.push(v));
    let recorded = 0;
    store.recorder = () => recorded++;
    store.apply({ l1_cutoff: "64", l1_res: store.get("l1_res"), l2_cutoff: "10" });
    assert.deepEqual(sent.map(([k]) => k), ["synth:l1_cutoff", "synth:l2_cutoff"], "unchanged keys are skipped");
    assert.equal(sent[0][1], "64.000", "wire format applies");
    assert.equal(seen.at(-1), "64");
    assert.equal(recorded, 0, "apply is not an edit");
}

{
    // Undo / redo / A-B on the store.
    const { store, sent, push, paint, advance } = harness();
    const h = new History(store);
    const snap = snapshot(store);
    assert.ok("layer_count" in snap && "l4_m8_via_depth" in snap && !("edit_layer" in snap));
    const order = Object.keys(snap);
    assert.ok(order.indexOf("layer_count") < order.indexOf("l1_on"), "globals before layers");

    const b = store.bind("cutoff", () => {});
    b.set("50"); b.set("51"); b.set("52");
    assert.equal(h.undos.length, 1, "a drag is one step");
    assert.equal(h.describe(h.undos), "Cutoff (L1)");
    store.setBare("res", "0.5");
    assert.equal(h.undos.length, 2);
    h.undo();
    assert.equal(store.get("l1_res"), "0");
    h.undo();
    assert.equal(store.get("l1_cutoff"), "127", "back to the default");
    assert.ok(!h.canUndo && h.canRedo);
    h.redo();
    assert.equal(store.get("l1_cutoff"), "52");
    store.setBare("cutoff", "60");
    assert.ok(!h.canRedo, "a new edit clears redo");

    // A device-side bulk change (preset load) is undone from its snapshot.
    advance(WRITE_MS);            // the trailing write for 60 goes out...
    advance(ECHO_HOLD_MS + 1);    // ...and its echo hold expires
    h.bulk("Load X");
    push({ "synth:l1_cutoff": "20.000", "synth:l2_osc1_table": "9", "synth:preset_name": "X" });
    paint();
    sent.length = 0;
    h.undo();
    assert.equal(store.get("l1_cutoff"), "60");
    assert.equal(store.get("l2_osc1_table"), "0");
    assert.ok(sent.some(([k, v]) => k === "synth:preset_name"), "the name is restored too");
    h.redo();
    assert.equal(store.get("l2_osc1_table"), "9");

    // A/B: B starts as a copy, edits stay in their slot.
    h.switchTo("B");
    assert.equal(h.compare.active, "B");
    store.setBare("cutoff", "33");
    h.switchTo("A");
    assert.equal(store.get("l1_cutoff"), "20.000");
    h.switchTo("B");
    assert.equal(store.get("l1_cutoff"), "33");
    h.resetCompare();
    assert.equal(h.compare.active, "A");
    assert.equal(h.compare.slots.B, null);
}

console.log("web ui tests passed");
