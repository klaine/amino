import assert from "node:assert/strict";
import { route, defaultAssign, isPassthrough, ENGINE_CHANNEL } from "../../web/router.js";

const on = (ch, n = 60) => [0x90 | ch, n, 100];
const off = (ch, n = 60) => [0x80 | ch, n, 0];

// All omni: every layer hears every channel.
let a = defaultAssign();
assert.ok(isPassthrough(a));
assert.deepEqual(route(a, on(5)).map((m) => m[0]), [0x90, 0x91, 0x92, 0x93]);

// Channelled layers only hear their channel; engine channel is per layer.
a = [1, 2, "off", "omni"];
assert.deepEqual(route(a, on(0)), [[0x90 | 0, 60, 100], [0x90 | 3, 60, 100]], "ch1 -> layer 1 + omni layer 4");
assert.deepEqual(route(a, on(1)), [[0x90 | 1, 60, 100], [0x90 | 3, 60, 100]], "ch2 -> layer 2 + omni layer 4");
assert.deepEqual(route(a, on(9)), [[0x90 | 3, 60, 100]], "ch10 -> only the omni layer");
assert.equal(ENGINE_CHANNEL(2), 3);

// Note off follows the same targets as note on.
assert.deepEqual(route(a, off(0)), [[0x80 | 0, 60, 0], [0x80 | 3, 60, 0]]);

// Two layers on one channel both play.
a = [3, 3, "off", "off"];
assert.equal(route(a, on(2)).length, 2);
assert.equal(route(a, on(0)).length, 0);

// Non-note messages go out once, if any layer listens on that channel.
a = [1, 1, 2, "off"];
assert.deepEqual(route(a, [0xe0, 0, 64]), [[0xe0, 0, 64]], "bend on ch1 once");
assert.deepEqual(route(a, [0xb0, 1, 80]), [[0xb0, 1, 80]]);
assert.deepEqual(route(a, [0xb5, 1, 80]), [], "ch6: nobody listens");
// All Notes Off is per layer so it reaches the right engine channel.
assert.deepEqual(route(a, [0xb0, 123, 0]), [[0xb0 | 0, 123, 0], [0xb0 | 1, 123, 0]]);
// System and malformed messages are dropped.
assert.deepEqual(route(defaultAssign(), [0xf8, 0, 0]), []);
assert.deepEqual(route(defaultAssign(), [0x20, 0, 0]), []);
console.log("router tests passed");
