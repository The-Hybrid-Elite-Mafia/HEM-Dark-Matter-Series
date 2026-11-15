#pragma once
// HEM Dark Matter Series - smoothing, envelopes, gain computers, meters, LFO.
#include "dm/core/Math.hpp"

#include <atomic>

namespace dm {

// Linear-in-time-constant parameter smoother (one-pole). snap() jumps to target.
struct Smoother {
    float value = 0, target = 0, coef = 0;
    void prepare(float ms, float sampleRate) noexcept { coef = onePoleCoef(ms, sampleRate); }
    void snap(float v) noexcept { value = target = v; }
    void setTarget(float v) noexcept { target = v; }
    [[nodiscard]] float next() noexcept {
        value = target + coef * (value - target);
        if (std::fabs(value - target) < 1.0e-7f) value = target;
        return value;
    }
    // Advance n samples at once (block-rate use).
    float advance(int n) noexcept {
        value = target + std::pow(coef, static_cast<float>(n)) * (value - target);
        if (std::fabs(value - target) < 1.0e-7f) value = target;
        return value;
    }
};

// Attack/release envelope follower on a rectified (or squared) input.
struct EnvFollower {
    float env = 0, att = 0, rel = 0;
    void prepare(float attackMs, float releaseMs, float sampleRate) noexcept {
        att = onePoleCoef(attackMs, sampleRate);
        rel = onePoleCoef(releaseMs, sampleRate);
    }
    void reset() noexcept { env = 0; }
    [[nodiscard]] float process(float level) noexcept {
        const float c = level > env ? att : rel;
        env = flushDenormal(level + c * (env - level));
        return env;
    }
};

// Static compressor curve with soft knee. Returns gain change in dB (<= 0 for downward).
[[nodiscard]] inline float compressorGainDb(float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept {
    const float over = levelDb - thresholdDb;
    const float slope = 1.0f - 1.0f / std::max(ratio, 1.0f);
    if (kneeDb > 0.0f && std::fabs(over) <= kneeDb * 0.5f) {
        const float t = over + kneeDb * 0.5f;
        return -slope * t * t / (2.0f * kneeDb);
    }
    return over > 0.0f ? -slope * over : 0.0f;
}

// Peak meter with fall-back, published atomically for the UI thread.
struct PeakMeter {
    std::atomic<float> published{0.0f};
    float hold = 0, fall = 0;
    void prepare(float sampleRate, int blockSize) noexcept {
        // ~20 dB per second visual fall measured per block.
        fall = std::pow(10.0f, -20.0f / 20.0f * static_cast<float>(blockSize) / sampleRate);
    }
    void push(float blockPeak) noexcept {
        hold = std::max(blockPeak, hold * fall);
        published.store(hold, std::memory_order_relaxed);
    }
    void reset() noexcept { hold = 0; published.store(0.0f, std::memory_order_relaxed); }
    [[nodiscard]] float read() const noexcept { return published.load(std::memory_order_relaxed); }
};

// A dB value published for the UI (gain reduction, auto-gain, duck amount).
struct DbReadout {
    std::atomic<float> v{0.0f};
    void set(float db) noexcept { v.store(db, std::memory_order_relaxed); }
    [[nodiscard]] float read() const noexcept { return v.load(std::memory_order_relaxed); }
};

// Sine LFO with optional phase offset per channel.
struct Lfo {
    float phase = 0, inc = 0;
    void setRate(float hz, float sampleRate) noexcept { inc = hz / sampleRate; }
    void reset(float p = 0.0f) noexcept { phase = p; }
    [[nodiscard]] float next() noexcept {
        const float v = std::sin(kTwoPi * phase);
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        return v;
    }
    [[nodiscard]] float at(float offset) const noexcept {
        float p = phase + offset;
        p -= std::floor(p);
        return std::sin(kTwoPi * p);
    }
};

} // namespace dm
