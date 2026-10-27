#!/usr/bin/env python3
"""Extract factory presets from docs/09, 10, 13, 14, 16 and 17 into presets/factory/*.json.

Each JSON file stores only values that differ from Init (docs/06 §3), using the
flat state-blob keys of docs/04 (globals bare, layer keys as l{n}_<key>).
"""
import json
import re
import sys
from pathlib import Path

from amino_params import GLOBAL_DEFAULTS, LAYER_DEFAULTS
from ml_presets import INIT_MODERN, INIT_MODERN_FX, ML_PRESETS
from notation import LayerSpec, parse_fx, parse_notation, table_key
from polish import add_expression, apply_level, load_trims

ROOT = Path(__file__).resolve().parents[2]
DOCS = [("Nostalgia", ROOT / "docs" / "09-factory-presets.md"),
        ("Modern", ROOT / "docs" / "10-factory-presets-modern.md"),
        (None, ROOT / "docs" / "13-genre-banks.md"),
        (None, ROOT / "docs" / "14-expansion-bank.md"),
        (None, ROOT / "docs" / "16-trance-gate-presets.md"),
        (None, ROOT / "docs" / "17-expansion-bank-2.md")]
BANK_HEADING = re.compile(r"^## Bank: (.+?) · (\d+)–(\d+) · (\d+) BPM · phrase `(\w+)`")
PRESET_COUNT = 324
MANIFEST = ROOT / "tools" / "wavegen" / "out" / "manifest.json"
OUT = ROOT / "presets" / "factory"

ROW = re.compile(r"^\|\s*(\d+)\s*\|\s*((?:IN|BS|LD|PD|KY|PL|FX|DR|ML) [^|]+?)\s*\|(.*)\|\s*$")

PHRASES = {"IN": "init", "BS": "bass", "LD": "lead", "PD": "pad", "KY": "keys",
           "PL": "pluck", "FX": "fx", "DR": "drone", "ML": "keys"}
# Presets that shipped with a stored Scale Set; every other preset leaves scale to the user.
KEEPS_SCALE = {257, 259}
GENRE_ROLE = {"BS": "bass", "LD": "lead", "KY": "keys", "PD": "pad", "PL": "pluck", "FX": "fx"}


def load_tables():
    manifest = json.loads(MANIFEST.read_text())
    return {table_key(t["name"]): t["name"] for t in manifest["tables"]}


def category_defaults(bank, cat, spec):
    """Apply the 'All <category>: ...' rules stated above each docs table."""
    p = spec.p

    def default(k, v):
        p.setdefault(k, v)

    if cat == "BS":
        default("poly", "Mono")
        if bank != "Nostalgia":
            default("flavour", 0.0)
    elif cat == "LD":
        default("poly", "Legato")
        default("glide", 0.08 if bank == "Nostalgia" else 0.05)
        if bank == "Modern":
            default("scale", "Set")
    elif cat == "PD":
        if "pan_mode" not in p:
            p["pan_mode"], p["pan"] = "Spread", 0.5
        if bank == "Nostalgia":
            default("flavour", 0.2)
    elif cat == "PL" and bank == "Modern":
        default("scale", "Set")


def spec_to_layer(spec, base=None):
    layer = dict(base or LAYER_DEFAULTS)
    layer.update(spec.p)
    slot = 1
    while slot <= 8 and layer[f"m{slot}_src"] != "Off":
        slot += 1
    for src, dst, amt in spec.mods:
        if slot > 8:
            raise ValueError("more than 8 mod slots")
        layer.update({f"m{slot}_src": src, f"m{slot}_dst": dst, f"m{slot}_amt": amt})
        slot += 1
    layer["on"] = "On"
    return layer


def to_flat(globals_, layers, keep_scale=False):
    flat = {k: v for k, v in globals_.items() if GLOBAL_DEFAULTS.get(k) != v}
    # The engine defaults fx_chain to Serial, so authored parallel pairs must be stored.
    if globals_["fx_chain"] == "Parallel" and globals_["fx1_type"] != "Off" and globals_["fx2_type"] != "Off":
        flat["fx_chain"] = "Parallel"
    for n, layer in enumerate(layers, start=1):
        for k, v in layer.items():
            if k == "scale" and not keep_scale:
                continue  # factory presets do not store scale (docs/10 §2)
            default = LAYER_DEFAULTS[k] if not (n == 1 and k == "on") else "On"
            if v != default:
                flat[f"l{n}_{k}"] = v
    return flat


def parse_docs(tables):
    presets = {}
    for doc_bank, path in DOCS:
        bank, bpm, family = doc_bank, 120, None
        for line in path.read_text().splitlines():
            h = BANK_HEADING.match(line)
            if h:
                bank, bpm, family = h.group(1), int(h.group(4)), h.group(5)
                continue
            m = ROW.match(line)
            if not m:
                continue
            index, name = int(m.group(1)), m.group(2).strip()
            cells = [c.strip() for c in m.group(3).split("|")]
            if len(cells) != 6:
                continue
            character, osc, flt, env, motion, fx = cells
            spec = parse_notation(" · ".join([osc, flt, env, motion]), tables)
            cat = name[:2]
            category_defaults(bank, cat, spec)
            g = dict(GLOBAL_DEFAULTS)
            fx_globals, fx_notes = parse_fx(fx)
            g.update(fx_globals)
            if cat == "BS" and bank != "Nostalgia":
                g["flavour"] = 0.0
            phrase = f"{family}_{GENRE_ROLE[cat]}" if family and family != "category" else PHRASES[cat]
            presets[index] = dict(index=index, name=name, bank=bank, category=cat, bpm=bpm,
                                  description=character,
                                  phrase=phrase, globals=g, layers=[spec_to_layer(spec)],
                                  flags=sorted(spec.flags - {"—", "-"}), fx_notes=fx_notes)
    return presets


def add_inits(presets, tables):
    presets[0] = dict(index=0, name="IN Init", bank="Nostalgia", category="IN", phrase="init",
                      description="Single saw, filter open, no FX",
                      globals=dict(GLOBAL_DEFAULTS, fx_chain="Serial"), layers=[spec_to_layer(LayerSpec())],
                      flags=[], fx_notes="")
    g = dict(GLOBAL_DEFAULTS, flavour=0.05)
    fx_globals, fx_notes = parse_fx(INIT_MODERN_FX)
    g.update(fx_globals)
    presets[64] = dict(index=64, name="IN Init Modern", bank="Modern", category="IN", phrase="init",
                       description="Clean detuned saws, gentle glue and room",
                       globals=g, layers=[spec_to_layer(parse_notation(INIT_MODERN, tables))],
                       flags=[], fx_notes=fx_notes)


def add_multilayer(presets, tables):
    for index, d in ML_PRESETS.items():
        layers = []
        for ld in d["layers"]:
            base = presets[ld["base"]]["layers"][0] if "base" in ld else None
            spec = parse_notation(ld.get("n", ""), tables)
            if "pan" in ld:
                spec.p.update(pan_mode="Fixed", pan=ld["pan"])
            if "vol" in ld:
                spec.p["vol"] = ld["vol"]
            if "key" in ld:
                spec.p["key_lo"], spec.p["key_hi"] = ld["key"]
            if "route" in ld:
                spec.p["route"] = ld["route"]
            layers.append(spec_to_layer(spec, base))
        g = dict(GLOBAL_DEFAULTS, layer_count=len(layers), layer_mode=d["mode"],
                 voices=d.get("voices", 8))
        fx_globals, fx_notes = parse_fx(d.get("fx", ""))
        g.update(fx_globals)
        presets[index] = dict(index=index, name=d["name"], bank="Nostalgia" if index < 64 else "Modern",
                              category="ML", phrase=d["phrase"], globals=g, layers=layers,
                              description=f"{d['mode']}, {len(layers)} layers",
                              flags=[], fx_notes=fx_notes)


def auto_route(preset):
    """A layer routed to an empty slot goes to the slot that has an effect (docs 09/10 list one FX)."""
    g = preset["globals"]
    active = {r for r in ("FX1", "FX2") if g.get(f"fx{r[-1]}_type", "Off") != "Off"}
    for layer in preset["layers"]:
        route = layer["route"]
        if route in ("FX1", "FX2") and route not in active:
            layer["route"] = ("FX1" if "FX1" in active else "FX2") if active else route


def slug(name):
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


def main():
    if not MANIFEST.exists():
        sys.exit("Run tools/wavegen/wavegen.py first (needs out/manifest.json).")
    tables = load_tables()
    presets = parse_docs(tables)
    add_inits(presets, tables)
    add_multilayer(presets, tables)
    missing = sorted(set(range(PRESET_COUNT)) - set(presets))
    if missing:
        sys.exit(f"missing presets: {missing}")
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("*.json"):
        old.unlink()
    trims = load_trims()
    for i in range(PRESET_COUNT):
        p = presets[i]
        auto_route(p)
        add_expression(p)
        apply_level(p, trims)
        doc = {"fmt": "amino-preset", "v": 1, "index": i, "name": p["name"], "bank": p["bank"],
               "category": p["category"], "description": p["description"], "phrase": p["phrase"],
               "bpm": p.get("bpm", 120), "flags": p["flags"],
               "fx_notes": p["fx_notes"], "params": to_flat(p["globals"], p["layers"], i in KEEPS_SCALE)}
        (OUT / f"{i:03d}_{slug(p['name'])}.json").write_text(json.dumps(doc, indent=2, ensure_ascii=False) + "\n")
    print(f"wrote {PRESET_COUNT} presets to {OUT}")


if __name__ == "__main__":
    main()
