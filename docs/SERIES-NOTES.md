# HEM Dark Matter Series — engine notes (Phase 1)

Every engine below sits on `EngineShell`, so all of them share: BYPASS (10 ms crossfade, bit-exact),
INPUT / OUTPUT trims and LED meters, K-weighted AUTO-GAIN, and the -0.1 dBFS look-ahead limiter
(72 samples = 1.5 ms at 48 kHz). All guesses live in `include/dm/Assumed.hpp`.

## HEM SYNDICATE RACK (+ 6 split plug-ins)
SATURATE (8 modes, SAT fader, CRISP/WARM tilt) → DYNAMICS (COMP 8 modes + COLOR + SIDECHAIN high-pass,
TRANSIENT + SHAPE) → TONE (8 voicings, SUB/LOW/MID/HI MID/HIGH/AIR+, HPF/LPF x2 = 12 dB/oct, x4 = 24 dB/oct)
→ MODULATE (6 modes, MOD, DEPTH, DRIFT, ANALOG wow/flutter) → MAXIMIZE (7 modes, MULTI 3-band up/down,
MAX push, adjustable 250 Hz / 5.2 kHz LR4 crossovers, CLIP OFF/SOFT/HARD/MST).
TONE ROUTING moves TONE to PRE (first), POST DYN (default) or POST (after MODULATE).
The brief's MAXIMIZE mode that carried another company's product name is called **TRIPLE CRUSH**.
Presets follow the brief's categories 01 Mixbus … 08 Acoustic Guitar (Acoustic Electric, Airy Crush,
Clean Lift, Dark, Open). Split plug-ins: same module code, own POWER switch.
SIDECHAIN "INPUT / GR" in the brief is a meter-view switch → faceplate only (no audio effect).

## HEM APEX CORE
ENGINE (PRIMA, AIR-1B, MULTIMAX, HELIX = mid/side, TITAN, VELVET), AMOUNT 0–200 % (127 % default),
CONTAIN (pulls a band down when it pokes above the band average), BOOST (upward detail), PARALLEL,
MONO-SAFE (< 120 Hz to mono), WIDTH, PEAKS / CURVE transfer stage, ATTACK / RELEASE multipliers,
COLOR PROFILE LOW / HIGH harmonics. Display helpers: `transferDb()`, `attackMs()`, `releaseMs()`.

## HEM IRON LOCK
YIN detection + PSOLA resynthesis (formants preserved; FORMANT re-samples grains, LINK ties formant to
PITCH SHIFT). STRENGTH, TRANSITION (glide ms), SUSTAIN (keeps vibrato), RANGE (cents), 12 note locks,
QUANTUM KEY (key finding from a decaying pitch-class histogram), GROUPS (shared scale across instances),
REALTIME / HQ. **Latency: REALTIME 2048 samples (42.7 ms @ 48 kHz), HQ 3328 (69.3 ms) + 1.5 ms limiter.**
The brief says "zero-latency"; real pitch correction needs this look-ahead, so it is not zero.

## HEM OUTLAW RIDER
TARGET, MAX boost / MIN cut handles, RANGE, SPEED (1.5 s … 20 ms), SENSITIVITY (holds below it),
OUTPUT. TRUEGAIN viewport fed by `readFrames()` (in / out / gain per 256 samples).
Auto-gain uses a 1.5 s window here so phrase-to-phrase riding survives.

## HEM DARK MATTER 4D
REVERB (FDN, DECAY 0.2–75 s, DEPTH, PRE-DELAY, LOW/HIGH COLOR), DELAY (TIME 1/8d default, FEEDBACK,
400 % MULTIPLIER = 4-grain cloud on the repeats), ROUTING (REV > DLY, PARALLEL, DLY > REV),
DUCKING + SHAPE (L/R meters, `shapeCurve()` for the display).

## HEM CHAINBREAKER 108
4 slots, each switchable to STUTTER (XY = rate / gate, FLUX = trigger + rate scatter), MULTI-CLIP
(3 bands, crossovers, DRIVE, SHAPE), GRAIN ENGINE (PITCH, REVERSE, WIDE, MIX), SPECTRO EQ (4 bands).
Re-ordering = swapping slot types. Quick-Load Chain Slots 1–8 via `loadChain(i)`.
Master: FLUX / DRIVE / TONE / MIX macros, BROADCAST EQ, 5 graphic trims, CROSSFEED, WIDTH, OUT LEVEL,
UTILITY ROUTING (PRE / SOFT / HARD / MST).
