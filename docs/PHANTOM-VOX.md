# HEM PHANTOM VOX — Phase 1 engine notes

Vocal channel strip: four bays (DYNAMICS, TONE, SPACE, SFX) plus the ENHANCER macro.
This file says what every faceplate control does in the DSP, so the Phase 2 faceplate can be bound 1:1.

## Signal flow
```
INPUT trim
 -> HPF/LPF (here when FLT POST is off)
 -> DYNAMICS: GATE -> COMP (mode) -> DE-ESS -> COLOR saturation -> bay trim
 -> TONE: LOW / MID / HIGH / AIR+ (mode voicing) -> HPF/LPF (here when FLT POST is on) -> bay trim
 -> [ANALYZER tap]
 -> SFX (mode, SFX macro) -> ENHANCER -> bay trim
 -> SPACE: send from before SFX (SPC POST off) or after SFX (SPC POST on)
          -> 120 Hz send high-pass -> REVERB + DELAY (parallel)
          -> each return ducked by the dry vocal (VRB DUCK / DLY DUCK)
          -> SPACE TONE filter -> added to the dry path -> bay trim
 -> AUTO-GAIN (K-weighted RMS, matches output loudness to input)
 -> OUTPUT trim -> brickwall limiter, ceiling -0.1 dBFS (1.5 ms look-ahead = reported latency)
 -> BYPASS crossfade (10 ms) against latency-aligned dry
```

## Controls (parameter id → what it does)
| Faceplate | id | Range / default | DSP |
|---|---|---|---|
| Top bar BYPASS | `bypass` | off | click-free crossfade to latency-aligned dry, bit-exact when settled |
| Bottom INPUT / OUTPUT | `input` / `output` | ±24 dB, 0 | input before everything; output after auto-gain, before limiter |
| (internal) AUTO GAIN | `autogain` | on | brief's mandatory auto-gain stage |
| Bay power | `dyn_on` `tone_on` `space_on` `sfx_on` | on | bay skipped entirely when off (no CPU) |
| Bay S (solo) | `*_solo` | off | only soloed bays run; SPACE solo alone = wet returns only |
| Bay `<` `>` | `dyn_mode` `tone_mode` `space_mode` `sfx_mode` | 7 / 7 / 14 / 11 modes | see `Assumed.hpp` tables |
| Bay trim | `*_trim` | ±12 dB, 0 | gain at the bay output |
| GATE fader | `gate` | -96..0 dB, -96 | stereo-linked gate, 6 dB hysteresis, 40 ms hold, -80 dB floor |
| COMP knob | `comp` | 0..100 %, 35 | threshold 0 → -36 dBFS, ratio scales from 1:1 to the mode ratio |
| COLOR readout | `color` | 0..200 %, 127 | mode saturation blend/drive (0 % = clean) |
| DE-ESS knob | `deess` | 0..100 %, 30 | split-band de-esser, up to 14 dB on the band above FOCUS × 0.75 |
| FOCUS readout | `focus` | 2–12 kHz, 5.2 kHz | de-esser detection centre |
| GR / DS meters | meters `gr`, `ds` | dB | live compressor / de-esser reduction |
| LOW / MID / HIGH / AIR+ faders | `low` `mid` `high` `air` | ±12 dB | mode-voiced bands (shelf/peak, proportional Q in some modes) |
| M / V switch | `voice` | M | moves LOW/MID centres down (M ×0.8) or up (V ×1.3) |
| HPF / LPF bar | `hpf` / `lpf` | 20–1000 Hz, 70 / 2–20 kHz, 20 kHz | 12 dB/oct |
| ANALYZER | `analyzer().compute()` | — | real 4096-point FFT of the post-TONE signal + `toneCurveDb()` overlay |
| REVERB knob / TIME | `reverb` / `reverb_time` | 0..100 %, 25 / 0.2–20 s, 2.3 s | return level / RT60 (× mode factor) |
| DELAY knob / RATE | `delay` / `delay_rate` | 0..100 %, 20 / 1/1…1/32, 1/8d | return level / tempo-synced time |
| VRB DUCK / DLY DUCK | `vrb_duck` / `dly_duck` | 0..100 %, 32 / 17 | up to 24 dB return ducking under the dry vocal |
| IN / DUCK / OUT LEDs | meters `revIn/revDuck/revOut`, `dlyIn/dlyDuck/dlyOut` | — | live send, duck depth, return level |
| SPACE TONE slider | `space_tone` | -100..+100 % | left = darker returns (LPF to 1.5 kHz), right = thinner (HPF to 1.5 kHz) |
| (host) tempo | `tempo` | 120 BPM | delay sync until host transport feeds BPM |
| SFX knob | `sfx` | 0..100 %, 25 | mode amount (drive / depth / mix) |
| ENHANCER knob | `enhancer` | 0..100 %, 15 | 3 kHz+ exciter + low-mid body shelf |
| SPC POST / FLT POST | `spc_post` / `flt_post` | off | routing as in the signal flow above |

## Not in Phase 1
- The faceplate itself (Phase 2): 3D knobs, LEDs, preset dropdown, gold crown logo.
- Host tempo: `tempo` is a parameter until the plug-in wrapper feeds the host BPM.
- Plug-in wrapper (VST3): the engine is host-agnostic; the wrapper comes with the faceplate.
