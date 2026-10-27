/* Main-thread side of the engine: owns the AudioContext and the worklet node,
 * and wraps the worklet's message protocol (see worklet.js) in a small API. */

export class Engine {
  constructor(ctx, node) {
    this.ctx = ctx;
    this.node = node;
    this.pending = new Map();
    this.nextId = 1;
    this.onPeak = null;
    node.port.onmessage = (e) => {
      const d = e.data;
      if (d.t === "got") {
        const done = this.pending.get(d.id);
        this.pending.delete(d.id);
        if (done) done(d.val);
      } else if (d.t === "peak" && this.onPeak) this.onPeak(d.v);
    };
  }

  /** Must run inside a user gesture so the AudioContext may start. */
  static async start(base = "./") {
    const ctx = new AudioContext({ sampleRate: 44100, latencyHint: "interactive" });
    // In the single-file build (scripts/build_single.py) everything is embedded.
    const emb = globalThis.AMINO_EMBED;
    if (emb) await emb.ready;
    const [wasm] = await Promise.all([
      emb ? emb.wasm() : fetch(base + "amino.wasm").then((r) => {
        if (!r.ok) throw new Error("amino.wasm: HTTP " + r.status);
        return r.arrayBuffer();
      }),
      ctx.audioWorklet.addModule(emb ? emb.workletUrl : base + "worklet.js"),
    ]);
    const node = new AudioWorkletNode(ctx, "amino", {
      numberOfInputs: 0,
      outputChannelCount: [2],
      processorOptions: { wasm },
    });
    const ready = new Promise((resolve) => {
      node.port.onmessage = (e) => { if (e.data.t === "ready") resolve(e.data); };
    });
    const info = await ready;
    if (!info.ok) throw new Error("engine failed to start: " + (info.error || "wa_init"));
    node.connect(ctx.destination);
    if (ctx.state !== "running") await ctx.resume();
    return new Engine(ctx, node);
  }

  set(key, val) { this.node.port.postMessage({ t: "set", key, val: String(val) }); }

  get(key) {
    const id = this.nextId++;
    return new Promise((resolve) => {
      this.pending.set(id, resolve);
      this.node.port.postMessage({ t: "get", id, key });
    });
  }

  midi(m) { this.node.port.postMessage({ t: "midi", m }); }

  tempo(bpm, run) { this.node.port.postMessage({ t: "tempo", bpm, run }); }
}
