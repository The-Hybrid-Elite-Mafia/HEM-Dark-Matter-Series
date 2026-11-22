# HEM Dark Matter Series

Hybrid Elite Mafia plug-in series, built in pure C++23 (no JUCE) for the Windows x64 / MSVC / AVX2 rig.

| # | Plug-in | Engine | Faceplate | VST3 |
|---|---------|--------|-----------|------|
| 1 | **HEM PHANTOM VOX** — 4-bay vocal strip | `phantom_vox/` | done | done |
| 2 | **HEM SYNDICATE RACK** — 5-column console / mix-bus strip (sidechain: EXT KEY) | `syndicate/` | done | done |
| 2a–f | **HEM SAT / COMP / TRANSIENT / TONE / MOD / MAX** — split modules (COMP: EXT KEY) | `syndicate/` (`SplitPlugin<>`) | done | done |
| 3 | **HEM APEX CORE** — 6-engine master finalizer | `apex/` | done | done |
| 4 | **HEM IRON LOCK** — pitch + formant (PSOLA), REALTIME / HQ latency reported | `ironlock/` | done | done |
| 5 | **HEM OUTLAW RIDER** — vocal gain rider | `rider/` | done | done |
| 6 | **HEM DARK MATTER 4D** — reverb + delay suite (ducking: EXT KEY), host tempo | `darkmatter/` | done | done |
| 7 | **HEM CHAINBREAKER 108** — 4-slot modular FX rack, host tempo | `chainbreaker/` | done | done |

Phase 1 (engines + tests), Phase 2 (faceplates) and Phase 3 (VST3 bundle, sidechain, presets on disk,
installer) are built. Download: the newest **Release** of this repository (`HEM-Dark-Matter-Setup-x.y.z.exe`).
Windows 64-bit VST3 only (no AU / AAX). The installer is not code-signed.

## Rules every engine follows (checked by `tests/`)
- No clipping: K-weighted auto-gain + look-ahead brickwall limiter at **-0.1 dBFS** on every output.
- Default load and every factory preset stay within **±1.0 dB** of the input level.
- All bays off = dry passes **bit-exact** (latency aligned). Bypass is bit-exact too.
- Every control is proven to change the audio; every mode is proven to sound different.
- Zero heap allocations on the audio thread. Disabled bays skip their DSP.
- All working guesses live in one file: `include/dm/Assumed.hpp`, each marked `// ASSUMED`.

## Layout
```
include/dm/Assumed.hpp      every tuning guess, in one place
include/dm/core/            shared DSP: filters, dynamics, reverb, delay, safety, analyzer, params
include/dm/core/EngineShell.hpp  shared plug-in shell (bypass, trims, auto-gain, limiter, meters, sidechain key)
include/dm/<plugin>/        each engine's interface + parameter table
src/<plugin>/               each engine's DSP + factory presets
include/dm/ui/, src/ui/     faceplates: model, interaction, drawing, one layout file per plug-in
include/dm/plugin/, src/plugin/  plug-in catalogue (13 plug-ins, permanent ids), host binding, in-host window
plugin/                     VST3 wrapper (one bundle: "HEM Dark Matter.vst3"); Steinberg SDK fetched at configure
installer/                  Inno Setup installer, how-to-test guide, third-party notices
tools/faceplate_snapshot/   renders every faceplate to PNG (CI pictures)
tools/vst3_hosttest/        loads the built / installed bundle like a music program and checks it
tests/                      quality-gate tests (real audio, measured numbers); Suite.hpp = shared gates
docs/                       per-plug-in notes
```

## What the tests prove
- Engines: every rule above, every control, every mode, every preset, 44.1 / 48 / 96 kHz.
- Faceplates: every control bound, on the plate, not overlapping; every control clicked / dragged moves its
  own parameter; presets, undo, curve handles, XY pad, quick-load chains — directly and through a host.
- VST3: Steinberg's validator on the bundle; the test host proves plug-in output = engine output bit for
  bit, host automation, saved state, sidechain, latency; on Windows it also opens every faceplate inside a
  host window. The installer is installed, tested, upgraded and uninstalled on a clean Windows machine.

## Build + test
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DDM_BUILD_VST3=ON
cmake --build build --config Release
ctest --test-dir build -C Release -V
```
Run one group: `dm_tests pv | sr | ax | il | rd | cb | ui | plugin`.
GitHub Actions runs the same on `windows-2022` (`.github/workflows/windows-ci.yml`).

License: same as The Hybrid Core (GPLv3).
