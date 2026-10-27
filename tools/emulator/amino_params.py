"""Amino parameter defaults (= Init preset, docs/09 §3) and enum lists (docs/04)."""

LAYER_DEFAULTS = {
    "on": "Off", "vol": 0.8, "poly": "Poly", "octave": 0, "glide": 0.0,
    "key_lo": 0, "key_hi": 127, "route": "FX1", "send": 1.0, "flavour": 0.0,
    "osc_mode": "Vintage", "scale": "Off", "chord": "Off",
    "xmod_type": "Off", "xmod": 0.0,
    "dirt_type": "Static", "dirt": 0.0, "drv_type": "Off", "drive": 0.0, "drv_pos": "Pre",
    "ftype": "LP24", "cutoff": 127.0, "res": 0.0, "fenv": 0.0, "fkbd": 0.5, "fvel": 0.2,
    "e1_a": 0.0, "e1_d": 0.5, "e1_s": 0.0, "e1_r": 0.3,
    "e2_a": 0.0, "e2_d": 0.4, "e2_s": 0.2, "e2_r": 0.3,
    "e3_a": 0.0, "e3_d": 0.3, "e3_s": 1.0, "e3_r": 0.25,
    "amp_vel": 0.5, "pan_mode": "Fixed", "pan": 0.0,
    "at_wave": 0.0, "at_cut": 0.0, "at_vib": 0.0,
    "lfo1_pitch": 0.0, "lfo2_pitch": 0.0,
}
for _o in ("osc1", "osc2"):
    LAYER_DEFAULTS.update({
        f"{_o}_table": "Basic", f"{_o}_wave": 63, f"{_o}_semi": 0, f"{_o}_fine": 0,
        f"{_o}_level": 1.0 if _o == "osc1" else 0.0, f"{_o}_wenv": 0.0, f"{_o}_vel": 0.0,
        f"{_o}_kbd": "On",
    })
LAYER_DEFAULTS["osc2_link"] = "Off"
for _l in ("lfo1", "lfo2"):
    LAYER_DEFAULTS.update({
        f"{_l}_shape": "Tri", f"{_l}_rate": 0.4, f"{_l}_sync": "Off", f"{_l}_div": "1/4",
        f"{_l}_phase": "Voice", f"{_l}_fade": 0.0, f"{_l}_mw": "Off",
    })
for _m in range(1, 9):
    LAYER_DEFAULTS.update({f"m{_m}_src": "Off", f"m{_m}_dst": "Off", f"m{_m}_amt": 0.0, f"m{_m}_via": "Off"})

GLOBAL_DEFAULTS = {
    "master_vol": 0.8, "layer_count": 1, "layer_mode": "Stack", "voices": 8,
    "bend_range": 2, "mpe": "Off", "flavour": 0.15, "fx_chain": "Parallel",
    "fx1_type": "Off", "fx1_mix": 0.3, "fx2_type": "Off", "fx2_mix": 0.3,
    "patch_gain": 0.0,
}

FILTER_TYPES = ["LP24", "LP12", "HP24", "HP12", "BP12", "Off"]
DRIVE_TYPES = ["Off", "Trans", "Tube", "Diode", "Fold"]
DIRT_TYPES = ["Static", "Crackle", "Geiger", "Click", "Burst", "Ring"]
XMOD_TYPES = ["Off", "PM", "AM", "Sync"]
LFO_SHAPES = ["Sine", "Tri", "Pulse", "SawDn", "SawUp", "Rand", "S&H"]
PAN_MODES = ["Fixed", "Random", "Alt", "Spread", "LFO1", "LFO2"]
LAYER_MODES = ["Stack", "RoundRb", "RandRR", "Split", "Chan"]
POLY_MODES = ["Poly", "Mono", "Legato"]
CHORDS = {
    "Off": [0], "Maj": [0, 4, 7], "Min": [0, 3, 7], "7": [0, 4, 7, 10], "Maj7": [0, 4, 7, 11],
    "m7": [0, 3, 7, 10], "sus2": [0, 2, 7], "sus4": [0, 5, 7], "5th": [0, 7], "Oct": [0, 12],
    "Stack4": [0, 5, 10, 15],
}
MOD_SOURCES = [
    "Off", "Wave Env", "Filter Env", "Amp Env", "LFO1", "LFO2", "LFO1xMW", "LFO2xMW",
    "Velocity", "Release Vel", "Key", "Poly AT", "Chan AT", "ModWheel", "Breath", "Foot",
    "Expression", "Pitch Bend", "Timbre", "Sustain", "Random", "Random Bi", "Voice #",
    "Layer Alt", "Constant", "Flavour",
]
MOD_TARGETS = [
    "Off", "Pitch", "Osc1 Pitch", "Osc2 Pitch", "Osc1 Wave", "Osc2 Wave", "Osc1 Level",
    "Osc2 Level", "Dirt Level", "Cutoff", "Resonance", "Drive", "Pan", "Amp Level",
    "LFO1 Rate", "LFO2 Rate", "LFO1 Depth", "LFO2 Depth", "E1 Decay", "E2 Decay", "E3 Decay",
    "E1 Attack", "E3 Release", "Glide", "FX Send", "XMod",
]
