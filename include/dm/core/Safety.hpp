#pragma once
// HEM Dark Matter Series - global safety stage required by the brief for every plugin:
//   * K-weighted RMS auto-gain compensation (processed level tracks input level)
//   * look-ahead brickwall limiter with a -0.1 dBFS ceiling and a final hard clamp
#include "dm/core/DelayLine.hpp"
#include "dm/core/Dynamics.hpp"
#include "dm/core/Filters.hpp"

#include <array>
#include <vector>

namespace dm {

// Approximation of the ITU-R BS.1770 K-weighting pre-filter (high shelf +4 dB @1.5 kHz, HP @38 Hz).
struct KWeight {
    std::array<StereoBiquad, 2> st{};
    void prepare(float sr) noexcept {
        st[0].set(designBiquad(BiquadType::HighShelf, 1500.0f, 0.707f, 4.0f, sr));
        st[1].set(designBiquad(BiquadType::HighPass, 38.0f, 0.5f, 0.0f, sr));
        reset();
    }
    void reset() noexcept { st[0].reset(); st[1].reset(); }
    [[nodiscard]] float process(int ch, float x) noexcept { return st[1].process(ch, st[0].process(ch, x)); }
};

class AutoGain {
public:
    // windowMs: RMS integration time; maxDb: correction limit both ways.
    void prepare(float sampleRate, float windowMs = 400.0f, float maxDb = 12.0f) {
        sr_ = sampleRate;
        coef_ = onePoleCoef(windowMs, sr_);
        maxDb_ = maxDb;
        kIn_.prepare(sr_);
        kOut_.prepare(sr_);
        gain_.prepare(250.0f, sr_);
        reset();
    }
    void reset() noexcept {
        pIn_ = pOut_ = 0;
        kIn_.reset(); kOut_.reset();
        gain_.snap(1.0f);
    }
    // Feed one frame of the reference (pre-processing) and processed signal; returns the gain to apply.
    [[nodiscard]] float process(float inL, float inR, float outL, float outR) noexcept {
        const float a = kIn_.process(0, inL), b = kIn_.process(1, inR);
        const float c = kOut_.process(0, outL), d = kOut_.process(1, outR);
        pIn_  = flushDenormal((a * a + b * b) + coef_ * (pIn_ - (a * a + b * b)));
        pOut_ = flushDenormal((c * c + d * d) + coef_ * (pOut_ - (c * c + d * d)));
        // Freeze while the input is near silence so tails and gates don't pump the gain.
        if (pIn_ > kSilencePow && pOut_ > 1.0e-14f) {
            float g = std::sqrt(pIn_ / pOut_);
            g = clampf(g, dbToGain(-maxDb_), dbToGain(maxDb_));
            gain_.setTarget(g);
        }
        return gain_.next();
    }
    [[nodiscard]] float currentDb() const noexcept { return gainToDb(gain_.value); }

private:
    static constexpr float kSilencePow = 2.0e-6f; // about -60 dBFS (stereo sum of squares)
    float sr_ = 48000.0f, coef_ = 0, maxDb_ = 12.0f, pIn_ = 0, pOut_ = 0;
    KWeight kIn_{}, kOut_{};
    Smoother gain_{};
};

// Look-ahead peak limiter. Latency = lookahead samples (reported to the host).
class BrickwallLimiter {
public:
    static constexpr float kCeilingDb = -0.1f;

    void prepare(float sampleRate, float lookaheadMs = 1.5f, float releaseMs = 80.0f) {
        sr_ = sampleRate;
        look_ = std::max(0, static_cast<int>(lookaheadMs * 0.001f * sr_));
        d_[0].prepare(look_ + 4);
        d_[1].prepare(look_ + 4);
        ring_.assign(static_cast<std::size_t>(look_ + 1), 1.0f);
        dq_.assign(static_cast<std::size_t>(look_ + 2), 0);
        rel_ = onePoleCoef(releaseMs, sr_);
        ceiling_ = dbToGain(kCeilingDb);
        attackCoef_ = look_ > 0 ? std::exp(-4.6f / static_cast<float>(look_)) : 0.0f; // ~99% of a step inside the look-ahead; 0 = instant
        reset();
    }
    void reset() noexcept {
        d_[0].reset(); d_[1].reset();
        std::fill(ring_.begin(), ring_.end(), 1.0f);
        head_ = tail_ = 0; idx_ = 0; g_ = 1.0f; grDb_ = 0;
    }
    [[nodiscard]] int latency() const noexcept { return look_; }
    [[nodiscard]] float gainReductionDb() const noexcept { return grDb_; }

    void process(float& l, float& r) noexcept {
        const float peak = std::max(std::fabs(l), std::fabs(r));
        const float need = peak > ceiling_ ? ceiling_ / peak : 1.0f;

        // Sliding-window minimum of required gain over the last look_+1 samples
        // (monotonic deque stored in fixed rings; no allocation).
        const int n = look_ + 1;
        const int cap = static_cast<int>(dq_.size());
        ring_[static_cast<std::size_t>(idx_ % n)] = need;
        while (head_ != tail_) {
            const int back = (tail_ - 1 + cap) % cap;
            if (ring_[static_cast<std::size_t>(dq_[static_cast<std::size_t>(back)] % n)] < need) break;
            tail_ = back;
        }
        dq_[static_cast<std::size_t>(tail_)] = idx_;
        tail_ = (tail_ + 1) % cap;
        while (dq_[static_cast<std::size_t>(head_)] <= idx_ - n) head_ = (head_ + 1) % cap;
        const float target = ring_[static_cast<std::size_t>(dq_[static_cast<std::size_t>(head_)] % n)];
        if (++idx_ >= kRebase) { // keep indices small; shift by a multiple of n so ring slots stay valid
            const int by = (kRebase / 2) / n * n;
            idx_ -= by;
            for (int i = head_; i != tail_; i = (i + 1) % cap) dq_[static_cast<std::size_t>(i)] -= by;
        }

        // Attack ramps toward the window minimum inside the look-ahead; release is a one-pole.
        if (target < g_) g_ = target + attackCoef_ * (g_ - target);
        else             g_ = target + rel_ * (g_ - target);

        d_[0].push(l);
        d_[1].push(r);
        l = d_[0].read(look_) * g_;
        r = d_[1].read(look_) * g_;
        // Final guarantee: nothing leaves above the ceiling.
        l = clampf(l, -ceiling_, ceiling_);
        r = clampf(r, -ceiling_, ceiling_);
        grDb_ = -gainToDb(g_);
    }

private:
    static constexpr int kRebase = 1 << 30;

    float attackCoef_ = 0.9f, sr_ = 48000.0f, rel_ = 0, ceiling_ = 0.9886f, g_ = 1.0f, grDb_ = 0;
    int look_ = 72, head_ = 0, tail_ = 0, idx_ = 0;
    std::array<DelayLine, 2> d_{};
    std::vector<float> ring_;
    std::vector<int> dq_;
};

} // namespace dm
