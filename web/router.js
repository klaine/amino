/* Optional MIDI channel -> layer routing, done in front of the engine.
 *
 * Each of the four layers is assigned "omni", "off" or a channel 1-16.  While
 * routing is on, the engine runs in its Channel layer mode with layer N
 * listening on engine channel N (see ENGINE_CHANNEL); this module turns one
 * incoming message into the messages for the layers it concerns.  A layer on
 * "omni" hears every channel, so omni and channelled layers can mix.
 *
 * Pure functions, no DOM: tests/web/test_router.mjs runs them under node.
 */
export const LAYERS = 4;
export const LAYER_MODE_CHAN = 4;    // params.py LAYER_MODES index of "Chan"
export const ENGINE_CHANNEL = (layer) => layer + 1;   // layer 0 -> midi_ch 1

export const defaultAssign = () => Array(LAYERS).fill("omni");

const matches = (assign, ch) => assign === "omni" || assign === ch + 1;

/** True when every layer is omni, i.e. routing would change nothing. */
export const isPassthrough = (assign) => assign.every((a) => a === "omni");

/**
 * Messages to send to the engine for one incoming MIDI message [status, d1, d2].
 * Note on/off, polyphonic pressure and All Notes/Sound Off go to each matching
 * layer on that layer's engine channel; everything else (CCs, bend, channel
 * pressure, program change) is sent once on engine channel 1 if any layer
 * listens on its channel.
 */
export function route(assign, msg) {
  const status = msg[0];
  if (status >= 0xf0 || status < 0x80) return [];
  const type = status & 0xf0;
  const ch = status & 0x0f;
  const hit = [];
  for (let i = 0; i < LAYERS; i++) if (matches(assign[i], ch)) hit.push(i);
  if (!hit.length) return [];
  const perLayer = type === 0x80 || type === 0x90 || type === 0xa0
    || (type === 0xb0 && (msg[1] === 120 || msg[1] === 123));
  if (perLayer) return hit.map((i) => [type | (ENGINE_CHANNEL(i) - 1), msg[1], msg[2]]);
  return [[type, msg[1], msg[2]]];
}
