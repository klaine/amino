"""Validate the compiled Amino UI contract (chain_params + ui_hierarchy).

Run from the repository root:  py -3 tests/contract/validate_contract.py
Exits non-zero and prints every violation.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "src", "dsp", "contract_gen.c")
LIMIT = 64 * 1024
KINDS = {"float", "int", "enum", "string", "canvas"}
sys.path.insert(0, os.path.join(ROOT, "tools", "gen"))
import params as PARAMS
problems = []


def fail(msg):
    problems.append(msg)


def load():
    text = open(SRC, encoding="utf-8").read()
    parts = re.findall(r"(?:static )?const char (\w+)\[\] =\n(.*?);\n", text, re.S)
    out = {}
    for name, body in parts:
        raw = bytes(int(x, 16) for x in re.findall(r"\\x([0-9a-f]{2})", body))
        out[name] = raw.decode("utf-8")
    if "amino_chain_params_json" not in out:
        fail("contract_gen.c has no amino_chain_params_json")
    if "amino_ui_hierarchy_json" not in out:
        fail("contract_gen.c has no amino_ui_hierarchy_json")
    if "amino_ui_pages_json" not in out:
        fail("contract_gen.c has no amino_ui_pages_json")
    return out


def check_params(cp):
    keys = set()
    for p in cp:
        key = p.get("key")
        if not key:
            fail("chain param without key")
            continue
        if key in keys:
            fail("duplicate chain key %s" % key)
        keys.add(key)
        kind = p.get("type")
        if kind not in KINDS:
            fail("%s: bad type %r" % (key, kind))
        short = p.get("short_name")
        if short is not None and len(short) > 5:
            fail("%s: short_name %r longer than 5" % (key, short))
        if kind == "enum":
            opts = p.get("options", [])
            if len(opts) > 128:
                fail("%s: %d options > 128" % (key, len(opts)))
            so = p.get("short_options")
            if so is not None and len(so) != len(opts):
                fail("%s: short_options length %d != %d" % (key, len(so), len(opts)))
            if so is not None and any(len(label) > 4 for label in so):
                fail("%s: enum short_options must be <= 4 chars" % key)
            if so is not None and len(set(so)) != len(so):
                fail("%s: enum short_options are ambiguous" % key)
        elif kind in ("float", "int"):
            if "min" not in p or "max" not in p:
                fail("%s: missing min/max" % key)
            elif "default" in p and (p["min"] > p["default"] or p["default"] > p["max"]):
                fail("%s: default outside range" % key)
    return keys


def main():
    out = load()
    if problems:
        return report()
    chain_text = out["amino_chain_params_json"]
    ui_text = out["amino_ui_hierarchy_json"]
    pages_text = out["amino_ui_pages_json"]
    if pages_text != ui_text:
        fail("ui_pages and ui_hierarchy should share the generated page tree")
    total = len(chain_text.encode()) + len(ui_text.encode())
    if total >= LIMIT:
        fail("contract is %d bytes, limit %d" % (total, LIMIT))

    cp = json.loads(chain_text)
    ui = json.loads(ui_text)
    if "ui_hierarchy" in ui:
        fail("get_param ui_hierarchy must return the hierarchy object directly")
    levels = ui.get("levels")
    if not isinstance(levels, dict):
        fail("ui_hierarchy.levels missing")
        return report()

    keys = check_params(cp)
    meta = {p["key"]: p for p in cp}
    factory_dir = os.path.join(ROOT, "presets", "factory")
    factory_count = sum(name.endswith(".json") for name in os.listdir(factory_dir))
    if meta.get("preset", {}).get("max") != factory_count - 1:
        fail("preset selector max must include every factory JSON preset")
    if "Trance Gate" not in meta.get("fx1_type", {}).get("options", []):
        fail("FX 1 type options are missing Trance Gate")
    for i in range(1, 9):
        key = "m%d_amt" % i
        if meta.get(key, {}).get("step") != 0.001:
            fail("%s must publish a 0.1%% fine adjustment step" % key)
        via_depth = meta.get("m%d_via_depth" % i, {})
        if (via_depth.get("min"), via_depth.get("max"), via_depth.get("default")) != (0, 1, 1):
            fail("m%d_via_depth must be 0..1 and default to 1" % i)
    matrix = levels.get("modmatrix", {})
    if matrix.get("knobs") != []:
        fail("Pad Matrix should be a custom page, not a duplicate knob grid")
    matrix_param = next((p for p in matrix.get("params", []) if p.get("key") == "mod_matrix"), {})
    canvas_meta = meta.get("mod_matrix", {})
    if canvas_meta.get("type") != "canvas" or not canvas_meta.get("as_page"):
        fail("mod_matrix must be a custom canvas page")
    if "mod_matrix_state" not in canvas_meta.get("extra_keys", []):
        fail("Pad Matrix must read the bundled mod_matrix_state value")

    root = levels.get("root", {})
    expected_main = ["cutoff", "res", "osc1_wave", "osc2_wave",
                     "fenv", "e3_a", "e3_r", "flavour"]
    if root.get("knobs") != expected_main:
        fail("Main/root knob order does not match docs/05 §4.2")
    if any(k in root for k in ("list_param", "count_param", "name_param")):
        fail("Main/root must not also host the preset browser")
    presets = levels.get("presets", {})
    if (presets.get("list_param"), presets.get("count_param"),
            presets.get("name_param")) != ("preset", "preset_count", "preset_name"):
        fail("presets level is missing the factory preset browser selectors")

    for key in ("osc1_wave", "osc2_wave", "osc1_semi", "osc2_semi"):
        if not meta.get(key, {}).get("display") == "big":
            fail("%s must use big value display (docs/05 §7)" % key)
    for key, script, extra in (("osc1_wave", "cards.js#osc1_wave_card", "wave_view"),
                               ("osc2_wave", "cards.js#osc2_wave_card", "wave_view2")):
        entry = meta.get(key, {})
        if entry.get("card_script") != script or entry.get("card_w", 0) <= 0 or entry.get("card_h", 0) <= 0:
            fail("%s is missing its touch/turn wave preview card" % key)
        if extra not in entry.get("viz", {}).get("extra_keys", []):
            fail("%s wave card depends on missing %s viz data" % (key, extra))
    if not os.path.exists(os.path.join(ROOT, "src", "cards.js")):
        fail("src/cards.js must ship for the oscillator preview cards")
    if "ftype" not in meta.get("cutoff", {}).get("viz", {}).get("extra_keys", []):
        fail("Main filter viz must read ftype for its response mode")
    # Viz extra keys reach the Remote UI; schwung-manager reads at most four
    # per declaration and sixteen per component (Schwung docs/MODULES.md).
    declared = []
    for entry in meta.values():
        extras = entry.get("viz", {}).get("extra_keys", [])
        if len(extras) > 4:
            fail("%s declares more than four viz extra keys" % entry.get("key"))
        declared += [k for k in extras if k not in declared]
    if len(declared) > 16:
        fail("more than sixteen distinct viz extra keys (%d)" % len(declared))
    if "rui_live" not in declared:
        fail("Remote UI telemetry (rui_live) must be declared as a viz extra key")
    for key in ("osc1_wenv", "osc1_vel", "osc2_wenv", "osc2_vel", "fenv",
                "fvel", "pan", "at_wave", "at_cut", "at_vib", "xmod",
                "dirt", "drive") + tuple("m%d_amt" % i for i in range(1, 9)):
        if meta.get(key, {}).get("unit") != "%":
            fail("%s must publish its percent value unit" % key)

    expected_split = ["l1_key_lo", "l1_key_hi", "l2_key_lo", "l2_key_hi",
                      "l3_key_lo", "l3_key_hi", "l4_key_lo", "l4_key_hi"]
    split = levels.get("split", {})
    if split.get("knobs") != expected_split:
        fail("Key Split knob order does not match docs/05 §4.17")
    for entry in split.get("params", []):
        if entry.get("type") != "note" or entry.get("mode") != "multi":
            fail("%s must be a multi-note selector" % entry.get("key"))
    for page, unit_keys in (("lmix", ["l%d_vol" % i for i in range(1, 5)]),
                            ("lroute", ["l%d_send" % i for i in range(1, 5)])):
        entries = {e.get("key"): e for e in levels.get(page, {}).get("params", [])}
        for key in unit_keys:
            if entries.get(key, {}).get("unit") != "%":
                fail("%s must retain its percent unit on %s" % (key, page))

    children = [level for level in levels.values() if "child_count" in level]
    if [level.get("name") for level in children] != ["Layer Tools"]:
        fail("only Layer Tools should use child-key expansion")
    for page in ("osc1", "osc2", "wenv", "mixer", "filter", "amp", "play",
                 "voice", "lfo1", "lfo2", "mod12", "mod34", "mod56", "mod78"):
        if "child_count" in levels.get(page, {}):
            fail("%s should use focused-layer aliases, not a child page" % page)
    copy_keys = children[0].get("child_copy_keys", []) if children else []
    if not copy_keys:
        fail("per-layer levels need child_copy_keys for complete layer copy")
    for level in children:
        if level.get("child_copy_keys") != copy_keys:
            fail("%s has inconsistent child_copy_keys" % level.get("name"))
    forbidden_copy = {"on", "key_lo", "key_hi", "midi_ch"}
    if forbidden_copy.intersection(copy_keys):
        fail("child_copy_keys must exclude on/key range/MIDI channel")
    expected_copy = [p["key"] for p in PARAMS.LAYER
                     if p["key"] not in forbidden_copy]
    if copy_keys != expected_copy:
        fail("child_copy_keys must contain every other layer parameter in order")

    expected_pages = {
        "osc1": ["osc1_table", "osc1_wave", "osc1_semi", "osc1_fine",
                 "osc1_level", "osc1_wenv", "osc1_vel", "osc1_kbd"],
        "osc2": ["osc2_table", "osc2_wave", "osc2_semi", "osc2_fine",
                 "osc2_level", "osc2_link", "xmod_type", "xmod"],
        "wenv": ["e1_a", "e1_d", "e1_s", "e1_r", "osc1_wave",
                 "osc1_wenv", "osc2_wave", "osc2_wenv"],
        "mixer": ["osc1_level", "osc2_level", "dirt_type", "dirt",
                  "drv_type", "drive", "drv_pos", "vol"],
        "filter": ["cutoff", "res", "fenv", "ftype", "e2_a", "e2_d",
                   "e2_s", "e2_r"],
        "amp": ["e3_a", "e3_d", "e3_s", "e3_r", "vol", "amp_vel",
                "pan_mode", "pan"],
        "play": ["osc1_vel", "osc2_vel", "fvel", "amp_vel", "fkbd",
                 "at_wave", "at_cut", "at_vib"],
        "voice": ["octave", "poly", "glide", "flavour", "scale", "chord",
                  "osc_mode", "send"],
        "lfo1": ["lfo1_shape", "lfo1_rate", "lfo1_div", "lfo1_sync",
                 "lfo1_phase", "lfo1_fade", "lfo1_mw", "lfo1_pitch"],
        "lfo2": ["lfo2_shape", "lfo2_rate", "lfo2_div", "lfo2_sync",
                 "lfo2_phase", "lfo2_fade", "lfo2_mw", "lfo2_pitch"],
        "fx": ["fx1_type", "fx1_mix", "fx2_type", "fx2_mix", "fx_chain",
               "l1_route", "l1_send", "master_vol"],
        "aux": ["e4_a", "e4_d", "e4_s", "e4_r",
                "e4_dst1", "e4_amt1", "e4_dst2", "e4_amt2"],
        "fx1": ["fx1_type", "fx1_mix"],
        "fx2": ["fx2_type", "fx2_mix"],
        "layers": ["layer_count", "layer_mode", "edit_layer", "voices",
                   "master_vol", "bend_range", "mpe", "flavour_global"],
        "lmix": ["l%d_vol" % i for i in range(1, 5)] +
                ["l%d_on" % i for i in range(1, 5)],
        "lroute": ["l%d_route" % i for i in range(1, 5)] +
                  ["l%d_send" % i for i in range(1, 5)],
    }
    for i in range(1, 9, 2):
        expected_pages["mod%d%d" % (i, i + 1)] = [
            "m%d_%s" % (slot, field)
            for slot in (i, i + 1) for field in ("src", "dst", "amt", "via")]
    # FX 1 / FX 2: one page per effect type, gated on that slot's type and
    # labelled with the effect's own knob names (docs/05 section 4.13).
    for n in (1, 2):
        gated = {}
        for lid, level in levels.items():
            gate = level.get("visible_if") or {}
            if gate.get("param") == "fx%d_type" % n:
                gated[gate.get("equals")] = (lid, level)
        if sorted(gated) != list(range(len(PARAMS.FX_TYPES))):
            fail("fx%d needs exactly one page per FX type" % n)
            continue
        if gated[0][1].get("name") != "FX %d Off" % n:
            fail("%s should be named FX %d Off" % (gated[0][0], n))
        for t, name in enumerate(PARAMS.FX_TYPES[1:], 1):
            lid, level = gated[t]
            want = ["fx%d_type" % n, "fx%d_mix" % n] + ["fx%d_p%d" % (n, i) for i in range(1, 7)]
            if level.get("knobs") != want:
                fail("%s knob order does not match docs/05" % lid)
            if level.get("name") != "FX %d %s" % (n, name):
                fail("%s should be named after its effect" % lid)
            labels = [e.get("short_name") for e in level.get("params", [])]
            if len(labels) != 6 or not all(labels) or any(l.startswith("P") and l[1:].isdigit()
                                                          for l in labels):
                fail("%s must label P1-P6 with the effect's knob names" % lid)
    for n in (1, 2):
        for i in range(1, 7):
            if meta.get("fx%d_p%d" % (n, i), {}).get("card_script") != "cards.js#fx_param_card":
                fail("fx%d_p%d is missing its real-unit value card" % (n, i))
    for i in range(1, 9):
        if meta.get("m%d_amt" % i, {}).get("card_script") != "cards.js#mod_amt_card":
            fail("m%d_amt is missing its route card (docs/05 section 4.12)" % i)
    cards_text = open(os.path.join(ROOT, "src", "cards.js"), encoding="utf-8").read()
    for ref in ("fx_param_card", "mod_amt_card", "aux_amt_card", "var META = {\"fxTypes\""):
        if ref not in cards_text:
            fail("src/cards.js is missing %s (run tools/gen/gen.py)" % ref)

    for page, expected in expected_pages.items():
        if levels.get(page, {}).get("knobs") != expected:
            fail("%s knob order does not match docs/05" % page)
    help_path = os.path.join(ROOT, "src", "help.json")
    with open(help_path, "r", encoding="utf-8") as f:
        help_tree = json.load(f)
    midi_cc_help = next((c for c in help_tree.get("children", [])
                         if c.get("title") == "MIDI CC"), None)
    if midi_cc_help is None:
        fail("on-device help must list supported MIDI CC mappings")
    else:
        cc_lines = midi_cc_help.get("lines", [])
        for cc in ("CC1:", "CC2:", "CC4:", "CC11:", "CC64:",
                   "CC74:", "CC120:", "CC121:", "CC123:"):
            if not any(line.startswith(cc) for line in cc_lines):
                fail("on-device MIDI CC help is missing %s" % cc)
    pages_help = next((c for c in help_tree.get("children", [])
                       if c.get("title") == "Pages"), {})
    help_pages = {c.get("title") for c in pages_help.get("children", [])}
    for level in levels.values():
        if level.get("knobs") or level.get("list_param") or level.get("items_param"):
            if level.get("name") not in help_pages:
                fail("help.json has no page leaf for %s" % level.get("name"))

    stack = [help_tree]
    while stack:
        node = stack.pop()
        if isinstance(node, dict):
            for line in node.get("lines", []):
                if len(line) > 20 or any(ord(c) > 127 for c in line):
                    fail("help line must be ASCII and <= 20 chars: %r" % line)
            stack.extend(v for v in node.values() if isinstance(v, (dict, list)))
        elif isinstance(node, list):
            stack.extend(node)

    def label(key, entry, scope):
        e = entry if isinstance(entry, dict) else {}
        if e.get("short_name"):
            return e["short_name"]
        if key in scope and scope[key].get("short_name"):
            return scope[key]["short_name"]
        if key in meta:
            return meta[key].get("short_name") or meta[key].get("name") or key
        return key

    gates = set()

    for lid, level in levels.items():
        if not isinstance(level, dict):
            fail("%s: level is not an object" % lid)
            continue
        params = level.get("params", [])
        knobs = level.get("knobs", [])
        localkeys = {}

        # Local param metadata (concrete keys and page overrides).
        for entry in params:
            if not isinstance(entry, dict):
                continue
            if "key" in entry:
                localkeys[entry["key"]] = entry
                if entry["key"] not in keys and "type" not in entry:
                    fail("%s: key %s has no chain metadata and no inline type"
                         % (lid, entry["key"]))
            elif "level" in entry:
                if entry["level"] not in levels:
                    fail("%s: nav target %s missing" % (lid, entry["level"]))
            else:
                fail("%s: param entry has neither key nor level" % lid)

        for knob in knobs:
            if isinstance(knob, dict) and knob.get("key") and knob.get("short_name"):
                localkeys[knob["key"]] = knob

        if "child_count" in level:
            if "child_key_template" not in level:
                fail("%s: child page without child_key_template" % lid)
            if level.get("child_index_param") and level.get("child_index_param") not in keys:
                fail("%s: child_index_param missing from chain" % lid)
            if "child_index_base" not in level:
                fail("%s: child page without child_index_base" % lid)

        # Knob keys must exist (chain key or local metadata) and be unique.
        seen = set()
        for k in knobs:
            key = k if isinstance(k, str) else k.get("key")
            if not key:
                fail("%s: knob without key" % lid)
                continue
            if key in seen:
                fail("%s: knob %s listed twice" % (lid, key))
            seen.add(key)
            if key not in keys and key not in localkeys:
                fail("%s: knob %s has no metadata" % (lid, key))

        # Selector keys must not also be knobs.
        for sel in ("list_param", "count_param", "name_param",
                    "items_param", "select_param"):
            if level.get(sel) in seen:
                fail("%s: selector %s listed in knobs" % (lid, level[sel]))

        # Labels: <= 5 chars, unique within the page, using actual knob order.
        labs = []
        for knob in knobs:
            key = knob if isinstance(knob, str) else knob.get("key")
            entry = localkeys.get(key, {})
            labs.append(label(key, entry, localkeys))
        for name in labs:
            if len(name) > 5:
                fail("%s: label %r longer than 5" % (lid, name))
        for name in set(labs):
            if labs.count(name) > 1:
                fail("%s: duplicate label %r" % (lid, name))

        # Viz groups: contiguous and inside one row of four.
        groups = {}
        for i, knob in enumerate(knobs):
            key = knob if isinstance(knob, str) else knob.get("key")
            src = localkeys.get(key, {})
            viz = src.get("viz") or meta.get(key, {}).get("viz")
            if not viz or "group" not in viz:
                continue
            groups.setdefault(viz["group"], []).append(i)
        for g, idx in groups.items():
            if len(idx) < 2:
                continue
            if idx != list(range(idx[0], idx[0] + len(idx))):
                fail("%s: viz group %s is not contiguous %s" % (lid, g, idx))
            if len({i // 4 for i in idx}) > 1:
                fail("%s: viz group %s crosses a row boundary %s" % (lid, g, idx))

        gate = level.get("visible_if")
        if gate:
            if "param" not in gate or "equals" not in gate and "greater_than" not in gate:
                fail("%s: malformed visible_if" % lid)
            else:
                gates.add(gate["param"])
                gp = meta.get(gate["param"])
                if not gp:
                    fail("%s: gate param %s not in chain" % (lid, gate["param"]))
                elif gp.get("type") == "enum":
                    n = len(gp.get("options", []))
                    want = gate.get("equals")
                    if want is not None and not (0 <= want < n):
                        fail("%s: gate value %r outside options" % (lid, want))

    if len(gates) > 4:
        fail("%d distinct visible_if params, limit 4: %s" % (len(gates), sorted(gates)))

    print("contract: %d bytes (%d chain + %d ui), %d levels, %d params"
          % (total, len(chain_text), len(ui_text), len(levels), len(cp)))
    print("gates: %s" % ", ".join(sorted(gates)))
    return report()


def report():
    if problems:
        print("FAIL (%d)" % len(problems))
        for p in problems:
            print(" -", p)
        return 1
    print("contract OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
