# HEM Dark Matter Series

Hybrid Elite Mafia plug-in series, built in pure C++23 (no JUCE) for the Windows x64 / MSVC / AVX2 rig.

| # | Plug-in | Engine file | Phase 1 (DSP + tests) |
|---|---------|-------------|------------------------|
| 1 | **HEM PHANTOM VOX** — 4-bay vocal strip | `phantom_vox/` | done |
| 2 | **HEM SYNDICATE RACK** — 5-column console / mix-bus strip | `syndicate/` | done |
| 2a–f | **HEM SAT / COMP / TRANSIENT / TONE / MOD / MAX** — split modules | `syndicate/` (`SplitPlugin<>`) | done |
| 3 | **HEM APEX CORE** — 6-engine master finalizer | `apex/` | done |
| 4 | **HEM IRON LOCK** — pitch + formant (PSOLA) | `ironlock/` | done |
| 5 | **HEM OUTLAW RIDER** — vocal gain rider | `rider/` | done |
| 6 | **HEM DARK MATTER 4D** — reverb + delay suite | `darkmatter/` | done |
| 7 | **HEM CHAINBREAKER 108** — 4-slot modular FX rack | `chainbreaker/` | done |

Phase 2 (GPU faceplates) and Phase 3 (plug-in wrapper / installer) are not started.

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
include/dm/core/EngineShell.hpp  shared plug-in shell (bypass, trims, auto-gain, limiter, meters)
include/dm/<plugin>/        each engine's interface + parameter table
src/<plugin>/               each engine's DSP + factory presets
tests/                      quality-gate tests (real audio, measured numbers); Suite.hpp = shared gates
docs/                       per-plug-in notes
```

## Build + test
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release -V
```
Run one plug-in group: `dm_tests pv | sr | ax | il | rd | cb`.
GitHub Actions runs the same on `windows-2022` (`.github/workflows/windows-ci.yml`).

License: same as The Hybrid Core (GPLv3).
