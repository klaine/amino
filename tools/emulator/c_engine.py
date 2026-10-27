"""ctypes wrapper around the compiled Amino engine (build/libamino_host).

The library is src/dsp/*.c plus tools/emulator/c_host.c, a minimal Schwung-like
host (tempo, transport, MIDI, 128-frame blocks).  scripts/build.sh builds it on
every run.  Renders through this wrapper are what the Move module plays.
"""
import ctypes
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
BLOCK = 128


def _lib_path():
    ext = {"darwin": "dylib", "win32": "dll"}.get(sys.platform, "so")
    return BUILD / f"libamino_host.{ext}"


def _stale_sources(lib):
    built = lib.stat().st_mtime
    srcs = list((ROOT / "src" / "dsp").glob("*.[ch]")) + [Path(__file__).with_name("c_host.c")]
    return [p.name for p in srcs if p.stat().st_mtime > built]


def load(check=True):
    """Load the engine library.  With check, raise RuntimeError when it is
    missing or older than its sources (callers decide how to report it)."""
    path = _lib_path()
    if check:
        if not path.exists():
            raise RuntimeError(f"{path} missing: run scripts/build.sh first")
        stale = _stale_sources(path)
        if stale:
            raise RuntimeError(f"{path.name} is older than {', '.join(stale[:3])}: "
                               "run scripts/build.sh")
    lib = ctypes.CDLL(str(path))
    lib.ar_sample_rate.restype = ctypes.c_int
    lib.ar_create.argtypes = [ctypes.c_float]
    lib.ar_create.restype = ctypes.c_void_p
    lib.ar_destroy.argtypes = [ctypes.c_void_p]
    lib.ar_transport.argtypes = [ctypes.c_int]
    lib.ar_set.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
    lib.ar_get.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    lib.ar_get.restype = ctypes.c_int
    lib.ar_midi.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.ar_render.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int16), ctypes.c_int]
    return lib


class Engine:
    """One Amino instance on the compiled engine.  Use as a context manager."""

    def __init__(self, lib, bpm=120.0):
        self.lib = lib
        self.sr = lib.ar_sample_rate()
        self.inst = lib.ar_create(float(bpm))
        if not self.inst:
            raise RuntimeError("no free Amino instance slot")
        self._buf = (ctypes.c_int16 * (BLOCK * 2))()
        self._pressure = -1

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        if self.inst:
            self.lib.ar_destroy(self.inst)
            self.inst = None

    def set(self, key, value):
        self.lib.ar_set(self.inst, key.encode(), str(value).encode())

    def get(self, key):
        buf = ctypes.create_string_buffer(65536)
        n = self.lib.ar_get(self.inst, key.encode(), buf, len(buf))
        return buf.value.decode() if n >= 0 else None

    def transport(self, running):
        self.lib.ar_transport(1 if running else 0)

    def midi(self, status, d1=0, d2=0):
        """Send one raw MIDI message (channel aftertouch uses only d1)."""
        self.lib.ar_midi(self.inst, int(status), int(d1), int(d2))

    def note_on(self, note, vel):
        v = max(1, min(127, round(vel * 127)))
        self.lib.ar_midi(self.inst, 0x90, int(note), v)

    def note_off(self, note):
        self.lib.ar_midi(self.inst, 0x80, int(note), 0)

    def pressure(self, value):
        """Channel aftertouch, sent only when the 7-bit value changes."""
        v = max(0, min(127, round(value * 127)))
        if v != self._pressure:
            self._pressure = v
            self.lib.ar_midi(self.inst, 0xD0, v, 0)

    def render(self, frames=BLOCK):
        """Render one block; returns interleaved stereo int16 bytes."""
        self.lib.ar_render(self.inst, self._buf, frames)
        return bytes(memoryview(self._buf).cast("B")[: frames * 4])
