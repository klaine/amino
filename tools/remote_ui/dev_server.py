#!/usr/bin/env python3
"""Remote UI development harness: the Amino web UI on the compiled engine.

Serves the module's web_ui.html in a sandboxed iframe, as schwung-manager does,
with a stand-in /static/schwung-remote-api.js that talks to this server instead
of the device.  Parameters live in one Amino instance on build/libamino_host
(scripts/build.sh builds it), so ranges, presets and state behave exactly as on
Move.  The harness page adds an on-screen keyboard and streams the instance's
audio to the browser.

    python3 tools/remote_ui/dev_server.py [--port 7700] [--no-audio]

Then open http://localhost:7700/.  Edits to src/web_ui.html and src/web/*
are served directly; ui_meta.json and the wave
previews come from the packaged module, so run tools/gen/gen.py after
changing tools/gen/params.py.
"""
import argparse
import json
import mimetypes
import queue
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / "tools" / "emulator"))
import c_engine  # noqa: E402

MODULE = ROOT / "modules" / "sound_generators" / "amino"
SRC = ROOT / "src"
# Published path -> source, checked before the packaged module folder so UI
# edits show up on reload without running the generator.
OVERLAY = {"web_ui.html": SRC / "web_ui.html"}
OVERLAY_DIRS = {"assets/ui/": SRC / "web"}
# Values the state blob does not carry but the manager's burst does.
EXTRA_KEYS = ("preset", "preset_name", "preset_count", "edit_layer")
# schwung-manager's viz extra-key pump (Schwung docs/MODULES.md): refreshed
# 150 ms after a parameter write, and on a 500 ms heartbeat otherwise.
EXTRA_THROTTLE_S = 0.15
EXTRA_HEARTBEAT_S = 0.5


class Synth:
    """One engine instance shared by every request, behind one lock."""

    def __init__(self):
        self.lock = threading.Lock()
        self.engine = c_engine.Engine(c_engine.load())
        self.values = {}
        self.listeners = set()
        self.refresh()
        params = json.loads(self.engine.get("chain_params") or "[]")
        self.extra_keys = []
        for p in params:
            for k in (p.get("viz") or {}).get("extra_keys", []):
                if k not in self.extra_keys:
                    self.extra_keys.append(k)
        self.extras = {}
        self.touched = threading.Event()
        threading.Thread(target=self._pump_extras, daemon=True).start()

    def _pump_extras(self):
        """Push viz extra keys that moved, as the manager does."""
        while True:
            if self.touched.wait(EXTRA_HEARTBEAT_S):
                self.touched.clear()
                time.sleep(EXTRA_THROTTLE_S)
            with self.lock:
                now = {k: self.engine.get(k) for k in self.extra_keys}
            changed = {k: v for k, v in now.items() if v is not None and self.extras.get(k) != v}
            self.extras.update(changed)
            if changed:
                for q in list(self.listeners):
                    q.put(changed)

    def _snapshot(self):
        values = json.loads(self.engine.get("state"))
        for k in ("fmt", "v", "rev", "name"):
            values.pop(k, None)
        for k in EXTRA_KEYS:
            v = self.engine.get(k)
            if v is not None:
                values[k] = v
        # The device also reports the focused layer's bare aliases.
        layer = values.get("edit_layer", "1")
        prefix = "l%s_" % layer
        for k, v in list(values.items()):
            if k.startswith(prefix):
                values[k[len(prefix):]] = v
        return values

    def refresh(self):
        """Re-read the instance and push every value that moved."""
        with self.lock:
            now = self._snapshot()
        changed = {k: v for k, v in now.items() if self.values.get(k) != v}
        self.values = now
        if changed:
            for q in list(self.listeners):
                q.put(changed)

    def set(self, key, value):
        with self.lock:
            self.engine.set(key, value)
        self.refresh()
        self.touched.set()

    def get(self, key):
        with self.lock:
            return self.engine.get(key)

    def midi(self, msg):
        with self.lock:
            self.engine.midi(*msg)

    def render(self, frames):
        with self.lock:
            return self.engine.render(frames)


class Audio:
    """Renders in real time, like the device, so notes, envelopes and the
    telemetry move whether or not a browser is listening; listening browsers
    get the audio."""

    AHEAD = 0.12  # seconds rendered ahead of the wall clock

    def __init__(self, synth):
        self.synth = synth
        self.clients = set()
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        sr = self.synth.engine.sr
        start = time.monotonic()
        frames = 0
        while True:
            if frames / sr >= time.monotonic() - start + self.AHEAD:
                time.sleep(0.004)
                continue
            chunk = b"".join(self.synth.render(c_engine.BLOCK) for _ in range(8))
            frames += 8 * c_engine.BLOCK
            for q in list(self.clients):
                try:
                    q.put_nowait(chunk)
                except queue.Full:
                    pass  # a stalled browser drops audio, not the engine

    def add(self, q):
        self.clients.add(q)

    def remove(self, q):
        self.clients.discard(q)


def module_file(rel):
    if rel in OVERLAY:
        return OVERLAY[rel]
    for prefix, src in OVERLAY_DIRS.items():
        if rel.startswith(prefix):
            return src / rel[len(prefix):]
    return MODULE / rel


class Handler(BaseHTTPRequestHandler):
    synth = None
    audio = None
    stream = True
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        if "/api/" in self.path and "/api/events" not in self.path:
            return
        sys.stderr.write("%s\n" % (fmt % args))

    def _send(self, body, ctype, status=200):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _json(self, obj):
        self._send(json.dumps(obj), "application/json")

    def _file(self, path):
        path = Path(path)
        if not path.is_file():
            self._send("not found", "text/plain", 404)
            return
        ctype = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
        if path.suffix in (".js", ".mjs"):
            ctype = "text/javascript"
        self._send(path.read_bytes(), ctype)

    def _body(self):
        n = int(self.headers.get("Content-Length") or 0)
        return json.loads(self.rfile.read(n) or b"{}")

    def _stream_headers(self, ctype):
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True

    def do_GET(self):
        url = urlparse(self.path)
        path = url.path
        if path == "/":
            return self._file(HERE / "harness.html")
        if path == "/static/schwung-remote-api.js":
            return self._file(HERE / "schwung-remote-api.js")
        if path.startswith("/remote-ui/amino/"):
            rel = path[len("/remote-ui/amino/"):]
            if ".." in rel or rel.endswith("module.json"):
                return self._send("forbidden", "text/plain", 403)
            return self._file(module_file(rel))
        if path == "/api/params":
            return self._json(self.synth.values)
        if path == "/api/chain_params":
            return self._send(self.synth.get("chain_params") or "null", "application/json")
        if path == "/api/hierarchy":
            return self._file(MODULE / "ui_pages.json")
        if path == "/api/get":
            key = parse_qs(url.query).get("key", [""])[0]
            return self._json({"value": self.synth.get(key)})
        if path == "/api/events":
            return self._events()
        if path == "/api/audio":
            return self._audio()
        self._send("not found", "text/plain", 404)

    def do_POST(self):
        path = urlparse(self.path).path
        body = self._body()
        if path == "/api/set":
            self.synth.set(str(body["key"]), str(body["value"]))
            return self._json({"ok": True})
        if path == "/api/midi":
            self.synth.midi([int(b) for b in body["msg"]][:3] + [0] * (3 - len(body["msg"])))
            return self._json({"ok": True})
        self._send("not found", "text/plain", 404)

    def _events(self):
        q = queue.Queue()
        self.synth.listeners.add(q)
        self._stream_headers("text/event-stream")
        try:
            self.wfile.write(b": connected\n\n")
            self.wfile.flush()
            while True:
                try:
                    changed = q.get(timeout=15)
                    self.wfile.write(b"data: " + json.dumps(changed).encode() + b"\n\n")
                except queue.Empty:
                    self.wfile.write(b": ping\n\n")
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            self.synth.listeners.discard(q)

    def _audio(self):
        if not self.stream:
            return self._send("audio disabled", "text/plain", 404)
        q = queue.Queue(maxsize=64)
        self.audio.add(q)
        self._stream_headers("application/octet-stream")
        try:
            while True:
                self.wfile.write(q.get())
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            self.audio.remove(q)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--port", type=int, default=7700)
    ap.add_argument("--no-audio", action="store_true")
    args = ap.parse_args()
    try:
        synth = Synth()
    except RuntimeError as e:
        sys.exit(str(e))
    Handler.synth = synth
    Handler.audio = Audio(synth)   # always renders; --no-audio only stops streaming
    Handler.stream = not args.no_audio
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    server.daemon_threads = True
    print("Amino Remote UI harness: http://localhost:%d/" % args.port)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
