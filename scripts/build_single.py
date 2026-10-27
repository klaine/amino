#!/usr/bin/env python3
"""Fold the WebAssembly site into ONE self-contained html file.

    python3 scripts/build_single.py [--out build/amino.html]

Run scripts/build_web.sh first (it makes build/amino.wasm and the packaged
module assets).  Everything the page needs -- the wasm engine, the AudioWorklet
source, the Remote UI scripts/styles/data, the host page logic -- ends up in the
one file, so it can be mailed, dropped in a chat or opened from disk.  Needs a
browser with WebAssembly, AudioWorklet and DecompressionStream (all current
Chrome, Edge, Firefox, Safari); WebMIDI is Chromium-only.
"""
import argparse
import base64
import gzip
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
MODULE = ROOT / "modules" / "sound_generators" / "amino"
ASSETS = MODULE / "assets"


def bundle_host():
    """app.js and its imports as one classic script (an IIFE, in dependency order)."""
    order = ["embed.js", "router.js", "engine.js", "bridge.js", "app.js"]
    parts = []
    for name in order:
        src = (WEB / name).read_text(encoding="utf-8")
        src = re.sub(r'^import\s[^;]*?from\s+"\./[\w.-]+";\s*\n', "", src, flags=re.M)
        assert not re.search(r"^import\b", src, flags=re.M), name + ": unhandled import"
        src = re.sub(r"^export\s+(?=(const|function|class|async)\b)", "", src, flags=re.M)
        assert not re.search(r"^export\b", src, flags=re.M), name
        parts.append("// ---- %s\n%s" % (name, src))
    return "(() => {\n\"use strict\";\n%s\n})();" % "\n".join(parts)


def ui_files():
    files = {}
    names = sorted(p.name for p in (ASSETS / "ui").glob("*.js"))
    for name in names:
        src = (ASSETS / "ui" / name).read_text(encoding="utf-8")
        # Relative module specifiers become bare names the import map resolves.
        src = re.sub(r'"\./([\w-]+\.js)"', r'"amino-ui/\1"', src)
        if name == "lazy.js":
            old = 'link.href = "assets/ui/lazy.css";\n  document.head.appendChild(link);'
            assert old in src, "lazy.js changed: update build_single.py"
            src = src.replace(old, "/* lazy.css is embedded in the page */")
        files["ui/" + name] = src.encode()
    for css in ("style.css", "lazy.css"):
        files["ui/" + css] = (ASSETS / "ui" / css).read_bytes()
    page = (MODULE / "web_ui.html").read_text(encoding="utf-8")
    page, n1 = re.subn(r'<link rel="stylesheet" href="assets/ui/style.css">', "{{STYLE}}", page)
    page, n2 = re.subn(r'<script src="/static/schwung-remote-api.js"></script>\s*<script type="module" src="assets/ui/app.js"></script>', "{{SCRIPTS}}", page)
    assert n1 == 1 and n2 == 1, "web_ui.html changed: update build_single.py"
    files["ui.html"] = page.encode()
    files["schwung-remote-api.js"] = (WEB / "schwung-remote-api.js").read_bytes()
    # Files the UI fetch()es, under the paths it asks for.
    for rel in ("assets/ui_meta.json", "assets/ui_presets.json"):
        files["vfs/" + rel] = (MODULE / rel).read_bytes()
    files["vfs/help.json"] = (MODULE / "help.json").read_bytes()
    for p in sorted((ASSETS / "wave-previews").glob("*.bin")):
        files["vfs/assets/wave-previews/" + p.name] = p.read_bytes()
    files["ui_pages.json"] = (MODULE / "ui_pages.json").read_bytes()
    return files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "build" / "amino.html"))
    args = ap.parse_args()

    wasm = ROOT / "build" / "amino.wasm"
    if not wasm.is_file():
        raise SystemExit("build/amino.wasm missing: run scripts/build_web.sh first")
    files = {"amino.wasm": wasm.read_bytes(), "worklet.js": (WEB / "worklet.js").read_bytes()}
    files.update(ui_files())

    index = json.dumps([[k, len(v)] for k, v in files.items()]).encode()
    blob = struct.pack("<I", len(index)) + index + b"".join(files.values())
    payload = base64.b64encode(gzip.compress(blob, 9, mtime=0)).decode()

    html = (WEB / "index.html").read_text(encoding="utf-8")
    html, n = re.subn(r'<script type="module" src="app.js"></script>', "@@HOST@@", html)
    assert n == 1, "index.html no longer loads app.js"
    host = bundle_host()
    assert "</script" not in host.lower().replace("<\\/script", "")
    html = html.replace(
        "@@HOST@@",
        '<script id="amino-payload" type="application/octet-stream">%s</script>\n<script>\n%s\n</script>'
        % (payload, host))
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(html, encoding="utf-8")
    print("wrote %s (%.2f MB, %d embedded files)" % (out, out.stat().st_size / 1e6, len(files)))


if __name__ == "__main__":
    main()
