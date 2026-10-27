"""Parser for the preset notation used in docs/09 and docs/10 (§2 Notation)."""
import re

from amino_params import DIRT_TYPES, FILTER_TYPES, XMOD_TYPES

DRIVE_ALIASES = {"Trans": "Trans", "Tube": "Tube", "Diode": "Diode", "Fold": "Fold", "Hard": "Diode"}
SRC_ALIASES = {
    "LFO1": "LFO1", "LFO2": "LFO2", "Wave Env": "Wave Env", "Filter Env": "Filter Env",
    "Amp Env": "Amp Env", "Poly AT": "Poly AT", "Random": "Random", "Key": "Key",
    "Velocity": "Velocity", "ModWheel": "ModWheel",
}
DST_ALIASES = {
    "O1Wave": "Osc1 Wave", "O2Wave": "Osc2 Wave", "Pitch": "Pitch", "Cutoff": "Cutoff",
    "O1Level": "Osc1 Level", "O2Level": "Osc2 Level", "Amp Level": "Amp Level",
    "LFO1 Rate": "LFO1 Rate", "LFO2 Rate": "LFO2 Rate", "E3 Decay": "E3 Decay", "Pan": "Pan",
    "Resonance": "Resonance", "Drive": "Drive", "XMod": "XMod",
}
SHAPE_ALIASES = {"Sine": "Sine", "Tri": "Tri", "Pulse": "Pulse", "SawDn": "SawDn",
                 "SawUp": "SawUp", "Rand": "Rand", "S&H": "S&H"}

NUM = r"[+-]?(?:\d+\.?\d*|\.\d+)"


def num(s):
    return float(s)


def clean(text):
    return text.replace("`", "").replace("−", "-").replace("–", "-").strip()


def table_key(name):
    return re.sub(r"[^a-z0-9]", "", name.lower())


class LayerSpec:
    """Collects parsed layer params; keys follow docs/04 (without the l{n}_ prefix)."""

    def __init__(self):
        self.p = {}
        self.mods = []
        self.flags = set()

    def set(self, k, v):
        self.p[k] = v


def parse_osc(part, spec, tables):
    m = re.match(r"(O[12])\s+(.*)", part)
    osc = "osc1" if m.group(1) == "O1" else "osc2"
    words = m.group(2).split()
    if words and words[0] == "link":
        spec.set("osc2_link", "On")
        words = words[1:]
    elif words and "@" in words[0]:
        name, wave = words[0].split("@")
        key = table_key(name)
        if key not in tables:
            raise ValueError(f"unknown wavetable '{name}' in '{part}'")
        spec.set(f"{osc}_table", tables[key])
        spec.set(f"{osc}_wave", int(wave))
        words = words[1:]
    if osc == "osc2" and "osc2_level" not in spec.p:
        spec.set("osc2_level", 1.0)
    for w in words:
        if re.fullmatch(r"s" + NUM, w):
            spec.set(f"{osc}_semi", int(num(w[1:])))
        elif re.fullmatch(r"f" + NUM, w):
            spec.set(f"{osc}_fine", int(num(w[1:])))
        elif re.fullmatch(r"L" + NUM, w):
            spec.set(f"{osc}_level", num(w[1:]))
        elif re.fullmatch(r"we" + NUM, w):
            spec.set(f"{osc}_wenv", num(w[2:]))
        elif re.fullmatch(r"v" + NUM, w):
            spec.set(f"{osc}_vel", num(w[1:]))
        else:
            raise ValueError(f"unknown oscillator token '{w}' in '{part}'")


def parse_words(part, spec, tables):
    words = part.split()
    i = 0

    def nxt():
        nonlocal i
        i += 1
        return words[i]

    while i < len(words):
        w = words[i]
        if w in FILTER_TYPES:
            spec.set("ftype", w)
            while i + 1 < len(words) and re.fullmatch(r"[cre]" + NUM, words[i + 1]):
                t = nxt()
                spec.set({"c": "cutoff", "r": "res", "e": "fenv"}[t[0]], num(t[1:]))
        elif w == "kbd":
            spec.set("fkbd", num(nxt()))
        elif w == "fv":
            spec.set("fvel", num(nxt()))
        elif w in DRIVE_ALIASES:
            spec.set("drv_type", DRIVE_ALIASES[w])
            spec.set("drive", num(nxt()))
            if i + 1 < len(words) and words[i + 1] in ("pre", "post"):
                spec.set("drv_pos", nxt().capitalize())
        elif w in DIRT_TYPES:
            spec.set("dirt_type", w)
            spec.set("dirt", num(nxt()))
        elif w == "XM":
            mode = nxt()
            if mode not in XMOD_TYPES:
                raise ValueError(f"unknown cross-mod type '{mode}' in '{part}'")
            spec.set("xmod_type", mode)
            if i + 1 < len(words) and re.fullmatch(NUM, words[i + 1]):
                spec.set("xmod", num(nxt()))
        elif w == "Mono":
            spec.set("poly", "Mono")
        elif w in ("Leg", "Legato"):
            spec.set("poly", "Legato")
        elif w == "Poly":
            spec.set("poly", "Poly")
        elif w == "gl":
            spec.set("glide", num(nxt()))
        elif w == "Flav":
            spec.set("flavour", num(nxt()))
        elif w == "Clean":
            spec.set("osc_mode", "Clean")
        elif w == "Vintage":
            spec.set("osc_mode", "Vintage")
        elif w == "Chord":
            spec.set("chord", nxt())
        elif w == "Scale":
            spec.set("scale", nxt())
        elif w == "octave":
            spec.set("octave", int(num(nxt())))
        elif w == "Pan":
            mode = nxt()
            if re.fullmatch(NUM, mode):
                spec.set("pan_mode", "Fixed")
                spec.set("pan", num(mode))
            else:
                spec.set("pan_mode", mode)
                if i + 1 < len(words) and re.fullmatch(NUM, words[i + 1]):
                    spec.set("pan", num(nxt()))
                elif mode in ("LFO1", "LFO2"):
                    spec.set("pan", 1.0)
        elif w in ("MPE-ready", "ARP", "-", "—"):
            spec.flags.add(w)
        elif w in ("O1", "O2"):
            parse_osc(" ".join(words[i:]), spec, tables)
            return
        else:
            raise ValueError(f"unknown token '{w}' in '{part}'")
        i += 1


def parse_part(part, spec, tables):
    part = clean(part)
    if not part or part in ("—", "-"):
        return
    m = re.fullmatch(r"(E[123](?:/E[123])*)\s+(" + NUM + ")/(" + NUM + ")/(" + NUM + ")/(" + NUM + ")", part)
    if m:
        for env in m.group(1).split("/"):
            for stage, val in zip("adsr", m.groups()[1:]):
                spec.set(f"{env.lower()}_{stage}", num(val))
        return
    m = re.fullmatch(r"(LFO[12])\s+(\S+)\s+(?:sync\s+(.+?)|(" + NUM + r"))(\s+MW On)?", part)
    if m:
        lfo = m.group(1).lower()
        spec.set(f"{lfo}_shape", SHAPE_ALIASES[m.group(2)])
        if m.group(3):
            spec.set(f"{lfo}_sync", "On")
            spec.set(f"{lfo}_div", m.group(3).strip())
        else:
            spec.set(f"{lfo}_rate", num(m.group(4)))
        if m.group(5):
            spec.set(f"{lfo}_mw", "On")
        return
    m = re.fullmatch(r"M:\s*(.+?)\s*→\s*(.+?)\s+(" + NUM + ")", part)
    if m:
        spec.mods.append((SRC_ALIASES[m.group(1)], DST_ALIASES[m.group(2)], num(m.group(3))))
        return
    m = re.fullmatch(r"AT→(Wave|Cut|Vib)\s+(" + NUM + ")", part)
    if m:
        spec.set({"Wave": "at_wave", "Cut": "at_cut", "Vib": "at_vib"}[m.group(1)], num(m.group(2)))
        return
    if re.match(r"O[12]\s", part):
        parse_osc(part, spec, tables)
        return
    parse_words(part, spec, tables)


def parse_notation(text, tables, spec=None):
    spec = spec or LayerSpec()
    for part in clean(text).split("·"):
        parse_part(part, spec, tables)
    return spec


FX_ITEM = re.compile(r"FX([12])\s+([\w]+(?:\s+\w+)?)\s+(" + NUM + r")(?:\s*\(([^)]*)\))?")
DIVISION = re.compile(r"^(?:sync\s+)?(\d+/\d+[dT]?|\d+ bars?)$", re.I)
UNIT = re.compile(r"^(" + NUM + r")\s*(dB|%|Hz|s|ms|:1)?$")


def _fx_params(fx_type, text):
    """Parse '(Tube, Gain 10 dB)'-style lists into {param name: knob 0..1}."""
    from fx import FX_DEFS, div_seconds
    defs = FX_DEFS[fx_type]
    knobs = {}

    def find(name):
        n = name.lower()
        return next((p for p in defs if n in p.aliases), None)

    for item in [s.strip() for s in text.split(",") if s.strip()]:
        if item.lower().startswith(("all ", "for ")):
            continue
        m = re.match(r"^Time\s+(.+?)\s+via Sync$", item)
        if m:
            item = m.group(1)
        div = DIVISION.match(item)
        if div:
            sync = find("Sync") or find("Timing")
            value = div.group(1)
            if sync and value in sync.options:
                knobs[sync.name] = sync.to_knob(value)
            elif find("Time"):
                knobs["Sync"] = 0.0
                knobs["Time"] = find("Time").to_knob(div_seconds(value, 120.0) * 1000)
            continue
        m = re.match(r"^(.+?)\s*=\s*Freeze$", item)
        if m:
            knobs[find(m.group(1)).name] = 1.0
            continue
        enum_hit = next((p for p in defs if p.curve == "enum" and item in map(str, p.options)), None)
        if enum_hit:
            knobs[enum_hit.name] = enum_hit.to_knob(next(o for o in enum_hit.options if str(o) == item))
            continue
        m = re.match(r"^(.+?)\s+(\S+(?:\s*(?:dB|%|Hz|s|ms))?)$", item)
        if not m or not find(m.group(1)):
            raise ValueError(f"cannot parse FX parameter '{item}' for {fx_type}")
        p, raw = find(m.group(1)), m.group(2)
        if raw in ("dark", "bright"):
            knobs[p.name] = 0.3 if raw == "dark" else 0.7
            continue
        if p.curve == "enum":
            val = next(o for o in p.options if str(o).lstrip("+") == raw.lstrip("+"))
            knobs[p.name] = p.to_knob(val)
            continue
        u = UNIT.match(raw)
        value, unit = float(u.group(1)), u.group(2)
        knobs[p.name] = p.to_knob(value) if unit or abs(value) > 1.0 else value
    if fx_type == "Drive" and "Crush" in knobs and "Type" not in knobs:
        knobs["Type"] = find("Type").to_knob("Crush")
    return [knobs.get(p.name, p.to_knob(p.default)) for p in defs]


def parse_fx(text):
    """FX notes -> (globals dict with fx{n}_type/mix/p1..p6 and fx_chain, raw note)."""
    text = clean(text)
    out = {}
    if not text or text in ("—", "-"):
        return out, ""
    if "→" in text:
        out["fx_chain"] = "Serial"
    for m in FX_ITEM.finditer(text):
        k, fx_type = m.group(1), m.group(2)
        out[f"fx{k}_type"] = fx_type
        out[f"fx{k}_mix"] = num(m.group(3))
        for j, knob in enumerate(_fx_params(fx_type, m.group(4) or ""), start=1):
            out[f"fx{k}_p{j}"] = round(knob, 4)
    return out, text
