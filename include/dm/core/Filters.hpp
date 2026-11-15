#pragma once
// HEM Dark Matter Series - filters: RBJ biquads, one-pole, DC blocker, allpass.
#include "dm/core/Math.hpp"

#include <complex>

namespace dm {

struct BiquadCoefs {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    // Magnitude response in dB at frequency f (for UI curves; not called on the audio thread).
    [[nodiscard]] float magnitudeDb(float f, float sampleRate) const noexcept {
        const float w = kTwoPi * f / sampleRate;
        const std::complex<float> z1 = std::polar(1.0f, -w);
        const std::complex<float> z2 = z1 * z1;
        const std::complex<float> num = b0 + b1 * z1 + b2 * z2;
        const std::complex<float> den = 1.0f + a1 * z1 + a2 * z2;
        return gainToDb(std::abs(num) / std::max(std::abs(den), 1.0e-12f));
    }
};

enum class BiquadType { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf, AllPass, Notch };

// Robert Bristow-Johnson cookbook designs.
[[nodiscard]] inline BiquadCoefs designBiquad(BiquadType type, float freq, float q, float gainDb,
                                             float sampleRate) noexcept {
    freq = clampf(freq, 5.0f, sampleRate * 0.49f);
    q    = std::max(q, 0.025f);
    const float A     = std::pow(10.0f, gainDb / 40.0f);
    const float w0    = kTwoPi * freq / sampleRate;
    const float cw    = std::cos(w0);
    const float sw    = std::sin(w0);
    const float alpha = sw / (2.0f * q);
    float b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    switch (type) {
    case BiquadType::LowPass:
        b0 = (1 - cw) * 0.5f; b1 = 1 - cw; b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case BiquadType::HighPass:
        b0 = (1 + cw) * 0.5f; b1 = -(1 + cw); b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case BiquadType::BandPass:
        b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case BiquadType::Notch:
        b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case BiquadType::AllPass:
        b0 = 1 - alpha; b1 = -2 * cw; b2 = 1 + alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case BiquadType::Peak:
        b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
        a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
    case BiquadType::LowShelf: {
        const float sq = 2 * std::sqrt(A) * alpha;
        b0 = A * ((A + 1) - (A - 1) * cw + sq);
        b1 = 2 * A * ((A - 1) - (A + 1) * cw);
        b2 = A * ((A + 1) - (A - 1) * cw - sq);
        a0 = (A + 1) + (A - 1) * cw + sq;
        a1 = -2 * ((A - 1) + (A + 1) * cw);
        a2 = (A + 1) + (A - 1) * cw - sq;
        break;
    }
    case BiquadType::HighShelf: {
        const float sq = 2 * std::sqrt(A) * alpha;
        b0 = A * ((A + 1) + (A - 1) * cw + sq);
        b1 = -2 * A * ((A - 1) + (A + 1) * cw);
        b2 = A * ((A + 1) + (A - 1) * cw - sq);
        a0 = (A + 1) - (A - 1) * cw + sq;
        a1 = 2 * ((A - 1) - (A + 1) * cw);
        a2 = (A + 1) - (A - 1) * cw - sq;
        break;
    }
    }
    const float inv = 1.0f / a0;
    return {b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv};
}

// Transposed direct form II, one channel.
struct Biquad {
    BiquadCoefs c;
    float z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }
    [[nodiscard]] float process(float x) noexcept {
        const float y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

// Stereo biquad sharing coefficients.
struct StereoBiquad {
    BiquadCoefs c;
    float z[2][2] = {{0, 0}, {0, 0}};

    void set(const BiquadCoefs& coefs) noexcept { c = coefs; }
    void reset() noexcept { z[0][0] = z[0][1] = z[1][0] = z[1][1] = 0; }
    [[nodiscard]] float process(int ch, float x) noexcept {
        float* s = z[ch];
        const float y = c.b0 * x + s[0];
        s[0] = c.b1 * x - c.a1 * y + s[1];
        s[1] = c.b2 * x - c.a2 * y;
        return y;
    }
};

// One-pole low-pass (and derived high-pass), cutoff in Hz.
struct OnePole {
    float a = 0, z = 0;
    void setCutoff(float hz, float sampleRate) noexcept {
        a = std::exp(-kTwoPi * clampf(hz, 1.0f, sampleRate * 0.49f) / sampleRate);
    }
    void reset() noexcept { z = 0; }
    [[nodiscard]] float lp(float x) noexcept { z = x + a * (z - x); return z; }
    [[nodiscard]] float hp(float x) noexcept { return x - lp(x); }
};

// DC blocker (~10 Hz).
struct DcBlocker {
    float x1 = 0, y1 = 0, r = 0.9995f;
    void prepare(float sampleRate) noexcept { r = 1.0f - kTwoPi * 10.0f / sampleRate; }
    void reset() noexcept { x1 = y1 = 0; }
    [[nodiscard]] float process(float x) noexcept {
        const float y = x - x1 + r * y1;
        x1 = x; y1 = flushDenormal(y);
        return y;
    }
};

// First-order allpass (phaser stage).
struct AllPass1 {
    float a = 0, z = 0;
    void reset() noexcept { z = 0; }
    void setFreq(float hz, float sampleRate) noexcept {
        const float t = std::tan(kPi * clampf(hz, 10.0f, sampleRate * 0.45f) / sampleRate);
        a = (t - 1.0f) / (t + 1.0f);
    }
    [[nodiscard]] float process(float x) noexcept {
        const float y = a * x + z;
        z = x - a * y;
        return y;
    }
};

} // namespace dm
