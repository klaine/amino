# Repository instructions

## Zig on Windows

Zig is installed in the current user's Python 3.13 site-packages, not necessarily
on `PATH`. In PowerShell, resolve it through `%APPDATA%` rather than hard-coding
the Windows username:

```powershell
$zig = Join-Path $env:APPDATA 'Python\Python313\site-packages\ziglang\zig.exe'
if (-not (Test-Path $zig)) { throw "zig not found: $zig" }
& $zig version
```

Use `& $zig cc ...` for native tests and the aarch64 device build. Check this
path before concluding Zig is unavailable or using a bare `zig` command.

## macOS

Install Zig with Homebrew (`brew install zig`); native tests use Apple clang
(`cc`). `scripts/build.sh` runs contract/native/golden tests when their sources
are present, cross-compiles `modules/sound_generators/amino/dsp.so` for
`aarch64-linux-gnu` and repackages `amino-module.tar.gz`:

```sh
scripts/build.sh            # tests + device build + package
scripts/build.sh --gen      # run tools/gen/gen.py first
scripts/build.sh --no-test  # device build + package only
scripts/deploy.sh [host]    # install the package on a Move over SSH
```

Host test binaries go to `build/` (ignored). Use `python3` where docs say `py -3`.
