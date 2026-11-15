#pragma once
// HEM Dark Matter Series - small math helpers shared by every engine.
#include <algorithm>
#include <cmath>
#include <cstdint>

#if defined(_M_X64) || defined(__x86_64__) || defined(__SSE__)
#include <immintrin.h>
#define DM_HAS_SSE 1
#endif

namespace dm {

inline constexpr float kPi    = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;

[[nodiscard]] inline float dbToGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }
[[nodiscard]] inline float gainToDb(float g) noexcept { return 20.0f * std::log10(std::max(g, 1.0e-9f)); }
[[nodiscard]] inline float powToDb(float p) noexcept { return 10.0f * std::log10(std::max(p, 1.0e-18f)); }

[[nodiscard]] inline float clampf(float v, float lo, float hi) noexcept { return std::min(std::max(v, lo), hi); }
[[nodiscard]] inline float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }

// Exponential map of 0..1 onto lo..hi (both > 0). Used for frequency and time ranges.
[[nodiscard]] inline float expMap(float t, float lo, float hi) noexcept {
    return lo * std::pow(hi / lo, clampf(t, 0.0f, 1.0f));
}

// One-pole coefficient for a time constant in milliseconds.
[[nodiscard]] inline float onePoleCoef(float ms, float sampleRate) noexcept {
    if (ms <= 0.0f) return 0.0f;
    return std::exp(-1.0f / (ms * 0.001f * sampleRate));
}

[[nodiscard]] inline float flushDenormal(float v) noexcept {
    return (std::fabs(v) < 1.0e-20f) ? 0.0f : v;
}

// Flush-to-zero / denormals-are-zero for the calling thread while in scope.
class ScopedNoDenormals {
public:
    ScopedNoDenormals() noexcept {
#if DM_HAS_SSE
        saved_ = _mm_getcsr();
        _mm_setcsr(saved_ | 0x8040u);
#endif
    }
    ~ScopedNoDenormals() {
#if DM_HAS_SSE
        _mm_setcsr(saved_);
#endif
    }
    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;

private:
    unsigned saved_ = 0;
};

// Deterministic white-noise source (no allocation, no std::random state on the audio thread).
class Noise {
public:
    explicit Noise(std::uint32_t seed = 0x2545F491u) noexcept : s_(seed ? seed : 1u) {}
    [[nodiscard]] float next() noexcept {
        s_ ^= s_ << 13; s_ ^= s_ >> 17; s_ ^= s_ << 5;
        return static_cast<float>(static_cast<std::int32_t>(s_)) * (1.0f / 2147483648.0f);
    }
private:
    std::uint32_t s_;
};

} // namespace dm
