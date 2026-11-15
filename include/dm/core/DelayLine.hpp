#pragma once
// HEM Dark Matter Series - power-of-two ring delay with fractional (Hermite) reads.
// Memory is allocated in prepare() only; process-time calls never allocate.
#include "dm/core/Math.hpp"

#include <vector>

namespace dm {

class DelayLine {
public:
    void prepare(int maxDelaySamples) {
        int n = 1;
        while (n < maxDelaySamples + 4) n <<= 1;
        buf_.assign(static_cast<std::size_t>(n), 0.0f);
        mask_ = n - 1;
        w_ = 0;
    }
    void reset() noexcept { std::fill(buf_.begin(), buf_.end(), 0.0f); w_ = 0; }
    [[nodiscard]] int capacity() const noexcept { return mask_ - 3; }

    void push(float x) noexcept {
        buf_[static_cast<std::size_t>(w_)] = x;
        w_ = (w_ + 1) & mask_;
    }
    // Integer read: d = 0 is the most recently pushed sample.
    [[nodiscard]] float read(int d) const noexcept {
        return buf_[static_cast<std::size_t>((w_ - 1 - d) & mask_)];
    }
    // Fractional read (Hermite 4-point). d >= 1 recommended.
    [[nodiscard]] float readFrac(float d) const noexcept {
        d = clampf(d, 1.0f, static_cast<float>(mask_ - 4));
        const int i = static_cast<int>(d);
        const float f = d - static_cast<float>(i);
        const float xm1 = read(i - 1), x0 = read(i), x1 = read(i + 1), x2 = read(i + 2);
        const float c0 = x0;
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }

private:
    std::vector<float> buf_;
    int mask_ = 0;
    int w_ = 0;
};

// Schroeder allpass diffuser built on a delay line.
class AllPassDiffuser {
public:
    void prepare(int maxSamples) { d_.prepare(maxSamples); }
    void reset() noexcept { d_.reset(); }
    [[nodiscard]] float process(float x, float delay, float g) noexcept {
        const float v = d_.readFrac(delay);
        const float u = x + g * v;
        d_.push(u);
        return v - g * u;
    }
private:
    DelayLine d_;
};

} // namespace dm
