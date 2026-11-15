#pragma once
// HEM Dark Matter Series - waveshapers. All are unity-slope near zero so low drive stays clean.
#include "dm/core/Math.hpp"

namespace dm {

enum class SatType { Soft, Tube, Tape, Odd, Hard };

// drive: linear pre-gain (>= 1 adds harmonics). Output is level-compensated by 1/drive' so the
// small-signal gain stays ~1 regardless of drive.
[[nodiscard]] inline float saturate(SatType t, float x, float drive) noexcept {
    drive = std::max(drive, 1.0e-3f);
    const float d = x * drive;
    float y = 0;
    switch (t) {
    case SatType::Soft: y = std::tanh(d); break;
    case SatType::Tube: {
        // Asymmetric: positive half compresses harder -> even harmonics. Bias removed so 0 -> 0.
        constexpr float bias = 0.25f;
        y = std::tanh(d + bias) - std::tanh(bias);
        y /= (1.0f - std::tanh(bias) * std::tanh(bias)); // unit slope at 0
        break;
    }
    case SatType::Tape: {
        // Gentle cubic-into-tanh curve: soft onset, rounded top.
        const float c = d - 0.12f * d * d * d / (1.0f + std::fabs(d));
        y = std::tanh(c);
        break;
    }
    case SatType::Odd: {
        // Firmer knee (transistor/FET style) - more odd harmonics.
        const float a = std::fabs(d);
        y = d / std::pow(1.0f + a * a * a, 1.0f / 3.0f);
        break;
    }
    case SatType::Hard: y = clampf(d, -1.0f, 1.0f); break;
    }
    return y / drive;
}

} // namespace dm
