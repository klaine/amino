"""Canonical Amino parameter definitions (docs/04).

The C descriptor table, the UI contract and the compiled factory bank are all
generated from this file.  Keep it in sync with docs/04-parameter-reference.md;
the emulator's amino_params.py is the behavioural default reference.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

ONOFF = ["Off", "On"]
POLY = ["Poly", "Mono", "Legato"]
LAYER_MODES = ["Stack", "RoundRb", "RandRR", "Split", "Chan"]
ROUTES = ["Dry", "FX1", "FX2"]
OSC_MODES = ["Vintage", "Clean"]
SCALES = ["Off", "Set", "Major", "Minor", "Dorian", "Phryg", "Lydian", "Mixo",
          "Penta", "MinPen", "Blues", "Chrom"]
CHORDS = ["Off", "Maj", "Min", "7", "Maj7", "m7", "sus2", "sus4", "5th", "Oct", "Stack4"]
# State v2 order; params.c migrates v1 blobs (LP24 LP12 HP24 HP12 BP12 Off Moog).
FILTER_TYPES = ["LP24", "LP12", "Ladder 24dB", "HP24", "HP12", "BP12", "Off"]
DRIVE_TYPES = ["Off", "Trans", "Tube", "Diode", "Fold"]
DIRT_TYPES = ["Static", "Crackle", "Geiger", "Click", "Burst", "Ring"]
XMOD_TYPES = ["Off", "PM", "AM", "Sync"]
PAN_MODES = ["Fixed", "Random", "Alt", "Spread", "LFO1", "LFO2"]
LFO_SHAPES = ["Sine", "Tri", "Pulse", "SawDn", "SawUp", "Rand", "S&H"]
LFO_PHASE = ["Free", "Voice", "Global"]
LFO_DIVS = ["1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4",
            "1/2", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars"]
FX_TYPES = ["Off", "Chorus", "Flanger", "Phaser", "Tremolo", "Drive", "Comp",
            "EQ", "Delay", "Reverb", "Shimmer", "Trance Gate"]
FX_CHAIN = ["Parallel", "Serial"]
MOD_SOURCES = [
    "Off", "Wave Env", "Filter Env", "Amp Env", "LFO1", "LFO2", "LFO1xMW", "LFO2xMW",
    "Velocity", "Release Vel", "Key", "Poly AT", "Chan AT", "ModWheel", "Breath", "Foot",
    "Expression", "Pitch Bend", "Timbre", "Sustain", "Random", "Random Bi", "Voice #",
    "Layer Alt", "Constant", "Flavour", "Aux Env",
]
MOD_TARGETS = [
    "Off", "Pitch", "Osc1 Pitch", "Osc2 Pitch", "Osc1 Wave", "Osc2 Wave", "Osc1 Level",
    "Osc2 Level", "Dirt Level", "Cutoff", "Resonance", "Drive", "Pan", "Amp Level",
    "LFO1 Rate", "LFO2 Rate", "LFO1 Depth", "LFO2 Depth", "E1 Decay", "E2 Decay", "E3 Decay",
    "E1 Attack", "E3 Release", "Glide", "FX Send", "XMod",
]

with open(os.path.join(ROOT, "tools", "wavegen", "out", "manifest.json"), "r",
          encoding="utf-8") as f:
    TABLE_NAMES = [t["name"] for t in json.load(f)["tables"]]

# kind: float | int | enum
# scope: layer | global
# opts: option list for enum
LAYER = [
    dict(key="on", name="Layer On", short="On", kind="enum", opts=ONOFF, default=0, viz="switch"),
    dict(key="vol", name="Layer Volume", short="Vol", kind="float", mn=0, mx=1, default=.8, unit="%"),
    dict(key="poly", name="Voice Mode", short="Poly", kind="enum", opts=POLY, default=0),
    dict(key="octave", name="Octave", short="Oct", kind="int", mn=-3, mx=3, default=0),
    dict(key="glide", name="Glide", short="Glide", kind="float", mn=0, mx=1, default=0),
    dict(key="key_lo", name="Key Low", short="KeyLo", kind="int", mn=0, mx=127, default=0),
    dict(key="key_hi", name="Key High", short="KeyHi", kind="int", mn=0, mx=127, default=127),
    dict(key="midi_ch", name="MIDI Channel", short="Chan", kind="int", mn=1, mx=16, default=1),
    dict(key="route", name="FX Route", short="Route", kind="enum", opts=ROUTES, default=1),
    dict(key="send", name="FX Send", short="Send", kind="float", mn=0, mx=1, default=1, unit="%"),
    dict(key="flavour", name="Flavour", short="Flav", kind="float", mn=0, mx=1, default=0, unit="%"),
    dict(key="osc_mode", name="Osc Character", short="Char", kind="enum", opts=OSC_MODES, default=0),
    dict(key="scale", name="Scale", short="Scale", kind="enum", opts=SCALES, default=0),
    dict(key="chord", name="Chord", short="Chord", kind="enum", opts=CHORDS, default=0),

    dict(key="osc1_table", name="Osc 1 Wavetable", short="Table", kind="enum", opts=TABLE_NAMES, default=0, tables=True),
    dict(key="osc1_wave", name="Osc 1 Wave Position", short="Wave", kind="int", mn=0, mx=63, default=63, big=True, live=True, viz="wave1", card="cards.js#osc1_wave_card"),
    dict(key="osc1_semi", name="Osc 1 Semitone", short="Semi", kind="int", mn=-48, mx=48, default=0, unit="st", big=True),
    dict(key="osc1_fine", name="Osc 1 Fine Tune", short="Fine", kind="int", mn=-100, mx=100, default=0, unit="cents"),
    dict(key="osc1_level", name="Osc 1 Level", short="Level", kind="float", mn=0, mx=1, default=1, unit="%", viz="fader"),
    dict(key="osc1_wenv", name="Osc 1 Wave Env", short="WEnv", kind="float", mn=-1, mx=1, default=0, unit="%"),
    dict(key="osc1_vel", name="Osc 1 Velocity to Wave", short="Vel", kind="float", mn=-1, mx=1, default=0, unit="%"),
    dict(key="osc1_kbd", name="Osc 1 Keytrack", short="Kbd", kind="enum", opts=ONOFF, default=1, viz="switch"),

    dict(key="osc2_table", name="Osc 2 Wavetable", short="Table", kind="enum", opts=TABLE_NAMES, default=0, tables=True),
    dict(key="osc2_wave", name="Osc 2 Wave Position", short="Wave", kind="int", mn=0, mx=63, default=63, big=True, live=True, viz="wave2", card="cards.js#osc2_wave_card"),
    dict(key="osc2_semi", name="Osc 2 Semitone", short="Semi", kind="int", mn=-48, mx=48, default=0, unit="st", big=True),
    dict(key="osc2_fine", name="Osc 2 Fine Tune", short="Fine", kind="int", mn=-100, mx=100, default=0, unit="cents"),
    dict(key="osc2_level", name="Osc 2 Level", short="Level", kind="float", mn=0, mx=1, default=0, unit="%", viz="fader"),
    dict(key="osc2_wenv", name="Osc 2 Wave Env", short="WEnv", kind="float", mn=-1, mx=1, default=0, unit="%"),
    dict(key="osc2_vel", name="Osc 2 Velocity to Wave", short="Vel", kind="float", mn=-1, mx=1, default=0, unit="%"),
    dict(key="osc2_kbd", name="Osc 2 Keytrack", short="Kbd", kind="enum", opts=ONOFF, default=1, viz="switch"),
    dict(key="osc2_link", name="Link to Osc 1", short="Link", kind="enum", opts=ONOFF, default=0, viz="switch"),

    dict(key="xmod_type", name="Cross Mod Type", short="XMode", kind="enum", opts=XMOD_TYPES, default=0),
    dict(key="xmod", name="Cross Mod Amount", short="XMod", kind="float", mn=0, mx=1, default=0, unit="%"),

    dict(key="dirt_type", name="Dirt Type", short="Dirt", kind="enum", opts=DIRT_TYPES, default=0),
    dict(key="dirt", name="Dirt Level", short="Amt", kind="float", mn=0, mx=1, default=0, unit="%"),
    dict(key="drv_type", name="Drive Type", short="Type", kind="enum", opts=DRIVE_TYPES, default=0),
    dict(key="drive", name="Drive", short="Drive", kind="float", mn=0, mx=1, default=0, unit="%"),
    dict(key="drv_pos", name="Drive Position", short="Pos", kind="enum", opts=["Pre", "Post"], default=0),

    dict(key="ftype", name="Filter Type", short="Type", kind="enum", opts=FILTER_TYPES, default=0),
    dict(key="cutoff", name="Cutoff", short="Cut", kind="float", mn=0, mx=127, default=127, viz="filter_cutoff"),
    dict(key="res", name="Resonance", short="Res", kind="float", mn=0, mx=1, default=0, viz="filter_res"),
    dict(key="fenv", name="Filter Env Amount", short="Env", kind="float", mn=-1, mx=1, default=0, unit="%"),
    dict(key="fkbd", name="Filter Keytrack", short="Kbd", kind="float", mn=0, mx=2, default=.5, unit="%"),
    dict(key="fvel", name="Filter Velocity", short="Vel", kind="float", mn=-1, mx=1, default=.2, unit="%"),

    dict(key="e1_a", name="Wave Env Attack", short="Atk", kind="float", mn=0, mx=1, default=0, viz="e1_attack"),
    dict(key="e1_d", name="Wave Env Decay", short="Dec", kind="float", mn=0, mx=1, default=.5, viz="e1_decay"),
    dict(key="e1_s", name="Wave Env Sustain", short="Sus", kind="float", mn=0, mx=1, default=0, viz="e1_sustain"),
    dict(key="e1_r", name="Wave Env Release", short="Rel", kind="float", mn=0, mx=1, default=.3, viz="e1_release"),
    dict(key="e2_a", name="Filter Env Attack", short="Atk", kind="float", mn=0, mx=1, default=0, viz="e2_attack"),
    dict(key="e2_d", name="Filter Env Decay", short="Dec", kind="float", mn=0, mx=1, default=.4, viz="e2_decay"),
    dict(key="e2_s", name="Filter Env Sustain", short="Sus", kind="float", mn=0, mx=1, default=.2, viz="e2_sustain"),
    dict(key="e2_r", name="Filter Env Release", short="Rel", kind="float", mn=0, mx=1, default=.3, viz="e2_release"),
    dict(key="e3_a", name="Amp Env Attack", short="Atk", kind="float", mn=0, mx=1, default=0, viz="e3_attack"),
    dict(key="e3_d", name="Amp Env Decay", short="Dec", kind="float", mn=0, mx=1, default=.3, viz="e3_decay"),
    dict(key="e3_s", name="Amp Env Sustain", short="Sus", kind="float", mn=0, mx=1, default=1, viz="e3_sustain"),
    dict(key="e3_r", name="Amp Env Release", short="Rel", kind="float", mn=0, mx=1, default=.25, viz="e3_release"),

    dict(key="e4_a", name="Aux Env Attack", short="Atk", kind="float", mn=0, mx=1, default=0, viz="e4_attack"),
    dict(key="e4_d", name="Aux Env Decay", short="Dec", kind="float", mn=0, mx=1, default=.4, viz="e4_decay"),
    dict(key="e4_s", name="Aux Env Sustain", short="Sus", kind="float", mn=0, mx=1, default=.5, viz="e4_sustain"),
    dict(key="e4_r", name="Aux Env Release", short="Rel", kind="float", mn=0, mx=1, default=.3, viz="e4_release"),
    # Dedicated Aux Env routes (docs/04 section 3.10), beside the matrix.
    dict(key="e4_dst1", name="Aux Env Target 1", short="Dst1", kind="enum", opts=MOD_TARGETS, default=0),
    dict(key="e4_amt1", name="Aux Env Amount 1", short="Amt1", kind="float", mn=-1, mx=1, default=0, unit="%", card="cards.js#aux_amt_card"),
    dict(key="e4_dst2", name="Aux Env Target 2", short="Dst2", kind="enum", opts=MOD_TARGETS, default=0),
    dict(key="e4_amt2", name="Aux Env Amount 2", short="Amt2", kind="float", mn=-1, mx=1, default=0, unit="%", card="cards.js#aux_amt_card"),

    dict(key="amp_vel", name="Amp Velocity", short="Vel", kind="float", mn=0, mx=1, default=.5, unit="%"),
    dict(key="pan_mode", name="Pan Mode", short="PanM", kind="enum", opts=PAN_MODES, default=0),
    dict(key="pan", name="Pan", short="Pan", kind="float", mn=-1, mx=1, default=0, unit="%"),

    dict(key="at_wave", name="Aftertouch to Wave", short="ATWav", kind="float", mn=-1, mx=1, default=0, unit="%"),
    dict(key="at_cut", name="Aftertouch to Cutoff", short="ATCut", kind="float", mn=-1, mx=1, default=0, unit="%"),
    dict(key="at_vib", name="Aftertouch to Vibrato", short="ATVib", kind="float", mn=0, mx=1, default=0, unit="%"),

    dict(key="lfo1_shape", name="LFO 1 Shape", short="Shape", kind="enum", opts=LFO_SHAPES, default=1, viz="waveform"),
    dict(key="lfo1_rate", name="LFO 1 Rate", short="Rate", kind="float", mn=0, mx=1, default=.4),
    dict(key="lfo1_sync", name="LFO 1 Sync", short="Sync", kind="enum", opts=ONOFF, default=0, viz="switch"),
    dict(key="lfo1_div", name="LFO 1 Division", short="Div", kind="enum", opts=LFO_DIVS, default=7),
    dict(key="lfo1_phase", name="LFO 1 Phase Mode", short="Phase", kind="enum", opts=LFO_PHASE, default=1),
    dict(key="lfo1_fade", name="LFO 1 Fade In", short="Fade", kind="float", mn=0, mx=1, default=0),
    dict(key="lfo1_mw", name="LFO 1 Mod Wheel Link", short="MW", kind="enum", opts=ONOFF, default=0, viz="switch"),
    dict(key="lfo1_pitch", name="LFO 1 to Pitch", short="Pitch", kind="float", mn=-1, mx=1, default=0, unit="%"),

    dict(key="lfo2_shape", name="LFO 2 Shape", short="Shape", kind="enum", opts=LFO_SHAPES, default=1, viz="waveform"),
    dict(key="lfo2_rate", name="LFO 2 Rate", short="Rate", kind="float", mn=0, mx=1, default=.4),
    dict(key="lfo2_sync", name="LFO 2 Sync", short="Sync", kind="enum", opts=ONOFF, default=0, viz="switch"),
    dict(key="lfo2_div", name="LFO 2 Division", short="Div", kind="enum", opts=LFO_DIVS, default=7),
    dict(key="lfo2_phase", name="LFO 2 Phase Mode", short="Phase", kind="enum", opts=LFO_PHASE, default=1),
    dict(key="lfo2_fade", name="LFO 2 Fade In", short="Fade", kind="float", mn=0, mx=1, default=0),
    dict(key="lfo2_mw", name="LFO 2 Mod Wheel Link", short="MW", kind="enum", opts=ONOFF, default=0, viz="switch"),
    dict(key="lfo2_pitch", name="LFO 2 to Pitch", short="Pitch", kind="float", mn=-1, mx=1, default=0, unit="%"),
]
for m in range(1, 9):
    LAYER += [
        dict(key=f"m{m}_src", name=f"Mod {m} Source", short="Src", kind="enum", opts=MOD_SOURCES, default=0),
        dict(key=f"m{m}_dst", name=f"Mod {m} Target", short="Dst", kind="enum", opts=MOD_TARGETS, default=0),
        dict(key=f"m{m}_amt", name=f"Mod {m} Amount", short="Amt", kind="float", mn=-1, mx=1, default=0, unit="%", step=0.001,
             card="cards.js#mod_amt_card"),
        dict(key=f"m{m}_via", name=f"Mod {m} Via", short="Via", kind="enum", opts=MOD_SOURCES, default=0),
        dict(key=f"m{m}_via_depth", name=f"Mod {m} Via Depth", short="VDep", kind="float", mn=0, mx=1, default=1, unit="%", step=0.01),
    ]

GLOBAL = [
    dict(key="master_vol", name="Master Volume", short="Vol", kind="float", mn=0, mx=1, default=.8, unit="%"),
    dict(key="layer_count", name="Layers", short="Lyrs", kind="int", mn=1, mx=4, default=1),
    dict(key="layer_mode", name="Layer Mode", short="Mode", kind="enum", opts=LAYER_MODES, default=0),
    dict(key="edit_layer", name="Edit Layer", short="Edit", kind="int", mn=1, mx=4, default=1),
    dict(key="voices", name="Voices", short="Voice", kind="int", mn=1, mx=16, default=8),
    dict(key="bend_range", name="Bend Range", short="Bend", kind="int", mn=0, mx=24, default=2, unit="st"),
    dict(key="mpe", name="MPE", short="MPE", kind="enum", opts=ONOFF, default=0),
    dict(key="flavour_global", name="Flavour (Global)", short="Flav", kind="float", mn=0, mx=1, default=.15, unit="%"),
    dict(key="fx_chain", name="FX Chain", short="Chain", kind="enum", opts=FX_CHAIN, default=1),
    dict(key="fx1_type", name="FX 1 Type", short="Type", kind="enum", opts=FX_TYPES, default=0),
    dict(key="fx1_mix", name="FX 1 Mix", short="Mix", kind="float", mn=0, mx=1, default=.3, unit="%"),
] + [dict(key=f"fx1_p{i}", name=f"FX 1 P{i}", short=f"P{i}", kind="float", mn=0, mx=1, default=0,
          card="cards.js#fx_param_card")
     for i in range(1, 7)] + [
    dict(key="fx2_type", name="FX 2 Type", short="Type", kind="enum", opts=FX_TYPES, default=0),
    dict(key="fx2_mix", name="FX 2 Mix", short="Mix", kind="float", mn=0, mx=1, default=.3, unit="%"),
] + [dict(key=f"fx2_p{i}", name=f"FX 2 P{i}", short=f"P{i}", kind="float", mn=0, mx=1, default=0,
          card="cards.js#fx_param_card")
     for i in range(1, 7)] + [
    dict(key="rand_amt", name="Random Amount", short="Rand", kind="float", mn=0, mx=1, default=.25, unit="%"),
    # Per-patch output trim; the factory bank is level-calibrated with it
    # (tools/emulator/calibrate_levels.py).  Appended so no index moves.
    dict(key="patch_gain", name="Patch Gain", short="Gain", kind="float", mn=-24, mx=24, default=0,
         unit="dB", step=0.1),
]

# Every key the factory bank uses must resolve through these two lists.
LAYER_BY_KEY = {p["key"]: i for i, p in enumerate(LAYER)}
GLOBAL_BY_KEY = {p["key"]: i for i, p in enumerate(GLOBAL)}
ALL_KEYS = [p["key"] for p in GLOBAL] + [f"l{n}_{p['key']}" for n in range(1, 5) for p in LAYER]
ALL_KEYS += ["preset", "preset_name", "preset_count", "init_layer", "randomize", "rand_amt"]

# Remote UI display mappings (tools/gen/gen.py -> assets/ui_meta.json).  Each
# names the curve src/dsp applies, so the browser can show real units where
# the device can only show the stored value.  Kinds are implemented in
# src/web/format.js.
DISPLAY = {
    "cutoff": {"kind": "note_hz"},                      # voice.c: am_note_hz(cutoff)
    "glide": {"kind": "glide_time"},                    # util.h: 5 s * x^2
    "lfo1_rate": {"kind": "lfo_hz"}, "lfo2_rate": {"kind": "lfo_hz"},
    "lfo1_fade": {"kind": "fade_time"}, "lfo2_fade": {"kind": "fade_time"},  # 0 = no fade
    "key_lo": {"kind": "note_name"}, "key_hi": {"kind": "note_name"},
    "res": {"kind": "percent"},
    # docs/07: slots 61-63 are the same Triangle/Square/Saw in every table.
    "osc1_wave": {"kind": "wave_pos"}, "osc2_wave": {"kind": "wave_pos"},
}
for _e in ("e1", "e2", "e3", "e4"):
    for _s in ("a", "d", "r"):
        DISPLAY[f"{_e}_{_s}"] = {"kind": "env_time"}    # util.h: 20 s * x^3
    DISPLAY[f"{_e}_s"] = {"kind": "percent"}

# FX slot knobs per type, mirroring src/dsp/fx.c (lin/exps/enumv) and
# docs/11 section 4.  Defaults come from fx.c FX_DEFAULTS.
def _lin(name, short, lo, hi, unit=""):
    return dict(name=name, short=short, display=dict(kind="lin", lo=lo, hi=hi, unit=unit))


def _exp(name, short, lo, hi, unit=""):
    return dict(name=name, short=short, display=dict(kind="exp", lo=lo, hi=hi, unit=unit))


def _sel(name, short, opts):
    return dict(name=name, short=short, options=list(opts))


def _pct(name, short):
    return dict(name=name, short=short, display=dict(kind="percent"))


SYNC_FLANGER = ["Off", "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars"]
SYNC_TREM = ["Off", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar"]
SYNC_DELAY = ["Off", "1/32", "1/16T", "1/16", "1/16d", "1/8T", "1/8", "1/8d",
              "1/4T", "1/4", "1/4d", "1/2", "1 bar"]
FX_PARAMS = {
    "Chorus": [_exp("Rate", "Rate", .05, 5, "Hz"), _pct("Depth", "Depth"),
               _lin("Delay", "Delay", 2, 30, "ms"), _lin("Feedback", "Fdbk", -90, 90, "%"),
               _lin("Spread", "Sprd", 0, 180, "°"), _sel("Mode", "Mode", ["Chorus", "Ensemble"])],
    "Flanger": [_exp("Rate", "Rate", .02, 10, "Hz"), _pct("Depth", "Depth"),
                _lin("Feedback", "Fdbk", -95, 95, "%"), _exp("Delay", "Delay", .1, 10, "ms"),
                _lin("Spread", "Sprd", 0, 180, "°"), _sel("Sync", "Sync", SYNC_FLANGER)],
    "Phaser": [_exp("Rate", "Rate", .02, 10, "Hz"), _pct("Depth", "Depth"),
               _lin("Feedback", "Fdbk", -95, 95, "%"), _sel("Stages", "Stage", ["4", "6", "8", "12"]),
               _exp("Center", "Centr", 100, 4000, "Hz"), _lin("Spread", "Sprd", 0, 180, "°")],
    "Tremolo": [_exp("Rate", "Rate", .1, 20, "Hz"), _pct("Depth", "Depth"),
                _sel("Shape", "Shape", ["Sine", "Tri", "Square", "SawUp", "SawDn", "Pump"]),
                _sel("Sync", "Sync", SYNC_TREM), _pct("Stereo", "Ster"),
                _lin("Phase", "Phase", 0, 360, "°")],
    "Drive": [_lin("Gain", "Gain", 0, 36, "dB"), _lin("Tone", "Tone", -6, 6, "dB"),
              _sel("Type", "Type", ["Soft", "Tube", "Hard", "Fold", "Crush"]),
              _lin("Output", "Out", -24, 0, "dB"), _exp("Low Cut", "LoCut", 20, 500, "Hz"),
              _pct("Crush", "Crush")],
    "Comp": [_lin("Threshold", "Thr", -40, 0, "dB"), _exp("Ratio", "Ratio", 1, 20, ":1"),
             _exp("Attack", "Atk", .1, 100, "ms"), _exp("Release", "Rel", 10, 1000, "ms"),
             _lin("Makeup", "Make", 0, 24, "dB"), _lin("Knee", "Knee", 0, 12, "dB")],
    "EQ": [_lin("Low", "Low", -15, 15, "dB"), _lin("Mid", "Mid", -15, 15, "dB"),
           _exp("Mid Freq", "MidF", 200, 8000, "Hz"), _lin("High", "High", -15, 15, "dB"),
           _exp("Mid Q", "Q", .3, 4), _exp("Low Cut", "LoCut", 20, 300, "Hz")],
    "Delay": [_exp("Time", "Time", 1, 2000, "ms"), _sel("Sync", "Sync", SYNC_DELAY),
              _pct("Feedback", "Fdbk"), _lin("Tone", "Tone", -100, 100, "%"),
              _pct("Ping-Pong", "PingP"), _pct("Mod", "Mod")],
    "Reverb": [_pct("Size", "Size"), _exp("Decay", "Decay", .2, 20, "s"),
               _pct("Damping", "Damp"), _lin("Pre-Delay", "PreD", 0, 200, "ms"),
               _pct("Mod", "Mod"), _pct("Width", "Width")],
    "Shimmer": [_pct("Size", "Size"), _exp("Decay", "Decay", .2, 20, "s"),
                _pct("Damping", "Damp"), _pct("Shimmer", "Shim"),
                _sel("Interval", "Intvl", ["-12 st", "+5 st", "+7 st", "+12 st", "+19 st", "+24 st"]),
                _pct("Mod", "Mod")],
    "Trance Gate": [_sel("Pattern", "Pattern", ["Classic 8ths", "Offbeat 8ths", "16th Pulse",
                                                "32nd Stutter", "Rolling Trance", "Broken Rave",
                                                "Long Gate"]),
                    _sel("Timing", "Grid", ["1/8", "1/16", "1/32"]),
                    _lin("Attack", "Atk", 0, 50, "ms"), _lin("Release", "Rel", 0, 50, "ms"),
                    _pct("Gate Length", "Length"), _pct("Floor", "Floor")],
}

# Mod-matrix target index -> the layer parameter whose knob it moves (None for
# targets with no single knob).  Order matches MOD_TARGETS / voice.c DST_*.
MOD_TARGET_KEYS = [
    None, None, "osc1_semi", "osc2_semi", "osc1_wave", "osc2_wave", "osc1_level",
    "osc2_level", "dirt", "cutoff", "res", "drive", "pan", "vol",
    "lfo1_rate", "lfo2_rate", None, None, "e1_d", "e2_d", "e3_d",
    "e1_a", "e3_r", "glide", "send", "xmod",
]
assert len(MOD_TARGET_KEYS) == len(MOD_TARGETS)

# How far amount 1.0 moves that knob, in the knob's own units (voice.c adds
# mdst[...] * scale to the stored value).  "mul" targets scale the value:
# Amp Level is vol * (1 + m), FX Send is send * (1 + m).
MOD_TARGET_SCALE = [
    None, None, 48, 48, 63, 63, 1, 1, 1, 72, 1, 1, 1, "mul",
    0.5, 0.5, None, None, 1, 1, 1, 1, 1, 1, "mul", 1,
]
assert len(MOD_TARGET_SCALE) == len(MOD_TARGETS)

# Source signal ranges from voice.c (src[SRC_*]): bipolar sources swing
# -1..+1, the rest 0..1.  Groups pick the colour family in the UI.
MOD_SOURCE_BIPOLAR = {"LFO1", "LFO2", "LFO1xMW", "LFO2xMW", "Key", "Pitch Bend",
                      "Random Bi", "Voice #", "Layer Alt", "Flavour"}
MOD_SOURCE_GROUP = {
    "Wave Env": "env", "Filter Env": "env", "Amp Env": "env", "Aux Env": "env",
    "LFO1": "lfo", "LFO2": "lfo", "LFO1xMW": "lfo", "LFO2xMW": "lfo",
    "Random": "random", "Random Bi": "random", "Voice #": "random",
    "Layer Alt": "random", "Constant": "random", "Flavour": "random",
}

CATEGORY_NAMES = {"IN": "Init", "BS": "Bass", "LD": "Lead", "PD": "Pad", "KY": "Keys",
                  "PL": "Pluck", "FX": "Effect", "DR": "Drone", "ML": "Multi-layer"}
