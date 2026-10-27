import json
import re
import sys

src = open("src/dsp/contract_gen.c", encoding="utf-8").read()
parts = re.findall(r"(?:static )?const char (\w+)\[\] =\n(.*?);\n", src, re.S)
out = {}
for name, body in parts:
    raw = bytes(int(x, 16) for x in re.findall(r"\\x([0-9a-f]{2})", body))
    out[name] = raw.decode("utf-8")

cp = json.loads(out["amino_chain_params_json"])
ui = json.loads(out["amino_ui_hierarchy_json"])
levels = ui["levels"]
print("chain", len(cp), "levels", len(levels))
print("root:", json.dumps(levels["root"], indent=1))
print("osc1:", json.dumps(levels["osc1"]))
print("fx1:", json.dumps(levels["fx1"]))
print("split gate:", levels["split"]["visible_if"])
print("loadlayer:", levels["loadlayer"])
print("chain sample:", json.dumps(cp[0]), json.dumps(cp[3]))
for p in cp:
    if p["key"] in ("osc1_table", "cutoff", "m1_src"):
        print(p["key"], len(json.dumps(p)), json.dumps(p)[:220])
