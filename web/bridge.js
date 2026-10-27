/* The schwungRemote side of the page.  The Remote UI in the iframe talks to
 * `window.parent.aminoBridge` through web/schwung-remote-api.js; this class
 * answers from the engine in the worklet, mirroring the Python harness
 * (tools/remote_ui/dev_server.py): a full value burst, then only what moved,
 * with the viz "extra keys" (rui_live, wave_view...) pumped on a timer.
 */
const EXTRA_KEYS = ["preset", "preset_name", "preset_count", "edit_layer"];
const EXTRA_THROTTLE_MS = 150;
const EXTRA_HEARTBEAT_MS = 500;
const SAVE_KEY = "amino.state.v1";

export class Bridge {
  constructor(engine, base = "./") {
    this.engine = engine;
    this.base = base;
    this.values = {};
    this.listeners = new Set();
    this.extraKeys = [];
    this.extras = {};
    this.refreshing = null;
    this.dirty = false;
    this.touched = false;
    this.saveTimer = 0;
  }

  async init() {
    try {
      const saved = localStorage.getItem(SAVE_KEY);
      if (saved) this.engine.set("state", saved);
    } catch (_) { /* storage blocked: start from the Init patch */ }
    this.chainParams = JSON.parse((await this.engine.get("chain_params")) || "null");
    for (const p of this.chainParams || []) {
      for (const k of (p.viz && p.viz.extra_keys) || []) {
        if (!this.extraKeys.includes(k)) this.extraKeys.push(k);
      }
    }
    await this.refresh();
    this.pump();
  }

  async snapshot() {
    const values = JSON.parse(await this.engine.get("state"));
    for (const k of ["fmt", "v", "rev", "name"]) delete values[k];
    for (const k of EXTRA_KEYS) {
      const v = await this.engine.get(k);
      if (v !== null) values[k] = v;
    }
    // The device also reports the focused layer's bare aliases.
    const prefix = "l" + (values.edit_layer || "1") + "_";
    for (const k of Object.keys(values)) {
      if (k.startsWith(prefix)) values[k.slice(prefix.length)] = values[k];
    }
    return values;
  }

  /** Re-read the instance and push every value that moved. */
  refresh() {
    if (this.refreshing) { this.dirty = true; return this.refreshing; }
    this.refreshing = (async () => {
      do {
        this.dirty = false;
        const now = await this.snapshot();
        const changed = {};
        for (const k in now) if (this.values[k] !== now[k]) changed[k] = now[k];
        this.values = now;
        if (Object.keys(changed).length) {
          this.emit(changed);
          this.scheduleSave();
        }
      } while (this.dirty);
      this.refreshing = null;
    })();
    return this.refreshing;
  }

  emit(changed) {
    for (const cb of [...this.listeners]) {
      try { cb(changed); } catch (err) { console.error(err); }
    }
  }

  async pump() {
    for (;;) {
      await new Promise((r) => setTimeout(r, this.touched ? EXTRA_THROTTLE_MS : EXTRA_HEARTBEAT_MS));
      this.touched = false;
      if (document.hidden) continue;
      const changed = {};
      for (const k of this.extraKeys) {
        const v = await this.engine.get(k);
        if (v !== null && this.extras[k] !== v) { this.extras[k] = v; changed[k] = v; }
      }
      if (Object.keys(changed).length) { Object.assign(this.values, changed); this.emit(changed); }
    }
  }

  scheduleSave() {
    clearTimeout(this.saveTimer);
    this.saveTimer = setTimeout(async () => {
      try {
        const state = await this.engine.get("state");
        if (state) localStorage.setItem(SAVE_KEY, state);
      } catch (_) { /* quota or blocked storage: patch just isn't remembered */ }
    }, 1000);
  }

  set(key, value) {
    this.engine.set(key, value);
    this.touched = true;
    clearTimeout(this.refreshTimer);
    this.refreshTimer = setTimeout(() => this.refresh(), 20);
  }

  subscribe(cb) {
    this.listeners.add(cb);
    cb(Object.assign({}, this.values));
    return () => this.listeners.delete(cb);
  }

  hierarchy() {
    const emb = globalThis.AMINO_EMBED;
    if (emb) return Promise.resolve(JSON.parse(emb.text("ui_pages.json")));
    return fetch(this.base + "remote-ui/amino/ui_pages.json").then((r) => r.ok ? r.json() : null).catch(() => null);
  }
}
