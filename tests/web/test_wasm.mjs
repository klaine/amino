/* Smoke test for build/amino.wasm, driven the way web/worklet.js drives it. */
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

const bytes = readFileSync(new URL("../../build/amino.wasm", import.meta.url));
const module = new WebAssembly.Module(bytes);
assert.deepEqual(WebAssembly.Module.imports(module), [], "the module needs no host imports");
const x = new WebAssembly.Instance(module, {}).exports;
x._initialize();
assert.equal(x.wa_init(44100), 1, "engine starts");

const enc = new TextEncoder(), dec = new TextDecoder();
const put = (ptr, s) => { const b = enc.encode(s); new Uint8Array(x.memory.buffer).set(b, ptr); new Uint8Array(x.memory.buffer)[ptr + b.length] = 0; };
const get = (k) => { put(x.wa_key(), k); const n = x.wa_get(); return n < 0 ? null : dec.decode(new Uint8Array(x.memory.buffer, x.wa_out(), n)); };
const set = (k, v) => { put(x.wa_key(), k); put(x.wa_val(), v); x.wa_set(); };
const render = (blocks) => {
    let sum = 0, peak = 0;
    for (let b = 0; b < blocks; b++) {
        x.wa_render(128);
        for (const s of new Int16Array(x.memory.buffer, x.wa_pcm(), 256)) { sum += s * s; peak = Math.max(peak, Math.abs(s)); }
    }
    return { rms: Math.sqrt(sum / (blocks * 256)), peak };
};

assert.equal(get("preset_count"), "324");
assert.ok(JSON.parse(get("chain_params")).length > 100, "chain_params is JSON");
assert.equal(get("no_such_key"), null);
assert.equal(render(20).peak, 0, "silent without notes");

x.wa_midi(0x90, 48, 100);
const note = render(200);
assert.ok(note.rms > 500 && note.peak < 32768, "a note sounds: " + JSON.stringify(note));

// Same bank, same engine: the Init patch level matches the native test (~3340 rms).
assert.ok(Math.abs(note.rms - 3340) < 400, "init level close to native: " + note.rms);

// State round trip, including the new Aux envelope and Ladder 24dB filter.
set("l1_ftype", "2");
set("l1_e4_d", "0.7");
const state = get("state");
assert.ok(state.includes("l1_e4_d"), "state carries the aux envelope");
set("preset", "5");
set("state", state);
assert.equal(get("l1_ftype"), "2");
assert.ok(JSON.parse(get("state")).l1_e4_d !== undefined);

// Layer channel routing as the page uses it: Chan mode, layer 2 on channel 2.
set("layer_mode", "4");
set("layer_count", "2");
set("l2_on", "1");
set("l1_midi_ch", "1");
set("l2_midi_ch", "2");
x.wa_midi(0x80, 48, 0);
render(100);
x.wa_midi(0x91, 60, 100);
assert.ok(get("rui_live").startsWith("v=1,0,1,"), "a ch2 note reaches layer 2 only: " + get("rui_live"));

console.log("wasm tests passed (rms " + note.rms.toFixed(0) + ")");
