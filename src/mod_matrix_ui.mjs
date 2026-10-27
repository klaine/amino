/* Pad/knob modulation-matrix drawing and pure helpers. */

export const MATRIX_ROW_SOURCE = 0;
export const MATRIX_ROW_VIA = 1;
export const MATRIX_ROW_TARGET = 2;
export const MATRIX_ROW_VIA_DEPTH = 3;

export function matrixPadAddress(note) {
    const n = Number(note) - 68;
    if (!Number.isInteger(n) || n < 0 || n >= 32) return null;
    return { slot: n % 8, row: 3 - Math.floor(n / 8) };
}

function amountColor(value, active) {
    if (!active || !Number.isFinite(value) || Math.abs(value) < 0.005) return 0;
    const magnitude = Math.abs(value);
    if (value > 0) return magnitude < 0.34 ? 86 : magnitude < 0.67 ? 85 : 11;
    return magnitude < 0.34 ? 66 : magnitude < 0.67 ? 65 : 1;
}

function viaColor(depth, active) {
    if (!active) return 0;
    if (depth < 0.34) return 96;
    if (depth < 0.67) return 95;
    return 16;
}

export function matrixPadColors(snapshot) {
    const colors = new Array(32).fill(0);
    const routes = snapshot && Array.isArray(snapshot.routes) ? snapshot.routes : [];
    for (let slot = 0; slot < 8; slot++) {
        const r = routes[slot] || [];
        const active = Number(r[0]) > 0 && Number(r[1]) > 0;
        const via = Number(r[3]) > 0;
        const depth = Number(r[4]);
        const amount = Number(r[2]);
        // MIDI note order rises from the bottom-left; rows in the UI are top-down.
        colors[24 + slot] = Number(r[0]) > 0 ? 16 : 0;
        colors[16 + slot] = via ? viaColor(depth, true) : 0;
        colors[8 + slot] = amountColor(amount, active);
        colors[slot] = viaColor(depth, via);
    }
    return colors;
}

function option(options, index, fallback) {
    return Array.isArray(options) && index >= 0 && index < options.length
        ? String(options[index]) : fallback;
}

function short(text, width) {
    const s = String(text || "");
    return s.length > width ? s.slice(0, Math.max(0, width - 3)) + "..." : s;
}

function envSeconds(v) {
    const x = Math.max(0, Math.min(1, Number(v) || 0));
    return Math.max(0.0005, 20 * x * x * x);
}

function fmtSeconds(v) {
    const seconds = envSeconds(v);
    return seconds < 1 ? Math.round(seconds * 1000) + "ms" : seconds.toFixed(1) + "s";
}

function lfoInfo(snapshot, source, options) {
    const n = source === 4 || source === 6 ? 0 : 1;
    const lfo = snapshot && snapshot.lfos && snapshot.lfos[n];
    if (!lfo) return "LFO settings unavailable";
    const shape = option(options.shapes, Number(lfo[0]), "Shape");
    const sync = Number(lfo[2]) > 0;
    if (sync) return shape + "  " + option(options.divisions, Number(lfo[3]), "Sync");
    const rate = Math.max(0, Math.min(1.2, Number(lfo[1]) || 0));
    const hz = 0.01 * Math.pow(5000, rate);
    return shape + "  " + (hz < 10 ? hz.toFixed(1) : hz.toFixed(0)) + " Hz";
}

function sourceInfo(snapshot, source, options) {
    if (source === 4 || source === 5 || source === 6 || source === 7)
        return lfoInfo(snapshot, source, options);
    if (source >= 1 && source <= 3) {
        const env = snapshot && snapshot.envs && snapshot.envs[source - 1];
        if (!env) return "Envelope settings unavailable";
        return "A " + fmtSeconds(env[0]) + " D " + fmtSeconds(env[1]) +
               " S " + Math.round(Number(env[2]) * 100) + "% R " + fmtSeconds(env[3]);
    }
    return "No extra source settings";
}

function frame(ctx, x, y, w, h) {
    ctx.fillRect(x, y, w, 1, 1);
    ctx.fillRect(x, y + h - 1, w, 1, 1);
    ctx.fillRect(x, y, 1, h, 1);
    ctx.fillRect(x + w - 1, y, 1, h, 1);
}

/* Amount bar, filled from the centre tick (bipolar) or the left edge. */
function drawBar(ctx, x0, y, w, h, value, bipolar) {
    frame(ctx, x0, y, w, h);
    const v = Math.max(bipolar ? -1 : 0, Math.min(1, Number(value) || 0));
    if (!bipolar) {
        const fill = Math.round(v * (w - 4));
        if (fill > 0) ctx.fillRect(x0 + 2, y + 2, fill, h - 4, 1);
        return;
    }
    const mid = x0 + Math.floor(w / 2);
    ctx.fillRect(mid, y - 1, 1, h + 2, 1);
    const len = Math.round(v * Math.floor((w - 4) / 2));
    if (len > 0) ctx.fillRect(mid + 1, y + 2, len, h - 4, 1);
    else if (len < 0) ctx.fillRect(mid + len, y + 2, -len, h - 4, 1);
}

function pct(v, signed) {
    return (signed && v >= 0 ? "+" : "") + (v * 100).toFixed(signed ? 1 : 0) + "%";
}

/**
 * Draws only the canvas-page body; the shared param-page renderer owns chrome,
 * including the footer that names what PAD / JOG / KNOB do.
 *
 * `options.slot` is the column on screen (the last pad pressed or knob
 * turned); `options.turn` ({ slot, via }) marks a knob turned just now, which
 * shows that slot's amount (or Via depth) as a bar until it expires.
 */
export function drawMatrixPage(ctx, band, snapshot, focus, options = {}) {
    const w = Math.max(1, Number(band && band.w) || 128);
    const h = Math.max(1, Number(band && band.h) || 44);
    ctx.fillRect(0, 0, w, h, 0);

    if (!snapshot || !Array.isArray(snapshot.routes)) {
        ctx.print(2, 3, "Loading mod matrix", 1);
        return;
    }

    const turn = options.turn || null;
    const shown = turn ? turn.slot : Number.isInteger(options.slot) ? options.slot
                : focus ? focus.slot : 0;
    const slot = Math.max(0, Math.min(7, shown));
    const row = focus && focus.slot === slot ? focus.row : -1;
    const r = snapshot.routes[slot] || [0, 0, 0, 0, 1];
    const src = Number(r[0]) || 0;
    const dst = Number(r[1]) || 0;
    const via = Number(r[3]) || 0;
    const amount = Number(r[2]) || 0;
    const viaDepth = Number(r[4]);
    const sourceOptions = options.sources || [];
    const targetOptions = options.targets || [];
    const fieldNames = ["SOURCE", "VIA", "TARGET", "VIA DEPTH"];
    const field = turn ? (turn.via ? "VIA DEPTH" : "AMOUNT")
                : row >= 0 ? fieldNames[row] : "AMOUNT";
    ctx.print(1, 1, "L" + (snapshot.layer || 1) + " M" + (slot + 1) + " " + field, 1);
    ctx.print(1, 11, short(option(sourceOptions, src, "Off"), 8) + " > " +
             short(option(targetOptions, dst, "Off"), 8), 1);

    const held = focus && focus.held && !turn && row >= 0;
    if (held && (row === MATRIX_ROW_SOURCE || row === MATRIX_ROW_VIA)) {
        const selectedSource = row === MATRIX_ROW_SOURCE ? src : via;
        const label = row === MATRIX_ROW_SOURCE ? "SRC " : "VIA ";
        ctx.print(1, 21, label + short(option(sourceOptions, selectedSource, "Off"), 18), 1);
        ctx.print(1, 31, sourceInfo(snapshot, selectedSource, options), 1);
    } else if (held && row === MATRIX_ROW_TARGET) {
        ctx.print(1, 21, "Amount " + pct(amount, true), 1);
        ctx.print(1, 31, via ? "Via depth " + pct(viaDepth) : "Via Off", 1);
    } else if (held && row === MATRIX_ROW_VIA_DEPTH) {
        ctx.print(1, 21, via ? "Via scales route amount" : "Choose a Via source first", 1);
        ctx.print(1, 31, "Via depth " + pct(viaDepth), 1);
    } else if (turn || row < 0) {
        // The knob's own reading: a bar, then the number under it.
        const showVia = !!(turn && turn.via);
        drawBar(ctx, 1, 22, w - 2, 7, showVia ? viaDepth : amount, !showVia);
        const text = showVia ? (via ? "Via depth " + pct(viaDepth) : "Via Off")
                   : pct(amount, true) + (via ? "  Via " + pct(viaDepth) : "");
        ctx.print(1, 32, text, 1);
    } else {
        const value = row === MATRIX_ROW_SOURCE ? option(sourceOptions, src, "Off")
                    : row === MATRIX_ROW_VIA ? option(sourceOptions, via, "Off")
                    : row === MATRIX_ROW_TARGET ? option(targetOptions, dst, "Off")
                    : pct(viaDepth);
        ctx.print(1, 21, short(value, 20), 1);
        ctx.print(1, 31, "Amt " + pct(amount, true) +
                 "  Via " + (via ? pct(viaDepth) : "Off"), 1);
    }
}
