#pragma once
// HEM Dark Matter Series - Linkwitz-Riley 4th-order crossovers. Bands sum back to an all-pass response,
// so a 3-band split with every band at unity reconstructs the input magnitude exactly.
#include "dm/core/Filters.hpp"

namespace dm {

// One LR4 split point (stereo): two cascaded Butterworth 2nd-order sections per side.
struct Lr4Split {
    StereoBiquad lp1, lp2, hp1, hp2;
    void set(float hz, float sampleRate) noexcept {
        const auto lp = designBiquad(BiquadType::LowPass, hz, 0.70710678f, 0, sampleRate);
        const auto hp = designBiquad(BiquadType::HighPass, hz, 0.70710678f, 0, sampleRate);
        lp1.set(lp); lp2.set(lp); hp1.set(hp); hp2.set(hp);
    }
    void reset() noexcept { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
    void split(int ch, float x, float& lo, float& hi) noexcept {
        lo = lp2.process(ch, lp1.process(ch, x));
        hi = hp2.process(ch, hp1.process(ch, x));
    }
};

// LR4 all-pass at a crossover frequency (LP + HP of the same LR4 pair), for phase-aligning the low band.
struct Lr4AllPass {
    Lr4Split s;
    void set(float hz, float sampleRate) noexcept { s.set(hz, sampleRate); }
    void reset() noexcept { s.reset(); }
    [[nodiscard]] float process(int ch, float x) noexcept { float lo, hi; s.split(ch, x, lo, hi); return lo + hi; }
};

// Three bands: low | mid | high, phase-coherent.
struct ThreeBandSplit {
    Lr4Split a, b;
    Lr4AllPass lowComp;
    void set(float lowHz, float highHz, float sampleRate) noexcept {
        highHz = std::max(highHz, lowHz * 1.5f);
        a.set(lowHz, sampleRate);
        b.set(highHz, sampleRate);
        lowComp.set(highHz, sampleRate);
    }
    void reset() noexcept { a.reset(); b.reset(); lowComp.reset(); }
    void split(int ch, float x, float& lo, float& mid, float& hi) noexcept {
        float rest;
        a.split(ch, x, lo, rest);
        b.split(ch, rest, mid, hi);
        lo = lowComp.process(ch, lo);
    }
};

} // namespace dm
