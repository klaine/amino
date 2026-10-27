import { createController, LAYOUT_MOVY } from "/data/UserData/schwung/shared/param_pages/page_controller.mjs";
import { applyInput, decodeInput } from "/data/UserData/schwung/shared/param_pages/page_input.mjs";
import {
    drawMatrixPage,
    matrixPadAddress,
    matrixPadColors,
    MATRIX_ROW_SOURCE,
    MATRIX_ROW_VIA,
    MATRIX_ROW_TARGET,
    MATRIX_ROW_VIA_DEPTH,
} from "./mod_matrix_ui.mjs";

const PAGE_PREFIX = "synth:";
const PAD_FIRST = 68;
const PAD_LAST = 99;
const NOTE_ON = 0x90;
const NOTE_OFF = 0x80;
const MATRIX_KEY = "mod_matrix";
const MATRIX_STATE_KEY = "mod_matrix_state";
const TURN_SHOW_MS = 1200;        // a turned knob's bar stays up this long

function pixelLine(x0, y0, x1, y1, value = 1) {
    x0 = Math.round(x0); y0 = Math.round(y0);
    x1 = Math.round(x1); y1 = Math.round(y1);
    const dx = Math.abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const dy = -Math.abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    let err = dx + dy;
    for (;;) {
        fill_rect(x0, y0, 1, 1, value);
        if (x0 === x1 && y0 === y1) break;
        const e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

function pixelArc(cx, cy, radius, start, sweep, value = 1) {
    const count = Math.max(1, Math.ceil(Math.abs(sweep) * Math.max(1, radius) / 4));
    let lastX = null, lastY = null;
    for (let i = 0; i <= count; i++) {
        const angle = (start + sweep * i / count) * Math.PI / 180;
        const x = cx + Math.sin(angle) * radius;
        const y = cy - Math.cos(angle) * radius;
        if (lastX !== null) pixelLine(lastX, lastY, x, y, value);
        lastX = x; lastY = y;
    }
}

const drawCtx = {
    fillRect: fill_rect,
    print,
    textWidth: text_width,
    line: typeof draw_line === "function" ? draw_line : pixelLine,
    drawArc: typeof draw_arc === "function" ? draw_arc : pixelArc,
    drawRect: typeof draw_rect === "function" ? draw_rect : undefined,
    setPixel: typeof set_pixel === "function" ? set_pixel : (x, y, v) => fill_rect(x, y, 1, 1, v),
    drawCircle: typeof draw_circle === "function" ? draw_circle : undefined,
    fillCircle: typeof fill_circle === "function" ? fill_circle : undefined,
};

let controller = null;
let uiError = null;
let focus = null;                 // { slot, row, held }
let viewSlot = 0;                 // matrix column on screen: last pad or knob
let lastTurn = null;              // { slot, via, at } for the amount bar
let matrixSnapshot = null;
let matrixSnapshotRaw = null;
let matrixEditUntil = 0;
let matrixLedsActive = false;
let nativePadLeds = null;
let padLedCache = Object.create(null);
let matrixLedCursor = 0;
let pendingWrites = Object.create(null);
let lastWriteAt = Object.create(null);

function localKey(fullKey) {
    const key = String(fullKey || "");
    return key.indexOf(PAGE_PREFIX) === 0 ? key.slice(PAGE_PREFIX.length) : key;
}

function getParam(fullKey) {
    let key = localKey(fullKey);
    // Sound generators use ui_hierarchy to select the host's editor. Amino's
    // custom UI uses the same contract as ui_pages so it can own the pads.
    if (key === "ui_hierarchy") key = "ui_pages";
    return host_module_get_param(key);
}

function setParam(fullKey, value) {
    return host_module_set_param(localKey(fullKey), String(value));
}

function announce(text) {
    if (typeof host_announce_screenreader === "function")
        host_announce_screenreader(String(text));
}

function parseSnapshot(raw) {
    if (raw && typeof raw === "object") return raw;
    if (typeof raw !== "string" || !raw) return null;
    try {
        const parsed = JSON.parse(raw);
        return parsed && Array.isArray(parsed.routes) ? parsed : null;
    } catch (_) {
        return null;
    }
}

function adoptSnapshot(raw) {
    if (typeof raw === "string" && raw === matrixSnapshotRaw) return;
    const incoming = parseSnapshot(raw);
    if (!incoming) return;
    const now = Date.now();
    if (matrixSnapshot && now < matrixEditUntil) return;
    if (typeof raw === "string") matrixSnapshotRaw = raw;
    matrixSnapshot = incoming;
}

function sourceOptions() {
    const meta = controller && controller.metaIndex && controller.metaIndex.getOrGuess("m1_src");
    return meta && Array.isArray(meta.options) ? meta.options : ["Off"];
}

function targetOptions() {
    const meta = controller && controller.metaIndex && controller.metaIndex.getOrGuess("m1_dst");
    return meta && Array.isArray(meta.options) ? meta.options : ["Off"];
}

function lfoShapeOptions() {
    const meta = controller && controller.metaIndex && controller.metaIndex.getOrGuess("lfo1_shape");
    return meta && Array.isArray(meta.options) ? meta.options : [];
}

function lfoDivisionOptions() {
    const meta = controller && controller.metaIndex && controller.metaIndex.getOrGuess("lfo1_div");
    return meta && Array.isArray(meta.options) ? meta.options : [];
}

function isMatrixPage() {
    const page = controller && controller.page;
    return !!(page && page.canvas && page.canvas.key === MATRIX_KEY);
}

function drawCanvasPage(ctx, band, canvas, data) {
    if (!canvas || canvas.key !== MATRIX_KEY) return;
    adoptSnapshot(data && data.values && data.values[MATRIX_STATE_KEY]);
    const turn = lastTurn && Date.now() - lastTurn.at < TURN_SHOW_MS ? lastTurn : null;
    drawMatrixPage(ctx, band, matrixSnapshot, focus, {
        slot: viewSlot,
        turn,
        sources: sourceOptions(),
        targets: targetOptions(),
        shapes: lfoShapeOptions(),
        divisions: lfoDivisionOptions(),
    });
}

function createUiController() {
    const io = {
        getParam,
        setParam,
        announce,
        drawCanvasPage,
        trailingMenus: () => {
            try {
                return typeof shadow_component_trailing_menus === "function"
                    ? shadow_component_trailing_menus() : [];
            } catch (_) { return []; }
        },
        loadCard: (script, ref) => {
            if (!script || !ref || typeof host_load_ui_module !== "function") return null;
            const file = String(script).split("#")[0];
            const path = "/data/UserData/schwung/modules/sound_generators/amino/" + file;
            try {
                if (!host_load_ui_module(path)) return null;
                return typeof globalThis[ref] === "function" ? globalThis[ref] : null;
            } catch (_) { return null; }
        },
    };
    const ctl = createController(io);
    ctl.setLayout(LAYOUT_MOVY);
    let slot = 0;
    try {
        if (typeof shadow_get_selected_slot === "function")
            slot = Math.max(0, Number(shadow_get_selected_slot()) | 0);
    } catch (_) { slot = 0; }
    ctl.load({ slot, component: "synth", prefix: "synth" });
    if (!Array.isArray(ctl.pages) ||
        !ctl.pages.some((page) => page.canvas && page.canvas.key === MATRIX_KEY))
        throw new Error("Pad Matrix missing from ui_pages");
    return ctl;
}

function sendPadLed(note, color, force = false) {
    if (!force && padLedCache[note] === color) return;
    if (typeof move_midi_internal_send === "function")
        move_midi_internal_send([0x09, NOTE_ON, note, color]);
    padLedCache[note] = color;
}

function beginMatrixLeds() {
    if (matrixLedsActive) return;
    matrixLedsActive = true;
    padLedCache = Object.create(null);
    matrixLedCursor = 0;
    nativePadLeds = null;
    try {
        if (typeof shadow_get_pad_led_snapshot === "function")
            nativePadLeds = shadow_get_pad_led_snapshot();
    } catch (_) { nativePadLeds = null; }
}

function restoreMatrixLeds() {
    if (!matrixLedsActive) return;
    let snapshot = nativePadLeds || {};
    try {
        if (typeof shadow_get_pad_led_snapshot === "function")
            snapshot = shadow_get_pad_led_snapshot() || snapshot;
    } catch (_) { /* restore the entry snapshot if the live read fails */ }
    for (let note = PAD_FIRST; note <= PAD_LAST; note++) {
        const value = snapshot[String(note)];
        sendPadLed(note, Number.isFinite(Number(value)) ? Number(value) : 0, true);
    }
    matrixLedsActive = false;
    nativePadLeds = null;
}

function updateMatrixLeds() {
    if (!matrixSnapshot) return;
    beginMatrixLeds();
    const colors = matrixPadColors(matrixSnapshot);
    // Move may repaint its own pads while the UI is open. Reassert a small
    // slice each tick (8 LEDs, below the MIDI-out packet budget) rather than
    // blasting the whole grid or assuming our last write still owns the LED.
    for (let n = 0; n < 8; n++) {
        const i = (matrixLedCursor + n) % 32;
        sendPadLed(PAD_FIRST + i, colors[i], true);
    }
    matrixLedCursor = (matrixLedCursor + 8) % 32;
}

function injectPadToMove(data) {
    if (typeof move_midi_inject_to_move !== "function") return;
    const type = data[0] & 0xf0;
    const packetType = type === NOTE_ON ? 0x09 : 0x08;
    move_midi_inject_to_move([packetType, data[0], data[1], data[2]]);
}

function rememberEdit() {
    matrixEditUntil = Date.now() + 180;
}

/* A knob turn shows its own column. A selected field on another column is
 * dropped, so the jog never edits a slot that is not on screen. */
function noteTurn(slot, via) {
    if (focus && focus.slot !== slot && !focus.held) focus = null;
    viewSlot = slot;
    lastTurn = { slot, via, at: Date.now() };
}

function editEnum(intent) {
    if (!focus || !matrixSnapshot || !Array.isArray(matrixSnapshot.routes)) return false;
    const field = focus.row === MATRIX_ROW_SOURCE ? 0
                : focus.row === MATRIX_ROW_TARGET ? 1
                : focus.row === MATRIX_ROW_VIA ? 3 : -1;
    if (field < 0) return false;
    const route = matrixSnapshot.routes[focus.slot];
    if (!route) return false;
    const key = field === 0 ? `m${focus.slot + 1}_src`
              : field === 1 ? `m${focus.slot + 1}_dst`
              : `m${focus.slot + 1}_via`;
    const choices = field === 1 ? targetOptions() : sourceOptions();
    if (!choices.length) return false;
    const next = (Math.round(Number(route[field]) || 0) + intent.delta + choices.length) % choices.length;
    pendingWrites[key] = String(next);
    route[field] = next;
    rememberEdit();
    return true;
}

function queueAmount(slot, direction, fine) {
    if (!matrixSnapshot || !matrixSnapshot.routes || !matrixSnapshot.routes[slot]) return;
    const route = matrixSnapshot.routes[slot];
    const step = fine ? 0.001 : 0.01;
    const value = Math.max(-1, Math.min(1, (Number(route[2]) || 0) + direction * step));
    route[2] = value;
    pendingWrites[`m${slot + 1}_amt`] = value.toFixed(3);
    rememberEdit();
    noteTurn(slot, false);
}

function queueViaDepth(slot, direction, fine) {
    if (!matrixSnapshot || !matrixSnapshot.routes || !matrixSnapshot.routes[slot]) return;
    const route = matrixSnapshot.routes[slot];
    const step = fine ? 0.001 : 0.01;
    const value = Math.max(0, Math.min(1, (Number(route[4]) || 0) + direction * step));
    route[4] = value;
    pendingWrites[`m${slot + 1}_via_depth`] = value.toFixed(3);
    rememberEdit();
    noteTurn(slot, true);
}

function flushPendingWrites(force = false) {
    const now = Date.now();
    for (const key of Object.keys(pendingWrites)) {
        if (!force && now - (lastWriteAt[key] || 0) < 20) continue;
        const value = pendingWrites[key];
        if (setParam(key, value)) {
            lastWriteAt[key] = now;
            delete pendingWrites[key];
        }
        // Keep each frame to at most one synchronous parameter write.
        if (!force) break;
    }
}

function takePendingAction(action) {
    if (!action || action.action !== "menu") return;
    const name = action.entry && action.entry.action;
    if (name && typeof shadow_component_run_action === "function")
        shadow_component_run_action(name);
}

function midiNoteEvent(data) {
    if (!data || data.length < 3) return false;
    const status = data[0] & 0xf0;
    return (status === NOTE_ON || status === NOTE_OFF) &&
           data[1] >= PAD_FIRST && data[1] <= PAD_LAST;
}

function handlePad(data) {
    const address = matrixPadAddress(data[1]);
    if (!address) return;
    const isDown = (data[0] & 0xf0) === NOTE_ON && Number(data[2]) > 0;
    if (!isMatrixPage()) {
        injectPadToMove(data);
        return;
    }
    if (isDown) {
        focus = { slot: address.slot, row: address.row, held: true };
        viewSlot = address.slot;
        lastTurn = null;
    } else if (focus && focus.slot === address.slot && focus.row === address.row) {
        focus = { slot: focus.slot, row: focus.row, held: false };
    }
}

function isShiftHeld() {
    try {
        return typeof shadow_get_shift_held === "function" && shadow_get_shift_held() === 1;
    } catch (_) { return false; }
}

function handleMidi(data) {
    if (!data || data.length < 3) return;
    if (midiNoteEvent(data)) {
        handlePad(data);
        return;
    }

    const rawStatus = data[0] & 0xf0;
    if (rawStatus === 0xb0 && data[2] > 0 &&
        ((data[1] >= 40 && data[1] <= 43) || data[1] === 50)) {
        // These remain Move-owned exit/track controls. Restore our LED overlay
        // before Move changes the selected track or dismisses the editor.
        focus = null;
        restoreMatrixLeds();
    }

    const intent = decodeInput(data, { shift: isShiftHeld() });
    if (!intent) return;

    if (isMatrixPage()) {
        if (intent.type === "page" && !intent.byLevel && focus &&
            focus.row >= MATRIX_ROW_SOURCE && focus.row <= MATRIX_ROW_TARGET && editEnum(intent))
            return;
        if (intent.type === "knob" && focus && focus.row === MATRIX_ROW_VIA_DEPTH &&
            intent.slot === focus.slot) {
            queueViaDepth(intent.slot, intent.direction, intent.fine);
            return;
        }
        if (intent.type === "knob") {
            queueAmount(intent.slot, intent.direction, intent.fine);
            return;
        }
    }

    takePendingAction(applyInput(controller, intent, { nowMs: Date.now() }));
}

function handleBack() {
    if (isMatrixPage() && focus) {
        focus = null;
        return true;
    }
    if (controller) {
        if (controller.dismissHint && controller.dismissHint()) return true;
        if (controller.dismissPeek && controller.dismissPeek()) return true;
        if (controller.pickerOpen) { controller.closePicker(); return true; }
        if (controller.exitMenu && controller.exitMenu()) return true;
    }
    flushPendingWrites(true);
    restoreMatrixLeds();
    return false;
}

function init() {
    focus = null;
    viewSlot = 0;
    lastTurn = null;
    matrixSnapshot = null;
    matrixSnapshotRaw = null;
    uiError = null;
    try {
        controller = createUiController();
    } catch (error) {
        controller = null;
        uiError = String(error && error.message ? error.message : error);
        try { console.log("Amino ui_chain init failed: " + uiError); } catch (_) {}
    }
}

function drawUiError() {
    clear_screen();
    print(1, 3, "Amino Matrix UI error", 1);
    print(1, 16, uiError ? uiError.slice(0, 20) : "Controller unavailable", 1);
    print(1, 47, "Back: exit", 1);
}

function tick() {
    // The host normally reconciles this before invoking the module. Restate it
    // here too so pads stay exclusive on hosts with older reconciliation logic.
    if (typeof host_pad_block === "function") host_pad_block(1);
    if (!controller) {
        drawUiError();
        return;
    }
    try {
        flushPendingWrites(false);
        controller.tick();

        const matrixPage = isMatrixPage();
        if (matrixPage) {
            adoptSnapshot(controller.state.values[MATRIX_STATE_KEY]);
            if (matrixSnapshot && controller.state.values) {
                for (let i = 0; i < 8; i++) {
                    const amount = Number(controller.state.values[`m${i + 1}_amt`]);
                    if (Number.isFinite(amount) && matrixSnapshot.routes[i])
                        matrixSnapshot.routes[i][2] = amount;
                }
            }
            updateMatrixLeds();
        } else {
            restoreMatrixLeds();
        }

        clear_screen();
        const enumFocus = focus && focus.row >= MATRIX_ROW_SOURCE && focus.row <= MATRIX_ROW_TARGET;
        const footer = matrixPage
            ? [["PAD", "FIELD"], ["JOG", enumFocus ? "VALUE" : "PAGE"],
               ["KNOB", focus && focus.row === MATRIX_ROW_VIA_DEPTH ? "VIA" : "AMT"]]
            : undefined;
        controller.render(drawCtx, { title: "Amino", footer });
        controller.renderOverlays(drawCtx, { clearScreen: clear_screen });
    } catch (error) {
        uiError = String(error && error.message ? error.message : error);
        try { console.log("Amino ui_chain frame failed: " + uiError); } catch (_) {}
        drawUiError();
    }
}

globalThis.chain_ui = {
    init,
    tick,
    onMidiMessageInternal: handleMidi,
    handleBack,
    onPresetsChanged: () => { if (controller) controller.refreshTrailing(); },
    restorePage: (name, opts) => { if (controller) controller.restorePage(name, opts || {}); },
};
