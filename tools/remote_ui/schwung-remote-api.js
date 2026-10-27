/* Development stand-in for schwung-manager's /static/schwung-remote-api.js.
 *
 * Same surface (Schwung docs/MODULES.md, "schwungRemote JavaScript API"), but
 * it talks to tools/remote_ui/dev_server.py over fetch and Server-Sent Events
 * instead of postMessage to the manager page.  Keys are component-prefixed,
 * getParam reads the local cache, and onParamChange starts with a burst of
 * every value, as on the device.
 */
(function () {
  "use strict";
  const query = new URLSearchParams(location.search);
  const component = query.get("component") || "synth";
  const prefix = component + ":";
  const cache = Object.create(null);
  const callbacks = [];
  let started = null;

  function prefixed(values) {
    const out = {};
    for (const k in values) {
      out[prefix + k] = values[k];
      cache[prefix + k] = values[k];
    }
    return out;
  }

  function deliver(values) {
    const params = prefixed(values);
    for (const cb of callbacks) {
      try { cb(params); } catch (err) { console.error(err); }
    }
  }

  function start() {
    if (started) return started;
    started = fetch("/api/params").then((r) => r.json()).then((values) => {
      deliver(values);
      const events = new EventSource("/api/events");
      events.onmessage = (event) => deliver(JSON.parse(event.data));
    });
    return started;
  }

  const strip = (key) => key.startsWith(prefix) ? key.slice(prefix.length) : key;

  window.schwungRemote = {
    component,
    onParamChange(cb) {
      callbacks.push(cb);
      if (started) started.then(() => cb(Object.assign({}, cache)));
      else start();
    },
    getParam(key) {
      return Promise.resolve(cache[key] !== undefined ? cache[key] : null);
    },
    setParam(key, value) {
      fetch("/api/set", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ key: strip(key), value: String(value) }),
      }).catch((err) => console.error("setParam", key, err));
    },
    getHierarchy() {
      return fetch("/api/hierarchy").then((r) => r.ok ? r.json() : null).catch(() => null);
    },
    getChainParams() {
      return fetch("/api/chain_params").then((r) => r.json()).catch(() => null);
    },
  };
})();
