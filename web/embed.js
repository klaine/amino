/* Single-file build only (scripts/build_single.py): unpacks the payload that
 * the build appended to the page and serves it to the rest of the app.
 *
 * The payload is gzip(JSON index + file bytes), base64 in a <script> tag.  It
 * holds amino.wasm, the Remote UI modules/styles/data and the worklet source.
 * The Remote UI runs in a srcdoc iframe whose ES modules are served from an
 * import map of data: URLs, and whose fetch() is answered from this store.
 */
globalThis.AMINO_EMBED = (() => {
  const files = new Map();
  const utf8 = new TextDecoder();
  const emb = { files, workletUrl: "" };

  const b64 = (bytes) => {
    let s = "";
    for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
    return btoa(s);
  };
  emb.text = (path) => utf8.decode(files.get(path));
  emb.wasm = async () => {
    const b = files.get("amino.wasm");
    return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
  };

  emb.ready = (async () => {
    const raw = atob(document.getElementById("amino-payload").textContent.trim());
    const gz = new Uint8Array(raw.length);
    for (let i = 0; i < raw.length; i++) gz[i] = raw.charCodeAt(i);
    const all = new Uint8Array(await new Response(
      new Blob([gz]).stream().pipeThrough(new DecompressionStream("gzip"))).arrayBuffer());
    const view = new DataView(all.buffer);
    const headLen = view.getUint32(0, true);
    const index = JSON.parse(utf8.decode(all.subarray(4, 4 + headLen)));
    let pos = 4 + headLen;
    for (const [path, length] of index) {
      files.set(path, all.subarray(pos, pos + length));
      pos += length;
    }
    emb.workletUrl = "data:text/javascript;base64," + b64(files.get("worklet.js"));
  })();

  /** The Remote UI page for the iframe's srcdoc. */
  emb.uiDoc = () => {
    const map = {};
    for (const path of files.keys()) {
      if (path.startsWith("ui/") && path.endsWith(".js")) {
        map["amino-ui/" + path.slice(3)] = "data:text/javascript;base64," + b64(files.get(path));
      }
    }
    const vfs = {};
    for (const path of files.keys()) if (path.startsWith("vfs/")) vfs[path.slice(4)] = 1;
    // Same-origin iframe: files come straight from the parent's store.
    const shim = `const E = parent.AMINO_EMBED, vfs = ${JSON.stringify(vfs)}, realFetch = window.fetch.bind(window);
window.fetch = (url, opts) => {
  const key = String(url).replace(/^\\.\\//, "").split("?")[0];
  if (!(key in vfs)) return realFetch(url, opts);
  const bytes = E.files.get("vfs/" + key);
  const type = key.endsWith(".json") ? "application/json" : "application/octet-stream";
  return Promise.resolve(new Response(bytes.slice(), { status: 200, headers: { "Content-Type": type } }));
};`;
    return emb.text("ui.html")
      .replace("{{STYLE}}", () => "<style>" + emb.text("ui/style.css") + "\n" + emb.text("ui/lazy.css") + "</style>")
      .replace("{{SCRIPTS}}", () =>
        '<script type="importmap">' + JSON.stringify({ imports: map }) + "<\/script>"
        + "<script>" + shim + "<\/script>"
        + "<script>" + emb.text("schwung-remote-api.js") + "<\/script>"
        + '<script type="module">import "amino-ui/app.js";<\/script>');
  };
  return emb;
})();
