"""Factory-bank polish applied after the docs are parsed (docs/18).

Two passes, both kept out of the docs tables so those stay a readable
description of each sound:

* Expression: every preset answers pad pressure (aftertouch) and the mod
  wheel.  A layer that already has a route from a controller keeps it; the
  routes added here are all scaled by that controller, so with no pressure and
  the wheel down a preset sounds exactly as authored.
* Level: presets/level_trims.json (written by calibrate_levels.py) sets each
  preset's Patch Gain so the bank plays at one loudness.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRIMS = ROOT / "presets" / "level_trims.json"

AT_SOURCES = ("Poly AT", "Chan AT")
MW_SOURCES = ("ModWheel", "LFO1xMW", "LFO2xMW")
LP_TYPES = ("LP24", "LP12", "Ladder 24dB")

# Mod-wheel vibrato on a free LFO2: sine at ~5.5 Hz (0.01 * 5000^x), a short
# fade-in, +-0.3 semitone at full wheel (LFO to Pitch is +-12 st at 100 %).
VIBRATO = {"lfo2_shape": "Sine", "lfo2_rate": 0.74, "lfo2_fade": 0.25,
           "lfo2_mw": "On", "lfo2_pitch": 0.025}


def _slot_values(layer, field):
    return [layer[f"m{m}_{field}"] for m in range(1, 9)]


def _uses(layer, sources):
    return any(v in sources for v in _slot_values(layer, "src") + _slot_values(layer, "via"))


def has_at(layer):
    return (_uses(layer, AT_SOURCES) or
            any(float(layer[k]) != 0.0 for k in ("at_cut", "at_wave", "at_vib")))


def has_mw(layer):
    return _uses(layer, MW_SOURCES) or "On" in (layer["lfo1_mw"], layer["lfo2_mw"])


def lfo2_free(layer):
    return (not _uses(layer, ("LFO2", "LFO2xMW")) and layer["pan_mode"] != "LFO2"
            and float(layer.get("lfo2_pitch", 0.0)) == 0.0)


def lfo1_used(layer):
    return _uses(layer, ("LFO1", "LFO1xMW")) or layer["pan_mode"] == "LFO1"


def openable(layer):
    """A low-pass that is not already wide open: brightening it is audible."""
    return layer["ftype"] in LP_TYPES and float(layer["cutoff"]) < 112.0


def scannable(layer):
    """Osc 1 sits on a wavetable position (61-63 are fixed classic waves)."""
    return int(layer["osc1_wave"]) < 61


def free_slot(layer):
    for m in range(1, 9):
        if layer[f"m{m}_src"] == "Off":
            return m
    return None


def add_route(layer, src, dst, amt):
    m = free_slot(layer)
    if m is None:
        return False
    layer.update({f"m{m}_src": src, f"m{m}_dst": dst, f"m{m}_amt": amt})
    return True


# Pressure: (cutoff amount when the filter can open, wave amount otherwise).
AT_AMOUNTS = {"BS": (0.15, 0.12), "LD": (0.2, 0.15), "PD": (0.2, 0.2), "DR": (0.2, 0.2),
              "KY": (0.12, 0.1), "PL": (0.12, 0.1), "FX": (0.25, 0.25), "ML": (0.15, 0.12)}


def add_pressure(cat, layer):
    if has_at(layer):
        return
    cut, wave = AT_AMOUNTS[cat]
    if openable(layer):
        layer["at_cut"] = cut
    elif scannable(layer):
        layer["at_wave"] = wave


def add_wheel(cat, layer):
    if has_mw(layer):
        return
    if cat in ("LD", "KY"):
        if lfo2_free(layer):
            layer.update(VIBRATO)
            if cat == "KY":
                layer["lfo2_pitch"] = 0.02
        elif openable(layer):
            add_route(layer, "ModWheel", "Cutoff", 0.2)
    elif cat in ("PD", "DR"):
        if scannable(layer):
            add_route(layer, "ModWheel", "Osc1 Wave", 0.25)
        elif openable(layer):
            add_route(layer, "ModWheel", "Cutoff", 0.2)
        elif lfo2_free(layer):
            layer.update(VIBRATO)
    elif cat == "PL":
        if float(layer["e3_s"]) < 0.5:
            add_route(layer, "ModWheel", "E3 Decay", 0.25)   # longer ring-out
        elif openable(layer):
            add_route(layer, "ModWheel", "Cutoff", 0.2)
    elif cat == "BS":
        if openable(layer):
            add_route(layer, "ModWheel", "Cutoff", 0.2)
        elif layer["drv_type"] != "Off":
            add_route(layer, "ModWheel", "Drive", 0.3)
        elif scannable(layer):
            add_route(layer, "ModWheel", "Osc1 Wave", 0.2)
    elif cat == "FX":
        if lfo1_used(layer):
            add_route(layer, "ModWheel", "LFO1 Rate", 0.3)
        elif scannable(layer):
            add_route(layer, "ModWheel", "Osc1 Wave", 0.3)
        elif openable(layer):
            add_route(layer, "ModWheel", "Cutoff", 0.25)
    elif cat == "ML":
        if openable(layer):
            add_route(layer, "ModWheel", "Cutoff", 0.15)
        elif scannable(layer):
            add_route(layer, "ModWheel", "Osc1 Wave", 0.2)


def add_expression(preset):
    cat = preset["category"]
    if cat == "IN":
        return                      # Init stays a blank slate
    for layer in preset["layers"]:
        add_pressure(cat, layer)
        add_wheel(cat, layer)


def load_trims():
    if not TRIMS.exists():
        return {}
    return {int(k): float(v) for k, v in json.loads(TRIMS.read_text()).items()}


def apply_level(preset, trims):
    if preset["category"] == "IN":
        return                      # Init carries no hidden trim
    gain = trims.get(preset["index"], 0.0)
    if gain:
        preset["globals"]["patch_gain"] = round(gain, 1)
