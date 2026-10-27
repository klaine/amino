import assert from "node:assert/strict";
import {
    drawMatrixPage,
    matrixPadAddress,
    matrixPadColors,
} from "../../src/mod_matrix_ui.mjs";

assert.deepEqual(matrixPadAddress(92), { slot: 0, row: 0 });
assert.deepEqual(matrixPadAddress(99), { slot: 7, row: 0 });
assert.deepEqual(matrixPadAddress(84), { slot: 0, row: 1 });
assert.deepEqual(matrixPadAddress(76), { slot: 0, row: 2 });
assert.deepEqual(matrixPadAddress(68), { slot: 0, row: 3 });
assert.equal(matrixPadAddress(67), null);
assert.equal(matrixPadAddress(100), null);

const snapshot = {
    layer: 2,
    routes: [
        [4, 9, 0.8, 13, 0.5],
        [8, 12, -0.4, 0, 1],
        ...Array.from({ length: 6 }, () => [0, 0, 0, 0, 1]),
    ],
    lfos: [[1, 0.4, 0, 7], [2, 0.2, 1, 5]],
    envs: [[0, 0.5, 0.8, 0.2], [0.1, 0.4, 0.2, 0.3], [0, 0.3, 1, 0.25]],
};
const colors = matrixPadColors(snapshot);
assert.equal(colors[24], 16, "source row lights for an assigned source");
assert.equal(colors[8], 11, "positive depth uses a bright green target LED");
assert.equal(colors[16], 95, "Via row distinguishes an assigned scaler");
assert.equal(colors[0], 95, "Via Depth intensity is visible on the bottom row");
assert.equal(colors[9], 65, "negative depth uses a red magnitude shade");
assert.equal(colors[10], 0, "empty slots stay dark");

const calls = [];
const ctx = {
    fillRect: (...args) => calls.push(["fill", ...args]),
    print: (...args) => calls.push(["print", ...args]),
};
drawMatrixPage(ctx, { w: 128, h: 44 }, snapshot,
    { slot: 0, row: 0, held: true }, {
        sources: ["Off", "Wave Env", "Filter Env", "Amp Env", "LFO1", "LFO2"],
        targets: ["Off", "Pitch", "Cutoff"],
        shapes: ["Sine", "Tri", "Pulse"],
        divisions: ["1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4"],
    });
assert(calls.some((c) => c[0] === "print" && String(c[3]).includes("LFO")),
    "held source pad displays route context");
const options = {
    sources: ["Off", "Wave Env", "Filter Env", "Amp Env", "LFO1", "LFO2", "LFO1xMW",
              "LFO2xMW", "Velocity", "Release Vel", "Key", "Poly AT", "Chan AT", "ModWheel"],
    targets: ["Off", "Pitch", "Osc1 Pitch", "Osc2 Pitch", "Osc1 Wave", "Osc2 Wave",
              "Osc1 Level", "Osc2 Level", "Dirt Level", "Cutoff", "Resonance", "Drive", "Pan"],
};
function render(focus, extra = {}) {
    const out = [];
    drawMatrixPage({
        fillRect: (...args) => out.push(["fill", ...args]),
        print: (...args) => out.push(["print", ...args]),
    }, { w: 128, h: 44 }, snapshot, focus, { ...options, ...extra });
    return out;
}
const texts = (out) => out.filter((c) => c[0] === "print").map((c) => String(c[3]));

// Text rows sit at least one glyph height (8 px) apart and inside the band,
// in every state: the old hint at h-9 collided with the row at y=31.
const states = [
    [null, {}], [{ slot: 0, row: 0, held: false }, {}],
    [{ slot: 0, row: 3, held: false }, {}], [{ slot: 0, row: 2, held: true }, {}],
    [{ slot: 0, row: 3, held: true }, {}], [null, { turn: { slot: 1, via: false } }],
    [null, { turn: { slot: 0, via: true } }],
];
for (const [focus, extra] of states) {
    const ys = render(focus, extra).filter((c) => c[0] === "print").map((c) => c[2]).sort((a, b) => a - b);
    for (let i = 1; i < ys.length; i++)
        assert(ys[i] - ys[i - 1] >= 8, `text rows overlap at y=${ys[i - 1]}/${ys[i]} (${JSON.stringify(focus)})`);
    assert(ys[ys.length - 1] + 8 <= 44, "text stays inside the body band");
}

// A turned knob shows its own slot, with the amount as a bar and a number.
let out = render({ slot: 0, row: 0, held: false }, { slot: 1, turn: { slot: 1, via: false } });
assert(texts(out).some((t) => t.includes("M2 AMOUNT")), "turn view names the turned slot");
assert(texts(out).some((t) => t.includes("Velocity > Pan")), "turn view shows that slot's route");
assert(texts(out).some((t) => t.startsWith("-40.0%")), "turn view prints the signed amount");
assert(out.some((c) => c[0] === "fill" && c[2] === 24 && c[4] === 3 && c[1] < 64 && c[1] + c[3] <= 64),
    "negative amount fills the bar left of centre");
out = render(null, { turn: { slot: 0, via: true } });
assert(texts(out).some((t) => t.includes("VIA DEPTH")), "Via depth turn is labelled");
assert(texts(out).some((t) => t === "Via depth 50%"), "Via depth turn prints the depth");
// Without a turn the last-touched slot stays on screen.
out = render(null, { slot: 1 });
assert(texts(out).some((t) => t.includes("M2 AMOUNT")), "last turned slot stays shown");
assert(!texts(out).some((t) => t.includes("KNOB") || t.includes("HOLD")),
    "control hints live in the footer, not the body");
console.log("Pad Matrix UI helpers: OK");
