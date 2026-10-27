/* schwungRemote for the static page: same surface as schwung-manager's
 * /static/schwung-remote-api.js (Schwung docs/MODULES.md), answered in-page
 * by web/bridge.js instead of by a device.  The Remote UI runs in a
 * same-origin iframe, so the bridge is reached through window.parent. */
(function () {
  "use strict";
  const query = new URLSearchParams(location.search);
  const component = query.get("component") || "synth";
  const prefix = component + ":";
  const bridge = () => window.parent.aminoBridge;
  const cache = Object.create(null);
  const callbacks = [];
  let started = false;

  function deliver(values) {
    const params = {};
    for (const k in values) { params[prefix + k] = values[k]; cache[prefix + k] = values[k]; }
    for (const cb of callbacks) {
      try { cb(params); } catch (err) { console.error(err); }
    }
  }

  const strip = (key) => key.startsWith(prefix) ? key.slice(prefix.length) : key;

  window.schwungRemote = {
    component,
    onParamChange(cb) {
      callbacks.push(cb);
      if (!started) {
        started = true;
        bridge().subscribe(deliver);        // first call is the full burst
      } else {
        Promise.resolve().then(() => cb(Object.assign({}, cache)));
      }
    },
    getParam(key) {
      return Promise.resolve(cache[key] !== undefined ? cache[key] : null);
    },
    setParam(key, value) { bridge().set(strip(key), String(value)); },
    getHierarchy() { return bridge().hierarchy(); },
    getChainParams() { return Promise.resolve(bridge().chainParams || null); },
  };
})();
