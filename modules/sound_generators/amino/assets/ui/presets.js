/* Preset browser drawer: factory presets with search, bank / type / favourite
 * filters and keyboard browsing, plus Load Layer into the focused layer.
 * Loaded on first open (layout.js presetNav).
 *
 * Browsing with the arrow keys loads each preset as it is selected, the way
 * stepping works on Move; Enter or a click loads and closes.  Favourites are
 * kept per browser (localStorage), by preset name.
 */
import { loadLibrary } from "./library.js";
import { lazyStyles } from "./lazy.js";

lazyStyles();

const FAV_KEY = "amino.favourites";

function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}

function readFavourites() {
  try { return new Set(JSON.parse(localStorage.getItem(FAV_KEY) || "[]")); } catch (e) { return new Set(); }
}
function writeFavourites(set) {
  try { localStorage.setItem(FAV_KEY, JSON.stringify([...set])); } catch (e) { /* private mode */ }
}

let drawer = null;

/**
 * Open the browser.  `nav` is presetNav's controller: { index(), load(i) }.
 */
export async function openPresetBrowser(store, nav, opener) {
  if (!drawer) drawer = await build(store, nav);
  drawer.open(opener);
}

async function build(store, nav) {
  const lib = await loadLibrary();
  const presets = lib.presets;
  const cats = lib.categories;
  const favourites = readFavourites();
  const filter = { q: "", bank: "", cat: "", fav: false, tab: "presets" };

  const back = el("div", "drawer-back");
  const panel = el("aside", "drawer drawer-left");
  panel.setAttribute("role", "dialog");
  panel.setAttribute("aria-modal", "true");
  panel.setAttribute("aria-label", "Preset browser");
  const head = el("div", "drawer-head");
  const tabs = el("div", "seg");
  const tabPresets = el("button", "seg-btn on", "Presets");
  const tabLayers = el("button", "seg-btn", "Load layer");
  tabs.append(tabPresets, tabLayers);
  const close = el("button", "icon-btn", "×");
  close.setAttribute("aria-label", "Close preset browser");
  head.append(tabs, close);
  const search = el("input", "search");
  search.type = "search";
  search.setAttribute("aria-label", "Search");
  const chips = el("div", "filter-chips");
  const list = el("div", "browser-list");
  list.setAttribute("role", "listbox");
  const foot = el("div", "drawer-foot");
  panel.append(head, search, chips, list, foot);
  document.body.append(back, panel);

  /* ---- filter chips */
  const chip = (label, active, onClick, title) => {
    const b = el("button", "filter-chip" + (active ? " on" : ""), label);
    b.type = "button";
    if (title) b.title = title;
    b.onclick = onClick;
    return b;
  };
  const banks = [...new Set(presets.map((p) => p.bank))];
  const types = [...new Set(presets.map((p) => p.category))];
  const drawChips = () => {
    chips.textContent = "";
    if (filter.tab !== "presets") return;
    chips.append(
      chip("★ Favourites", filter.fav, () => { filter.fav = !filter.fav; refresh(); }),
      chip("All banks", !filter.bank, () => { filter.bank = ""; refresh(); }),
      ...banks.map((b) => chip(b, filter.bank === b, () => { filter.bank = filter.bank === b ? "" : b; refresh(); })),
    );
    const row2 = el("div", "filter-break");
    chips.append(row2, chip("All types", !filter.cat, () => { filter.cat = ""; refresh(); }),
      ...types.map((c) => chip(cats[c] || c, filter.cat === c, () => { filter.cat = filter.cat === c ? "" : c; refresh(); }, c)));
  };

  /* ---- rows */
  let rows = [];        // [{ i, button }] currently shown
  let cursor = -1;      // index into rows
  const matches = (p) => {
    const q = filter.q;
    if (filter.fav && !favourites.has(p.name)) return false;
    if (filter.bank && p.bank !== filter.bank) return false;
    if (filter.cat && p.category !== filter.cat) return false;
    return !q || (p.name + " " + p.bank + " " + (cats[p.category] || "")).toLowerCase().includes(q);
  };
  const drawPresets = () => {
    const current = nav.index();
    rows = [];
    presets.forEach((p, i) => {
      if (!matches(p)) return;
      const b = el("div", "browser-row" + (i === current ? " on" : ""));
      b.setAttribute("role", "option");
      b.setAttribute("aria-selected", String(i === current));
      const star = el("button", "star" + (favourites.has(p.name) ? " on" : ""), "★");
      star.type = "button";
      star.setAttribute("aria-label", (favourites.has(p.name) ? "Remove " : "Add ") + p.name + " favourite");
      star.onclick = (e) => {
        e.stopPropagation();
        if (favourites.has(p.name)) favourites.delete(p.name); else favourites.add(p.name);
        writeFavourites(favourites);
        star.classList.toggle("on", favourites.has(p.name));
        if (filter.fav) refresh();
      };
      b.append(el("span", "num", String(i).padStart(3, "0")), star, el("span", "name", p.name),
        el("span", "cat", cats[p.category] || p.category), el("span", "bank", p.bank));
      b.onclick = () => { load(i); hide(); };
      list.appendChild(b);
      rows.push({ i, b });
    });
    foot.textContent = rows.length + " of " + presets.length + " presets · ↑↓ to audition, Enter to keep";
    cursor = rows.findIndex((r) => r.i === current);
  };
  const drawLayers = () => {
    rows = [];
    const q = filter.q;
    lib.layerSources.forEach((label, i) => {
      if (q && !label.toLowerCase().includes(q)) return;
      const b = el("div", "browser-row");
      b.setAttribute("role", "option");
      const [name, layer] = label.split("/");
      b.append(el("span", "num", layer), el("span", "name", name));
      b.onclick = () => loadLayer(i, label);
      list.appendChild(b);
      rows.push({ i, b });
    });
    foot.textContent = "Replaces layer " + store.layer + " with the chosen source layer · " + rows.length + " sources";
    cursor = -1;
  };
  const refresh = () => {
    list.textContent = "";
    search.placeholder = filter.tab === "presets" ? "Search " + presets.length + " presets" : "Search layer sources";
    tabPresets.classList.toggle("on", filter.tab === "presets");
    tabLayers.classList.toggle("on", filter.tab === "layers");
    drawChips();
    if (filter.tab === "presets") drawPresets(); else drawLayers();
    rows[cursor]?.b.scrollIntoView({ block: "nearest" });
  };

  const load = (i) => {
    nav.load(i); // records the undo step and resets A/B
    rows.forEach((r) => {
      r.b.classList.toggle("on", r.i === i);
      r.b.setAttribute("aria-selected", String(r.i === i));
    });
  };
  const loadLayer = (i, label) => {
    store.history?.bulk("Load layer " + label);
    store.send("layer_load_src", String(i));
    foot.textContent = "Loaded " + label + " into layer " + store.layer;
  };

  /* ---- open / close / keys */
  let opener = null;
  const hide = () => {
    panel.classList.remove("open");
    back.classList.remove("open");
    opener?.focus();
  };
  close.onclick = hide;
  back.onclick = hide;
  tabPresets.onclick = () => { filter.tab = "presets"; refresh(); };
  tabLayers.onclick = () => { filter.tab = "layers"; refresh(); };
  search.oninput = () => { filter.q = search.value.trim().toLowerCase(); refresh(); };
  panel.addEventListener("keydown", (e) => {
    if (e.key === "Escape") { e.preventDefault(); hide(); return; }
    if (e.key === "ArrowDown" || e.key === "ArrowUp") {
      if (!rows.length) return;
      e.preventDefault();
      cursor = Math.max(0, Math.min(rows.length - 1, cursor + (e.key === "ArrowDown" ? 1 : -1)));
      rows.forEach((r, k) => r.b.classList.toggle("cursor", k === cursor));
      rows[cursor].b.scrollIntoView({ block: "nearest" });
      if (filter.tab === "presets") load(rows[cursor].i);
    }
    if (e.key === "Enter" && rows[cursor]) {
      e.preventDefault();
      rows[cursor].b.click();
    }
  });
  // Keep the highlight right when the preset changes elsewhere.
  store.bind("preset", () => { if (panel.classList.contains("open") && filter.tab === "presets") refresh(); });

  return {
    open(from) {
      opener = from || null;
      refresh();
      panel.classList.add("open");
      back.classList.add("open");
      search.focus();
    },
  };
}
