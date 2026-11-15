# HEM Dark Matter Series

Hybrid Elite Mafia plug-in series, built in pure C++23 (no JUCE) for the Windows x64 / MSVC / AVX2 rig.

| # | Plug-in | Status |
|---|---------|--------|
| 1 | **HEM PHANTOM VOX** — 4-bay vocal channel strip | Phase 1 DSP engine + tests |
| 2 | HEM SYNDICATE RACK (+ 6 split modules) | not started |
| 3 | HEM APEX CORE | not started |
| 4 | HEM IRON LOCK | not started |
| 5 | HEM OUTLAW RIDER | not started |
| 6 | HEM DARK MATTER 4D | not started |
| 7 | HEM CHAINBREAKER 108 | not started |

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
include/dm/phantom_vox/     PHANTOM VOX engine interface
src/phantom_vox/            PHANTOM VOX engine, SFX bay, parameter table + factory presets
tests/                      quality-gate tests (real audio, measured numbers)
docs/                       per-plug-in notes
```

## Build + test
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release -V
```
GitHub Actions runs the same on `windows-2022` (`.github/workflows/windows-ci.yml`).

License: same as The Hybrid Core (GPLv3).
