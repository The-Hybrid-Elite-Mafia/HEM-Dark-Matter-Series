#pragma once
// HEM Dark Matter Series - granular helpers: crossfaded micro pitch shifter and a multi-grain cloud.
#include "dm/core/DelayLine.hpp"

#include <array>

namespace dm {

// Two taps half a window apart sweep through a delay window at (1 - ratio) samples per sample and are
// crossfaded with complementary sin^2 windows (constant power sum = 1), giving a click-free pitch shift.
class GrainShifter {
public:
    void prepare(int maxWindowSamples) { d_.prepare(maxWindowSamples * 2 + 8); }
    void reset() noexcept { d_.reset(); phase_ = 0; }
    [[nodiscard]] float process(float x, float ratio, float window, bool reverse = false) noexcept {
        d_.push(x);
        const float step = reverse ? (1.0f + ratio) : (1.0f - ratio);
        phase_ += step / window;
        phase_ -= std::floor(phase_);
        float p2 = phase_ + 0.5f;
        p2 -= std::floor(p2);
        const float d1 = 2.0f + phase_ * window, d2 = 2.0f + p2 * window;
        const float g1 = std::sin(kPi * phase_), g2 = std::sin(kPi * p2);
        return d_.readFrac(d1) * g1 * g1 + d_.readFrac(d2) * g2 * g2;
    }

private:
    DelayLine d_;
    float phase_ = 0;
};

// N overlapping shifted grains with per-grain jitter and pan: a texture "cloud".
template <int N>
class GrainCloud {
public:
    void prepare(int maxWindowSamples, std::uint32_t seed = 0x1234567u) {
        for (auto& s : sh_) s.prepare(maxWindowSamples + 64);
        maxWin_ = static_cast<float>(maxWindowSamples);
        seed_ = seed;
        reset();
    }
    // Back to the exact start: same grains, same jitter sequence (so two renders after reset() match).
    void reset() noexcept {
        noise_ = Noise(seed_);
        counter_ = 0;
        for (int i = 0; i < N; ++i) {
            sh_[static_cast<std::size_t>(i)].reset();
            jitter_[static_cast<std::size_t>(i)] = 0.0f;
        }
    }
    // spread 0..1 (stereo spread), scatter 0..1 (pitch jitter amount in +/- 25 cents), ratio = base pitch ratio.
    void process(float in, float ratio, float scatter, float spread, bool reverse, float& outL, float& outR) noexcept {
        outL = outR = 0;
        for (int i = 0; i < N; ++i) {
            const auto k = static_cast<std::size_t>(i);
            if ((counter_ & 1023) == static_cast<unsigned>(i * 1024 / N)) jitter_[k] = noise_.next();
            const float cents = 25.0f * scatter * jitter_[k];
            const float r = ratio * std::pow(2.0f, cents / 1200.0f);
            const float win = maxWin_ * (0.55f + 0.45f * static_cast<float>(i + 1) / N);
            const float y = sh_[k].process(in, r, win, reverse);
            const float pan = 0.5f + 0.5f * spread * ((i & 1) ? 1.0f : -1.0f) * (0.5f + 0.5f * static_cast<float>(i) / N);
            outL += y * (1.0f - pan);
            outR += y * pan;
        }
        ++counter_;
        outL *= 2.0f / N;
        outR *= 2.0f / N;
    }

private:
    std::array<GrainShifter, N> sh_{};
    std::array<float, N> jitter_{};
    float maxWin_ = 2048;
    unsigned counter_ = 0;
    std::uint32_t seed_ = 0x1234567u;
    Noise noise_{};
};

} // namespace dm
