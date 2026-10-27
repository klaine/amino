# Amino: agent notes

8-bit wavetable `sound_generator` (Schwung API v2), module id `amino`. Read [README.md](README.md) for
the product and the document map. This file covers working on the code.

@AGENTS.md

## Commands

```sh
scripts/build.sh              # contract, native, UI and golden tests, then dsp.so + amino-module.tar.gz
scripts/build.sh --gen        # run tools/gen/gen.py first (after changing params, presets or src/ UI files)
scripts/build_web.sh          # WebAssembly site in build/site (needs a --gen build first)
scripts/deploy.sh [host]      # staged install on a Move; scripts/logs.sh follows the debug log
python3 tools/remote_ui/dev_server.py   # Remote UI harness on http://localhost:7700, no Move needed
```

## Where things come from

- **Parameters:** `tools/gen/params.py` is the canonical list. `gen.py` turns it, the
  `presets/factory/*.json` bank and `tools/wavegen/out/amino_wavetables.bin` into
  `src/dsp/params_gen.c`, `contract_gen.c`, `factory_presets.c` and `wavetables.c` ("do not edit").
- **Module folder:** `modules/sound_generators/amino/` is mostly generated. `gen.py` copies
  `module.json`, `ui_chain.js`, `mod_matrix_ui.mjs`, `cards.js`, `help.json` and `web_ui.html` from
  `src/` into it, writes `ui_pages.json` and `assets/`, and copies `src/web/` to `assets/ui/`. **Edit
  the `src/` copy**, then run `--gen`.
- **Wavetables:** `tools/wavegen/` (procedural recipes plus AKWF-FREE, which `fetch_akwf.sh` caches).
  Licensing rules are in [docs/07](docs/07-wavetables-and-ip.md).

## Tests

- The C engine in `src/dsp/` is the reference. The Python emulator in `tools/emulator/engine.py` is
  frozen history ([docs/12](docs/12-emulator-guide-for-agents.md)); don't extend it.
- Golden audio: `tests/golden/golden.json` covers all 324 factory presets. A sound change that is
  intended needs `tests/golden/check_golden.py build/amino_golden --write`, and the reason goes in
  the change description. Never rewrite it just to make a failure go away.
- `tests/contract/validate_contract.py` checks the generated contract against Schwung's limits.

## Notes

- `docs/_*` are local copies of Schwung's docs and sources (git-ignored): the ABI reference for all
  modules in this workspace.
- GitHub Pages serves the `pages` branch (single-file `index.html` from `build/amino.html`, plus `gallery/` from
  `tools/emulator/gallery.py --site`). It is published by hand; there is no workflow.
- `../Original reference/` (outside this repo) is the original spec and preset set Amino was built
  from.
- Crest's effects were adapted from Amino's `src/dsp/fx.c`: a fix here may apply there too.
