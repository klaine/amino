#!/usr/bin/env python3
"""Build tools/emulator/out/index.html: an offline preview page for rendered presets.

Open the file directly in a browser; data and waveform peaks are embedded, the
page plays the WAVs next to it.
"""
import json
import math
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRESETS = ROOT / "presets" / "factory"
OUT = Path(__file__).resolve().parent / "out"
POINTS = 240


def analyse(path):
    with wave.open(str(path), "rb") as w:
        n, ch = w.getnframes(), w.getnchannels()
        data = struct.unpack("<%dh" % (n * ch), w.readframes(n))
    mono = [max(abs(data[i]), abs(data[i + 1])) for i in range(0, len(data), ch)]
    step = max(1, len(mono) // POINTS)
    peaks = [round(max(mono[i:i + step] or [0]) / 32768, 3) for i in range(0, step * POINTS, step)]
    peak = max(mono) / 32768 if mono else 0
    rms = math.sqrt(sum(v * v for v in data) / max(1, len(data))) / 32768
    db = lambda x: round(20 * math.log10(x), 1) if x > 1e-9 else -120.0  # noqa: E731
    return peaks, db(peak), db(rms), round(n / w.getframerate(), 2)


def summary(params):
    """Short human-readable engine summary for the card."""
    out = []
    layers = int(params.get("layer_count", 1))
    if layers > 1:
        out.append(f"{layers} layers · {params.get('layer_mode', 'Stack')}")
    for n in range(1, layers + 1):
        t1 = params.get(f"l{n}_osc1_table", "Basic")
        w1 = params.get(f"l{n}_osc1_wave", 63)
        osc = f"{t1}@{w1}"
        if f"l{n}_osc2_level" in params or f"l{n}_osc2_link" in params:
            if params.get(f"l{n}_osc2_link") == "On":
                osc += " + link"
            else:
                osc += f" + {params.get(f'l{n}_osc2_table', 'Basic')}@{params.get(f'l{n}_osc2_wave', 63)}"
        flt = params.get(f"l{n}_ftype", "LP24")
        mode = "Clean" if params.get(f"l{n}_osc_mode") == "Clean" else "Vintage"
        out.append(f"L{n}: {osc} · {flt} · {mode}")
    return out


def main(fx=True):
    items = []
    # out/before_<label>/NNN_name.wav = earlier renders to A/B against (newest folder first)
    compare = sorted((d for d in OUT.glob("before_*") if d.is_dir()),
                     key=lambda d: d.stat().st_mtime, reverse=True)
    for f in sorted(PRESETS.glob("*.json")):
        wav = OUT / f"{f.stem}.wav"
        if not wav.exists():
            continue
        doc = json.loads(f.read_text())
        peaks, peak, rms, secs = analyse(wav)
        alts = []
        for d in compare:
            old = d / wav.name
            if old.exists():
                a_peaks, a_peak, a_rms, a_secs = analyse(old)
                alts.append({"label": d.name.replace("_", " "),
                             "file": f"{d.name}/{wav.name}", "peaks": a_peaks,
                             "peak": a_peak, "rms": a_rms, "secs": a_secs})
        items.append({
            "i": doc["index"], "name": doc["name"], "bank": doc["bank"], "cat": doc["category"],
            "desc": doc.get("description", ""), "phrase": doc["phrase"], "flags": doc["flags"],
            "bpm": doc.get("bpm", 120),
            "fx": doc["fx_notes"], "file": wav.name, "peaks": peaks, "peak": peak, "rms": rms,
            "secs": secs, "summary": summary(doc["params"]), "params": doc["params"],
            "alts": alts,
        })
    html = TEMPLATE.replace("/*DATA*/[]", json.dumps(items, ensure_ascii=False))
    html = html.replace("__MODE__", "engine renders with FX" if fx else "dry engine renders · FX bypassed")
    html = html.replace("__FXLABEL__", "FX: " if fx else "FX (bypassed): ")
    (OUT / "index.html").write_text(html)
    print(f"{len(items)} presets -> {OUT / 'index.html'}")


TEMPLATE = r"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Amino — preset previews</title>
<style>
:root { --bg:#111317; --card:#1b1e24; --line:#2a2e36; --fg:#e7e9ee; --dim:#8a91a0;
        --acc:#7fd1b9; --acc2:#e9b872; }
* { box-sizing: border-box; }
body { margin:0; background:var(--bg); color:var(--fg); font:14px/1.4 system-ui, sans-serif; }
header { position:sticky; top:0; z-index:2; background:#111317ee; backdrop-filter:blur(6px);
         border-bottom:1px solid var(--line); padding:12px 20px; }
h1 { font-size:18px; margin:0 0 8px; } h1 span { color:var(--dim); font-weight:400; font-size:13px; }
.controls { display:flex; flex-wrap:wrap; gap:8px; align-items:center; }
input[type=search] { background:var(--card); border:1px solid var(--line); color:var(--fg);
         border-radius:6px; padding:6px 10px; min-width:220px; }
button.chip { background:var(--card); border:1px solid var(--line); color:var(--dim);
         border-radius:999px; padding:4px 10px; cursor:pointer; font-size:12px; }
button.chip.on { color:#111; background:var(--acc); border-color:var(--acc); }
label.opt { color:var(--dim); font-size:12px; display:flex; gap:4px; align-items:center; }
main { padding:16px 20px 80px; display:grid; gap:10px;
       grid-template-columns:repeat(auto-fill, minmax(360px, 1fr)); }
.card { background:var(--card); border:1px solid var(--line); border-radius:10px; padding:10px 12px;
        cursor:pointer; outline:none; }
.card:focus-visible { border-color:var(--acc2); }
.card.playing { border-color:var(--acc); box-shadow:0 0 0 1px var(--acc) inset; }
.top { display:flex; justify-content:space-between; gap:8px; align-items:baseline; }
.name { font-weight:600; } .idx { color:var(--dim); font-variant-numeric:tabular-nums; margin-right:6px; }
.badge { font-size:11px; color:var(--dim); border:1px solid var(--line); border-radius:4px; padding:0 5px; }
.badge.Modern { color:var(--acc2); }
.desc { color:var(--dim); margin:2px 0 6px; font-size:13px; }
canvas { width:100%; height:44px; display:block; border-radius:4px; background:#14171c; }
.meta { display:flex; justify-content:space-between; color:var(--dim); font-size:11px; margin-top:4px;
        font-variant-numeric:tabular-nums; }
.warn { color:#ff8a80; }
details { margin-top:6px; color:var(--dim); font-size:12px; }
details pre { white-space:pre-wrap; margin:6px 0 0; font-size:11px; color:#aab2c0; }
footer { position:fixed; bottom:0; left:0; right:0; background:#0d0f12; border-top:1px solid var(--line);
         padding:8px 20px; display:flex; gap:12px; align-items:center; color:var(--dim); font-size:12px; }
footer b { color:var(--fg); }
.empty { color:var(--dim); padding:40px; }
.ab { display:flex; gap:4px; margin:0 0 6px; align-items:center; }
.ab button { background:#14171c; border:1px solid var(--line); color:var(--dim); border-radius:4px;
         padding:2px 8px; font-size:11px; cursor:pointer; font-variant-numeric:tabular-nums; }
.ab button.on { color:#111; background:var(--acc2); border-color:var(--acc2); }
.ab .hint { color:var(--dim); font-size:11px; margin-left:auto; }
</style></head>
<body>
<header>
  <h1>Amino preset previews <span>__MODE__</span></h1>
  <div class="controls">
    <input type="search" id="q" placeholder="Search name, description, wavetable…">
    <span id="banks"></span><span id="cats"></span>
    <label class="opt"><input type="checkbox" id="auto"> auto-advance</label>
    <label class="opt"><input type="checkbox" id="norm"> loudness-match</label>
    <label class="opt" id="abopt"><input type="checkbox" id="abonly"> A/B only</label>
  </div>
</header>
<main id="grid"></main>
<footer><span id="now">Click a card (or press Space) to play · ←/→ previous/next · B toggle A/B · Esc stop</span></footer>
<audio id="player" preload="none"></audio>
<script>
const DATA = /*DATA*/[];
const CATS = {IN:"Init",BS:"Bass",LD:"Lead",PD:"Pad",KY:"Keys",PL:"Pluck",FX:"Effects",DR:"Drones",ML:"Multi-layer"};
const BANKS = [...new Set(DATA.map(d => d.bank))];
const state = { bank:new Set(BANKS), cat:new Set(Object.keys(CATS)), q:"", current:-1, side:0, abOnly:false };
const grid = document.getElementById("grid"), player = document.getElementById("player");
const now = document.getElementById("now");
let visible = [];
if (!DATA.some(d => d.alts.length)) document.getElementById("abopt").hidden = true;
// side 0 = current render (A), side k = d.alts[k-1] (B, C…)
const take = (d, side) => side ? d.alts[side - 1] : d;

function chips(el, values, set, label) {
  for (const v of values) {
    const b = document.createElement("button");
    b.className = "chip on"; b.textContent = label ? label(v) : v;
    b.onclick = () => { set.has(v) ? set.delete(v) : set.add(v); b.classList.toggle("on"); render(); };
    el.appendChild(b);
  }
}
chips(document.getElementById("banks"), BANKS, state.bank);
chips(document.getElementById("cats"), Object.keys(CATS), state.cat, k => CATS[k]);

function draw(canvas, d, progress) {
  const r = devicePixelRatio || 1, w = canvas.clientWidth, h = canvas.clientHeight;
  if (canvas.width !== w * r) { canvas.width = w * r; canvas.height = h * r; }
  const c = canvas.getContext("2d"); c.setTransform(r,0,0,r,0,0); c.clearRect(0,0,w,h);
  const n = d.peaks.length, bw = w / n;
  for (let i = 0; i < n; i++) {
    const v = Math.max(0.02, d.peaks[i]) * (h / 2 - 2);
    c.fillStyle = (i / n) < progress ? "#7fd1b9" : "#4a5260";
    c.fillRect(i * bw, h / 2 - v, Math.max(1, bw - 0.5), v * 2);
  }
}

function card(d, pos) {
  const el = document.createElement("div");
  el.className = "card"; el.tabIndex = 0; el.dataset.pos = pos;
  const warn = d.peak < -30 ? " warn" : (d.peak > -0.5 ? " warn" : "");
  el.innerHTML = `
    <div class="top"><div><span class="idx">${String(d.i).padStart(3,"0")}</span><span class="name"></span></div>
      <span class="badge ${d.bank === 'Nostalgia' ? '' : 'Modern'}">${d.bank}</span></div>
    <div class="desc"></div>
    ${d.alts.length ? `<div class="ab"><button class="on">A · now</button>${d.alts.map((a, k) =>
      `<button>${String.fromCharCode(66 + k)} · ${a.label}</button>`).join("")}<span class="hint"></span></div>` : ""}
    <canvas></canvas>
    <div class="meta"><span>${CATS[d.cat]} · ${d.phrase} · ${d.bpm} BPM${d.flags.length ? " · " + d.flags.join(", ") : ""}</span>
      <span class="${warn.trim()}">peak ${d.peak} · rms ${d.rms} dBFS · ${d.secs}s</span></div>
    <details><summary>engine</summary><pre></pre></details>`;
  el.querySelector(".name").textContent = d.name;
  el.querySelector(".desc").textContent = d.desc;
  el.querySelector("pre").textContent = d.summary.join("\n") + (d.fx ? "\n__FXLABEL__" + d.fx : "")
    + "\n\n" + JSON.stringify(d.params, null, 1);
  el.querySelector("details").onclick = e => e.stopPropagation();
  el.querySelectorAll(".ab button").forEach((b, side) => b.onclick = e => {
    e.stopPropagation();
    switchSide(pos, side);
  });
  if (d.alts.length) el.querySelector(".ab .hint").textContent =
    d.alts.map(a => `rms ${d.rms} vs ${a.rms}`).join(" · ");
  const cv = el.querySelector("canvas");
  cv.onclick = e => {
    e.stopPropagation();
    const frac = e.offsetX / cv.clientWidth;
    play(pos, frac, state.current === pos ? state.side : 0);
  };
  el.onclick = () => (state.current === pos && !player.paused) ? player.pause()
    : play(pos, 0, state.current === pos ? state.side : 0);
  requestAnimationFrame(() => draw(cv, d, 0));
  return el;
}

function render() {
  const q = state.q.toLowerCase();
  visible = DATA.filter(d => state.bank.has(d.bank) && state.cat.has(d.cat) &&
    (!state.abOnly || d.alts.length) &&
    (!q || (d.name + " " + d.desc + " " + d.summary.join(" ")).toLowerCase().includes(q)));
  grid.replaceChildren(...visible.map(card));
  if (!visible.length) grid.innerHTML = `<div class="empty">No presets match.</div>`;
  state.current = -1; state.side = 0;
}

function play(pos, frac = 0, side = 0, autoplay = true) {
  if (pos < 0 || pos >= visible.length) return;
  const d = visible[pos], t = take(d, side);
  document.querySelectorAll(".card.playing").forEach(c => c.classList.remove("playing"));
  const el = grid.children[pos]; el.classList.add("playing"); el.focus({preventScroll:true});
  el.scrollIntoView({block:"nearest", behavior:"smooth"});
  const changed = state.current !== pos || state.side !== side;
  if (changed && state.current >= 0 && state.current !== pos) markSide(state.current, 0);
  state.current = pos; state.side = side;
  markSide(pos, side);
  const norm = document.getElementById("norm").checked;
  player.volume = norm ? Math.min(1, Math.pow(10, (-20 - t.rms) / 20)) : 1;
  const go = () => {
    if (!autoplay) return;
    player.play().catch(() => {
      now.textContent = "Browser blocked audio from file:// — run: python3 -m http.server -d tools/emulator/out, then open http://localhost:8000";
    });
  };
  const my = ++loadToken;
  if (changed) {
    player.pause();
    audioUrl(t.file).then(u => {
      if (my !== loadToken) return;
      player.onloadedmetadata = () => { player.currentTime = frac * player.duration; };
      player.src = u;
      go();
    });
  } else {
    if (player.readyState >= 1) player.currentTime = frac * player.duration;
    go();
  }
  d.alts.forEach(a => audioUrl(a.file));
  if (d.alts.length) audioUrl(d.file);
  const tag = d.alts.length ? ` [${side ? String.fromCharCode(65 + side) + " · " + t.label : "A · now"}]` : "";
  now.innerHTML = `Playing <b>${String(d.i).padStart(3,"0")} ${d.name}</b>${tag} — ${d.desc}`;
}

// Blob URLs: python's http.server ignores Range requests, which breaks seeking.
const blobs = new Map();
let loadToken = 0;
function audioUrl(file) {
  if (location.protocol === "file:") return Promise.resolve(file);
  if (!blobs.has(file)) blobs.set(file, fetch(file)
    .then(r => r.ok ? r.blob() : Promise.reject())
    .then(b => URL.createObjectURL(b), () => file));
  return blobs.get(file);
}

function markSide(pos, side) {
  const el = grid.children[pos];
  if (!el) return;
  el.querySelectorAll(".ab button").forEach((b, k) => b.classList.toggle("on", k === side));
  draw(el.querySelector("canvas"), take(visible[pos], side), 0);
}

// Swap A/B at the same playback position.
function switchSide(pos, side) {
  const d = visible[pos];
  if (!d || side > d.alts.length) return;
  const same = state.current === pos && player.duration;
  const frac = same ? player.currentTime / player.duration : 0;
  const resume = !same || !player.paused;
  play(pos, frac, side, resume);
}

player.ontimeupdate = () => {
  const el = grid.children[state.current];
  if (el && player.duration) draw(el.querySelector("canvas"), take(visible[state.current], state.side), player.currentTime / player.duration);
};
player.onended = () => {
  const el = grid.children[state.current];
  if (el) draw(el.querySelector("canvas"), take(visible[state.current], state.side), 0);
  if (document.getElementById("auto").checked) play(state.current + 1);
};
player.onerror = () => { now.textContent = "Could not load audio — run render.py so the WAV files sit next to index.html."; };

document.getElementById("q").oninput = e => { state.q = e.target.value; render(); };
document.getElementById("abonly").onchange = e => { state.abOnly = e.target.checked; player.pause(); render(); };
document.addEventListener("keydown", e => {
  if (e.target.tagName === "INPUT" && e.target.type === "search") return;
  if (e.key === "ArrowRight") { e.preventDefault(); play(state.current + 1); }
  else if (e.key === "ArrowLeft") { e.preventDefault(); play(Math.max(0, state.current - 1)); }
  else if ((e.key === "b" || e.key === "B") && state.current >= 0) {
    const n = visible[state.current].alts.length;
    if (n) switchSide(state.current, (state.side + 1) % (n + 1));
  }
  else if (e.key === " ") { e.preventDefault();
    if (state.current < 0) play(0); else player.paused ? player.play() : player.pause(); }
  else if (e.key === "Escape") { player.pause(); player.currentTime = 0; }
});
window.addEventListener("resize", () => visible.forEach((d, i) => draw(grid.children[i].querySelector("canvas"), take(d, i === state.current ? state.side : 0), 0)));
render();
</script>
</body></html>
"""

if __name__ == "__main__":
    main()
