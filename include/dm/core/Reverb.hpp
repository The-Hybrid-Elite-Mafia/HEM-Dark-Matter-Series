#pragma once
// HEM Dark Matter Series - 8-line feedback delay network reverb.
//   input diffusion (4 allpasses) -> 8 modulated delay lines -> Householder mix -> per-line damping
//   + RT60-correct per-line feedback gain + early-reflection taps + stereo width.
// All memory allocated in prepare(); process() is allocation-free.
#include "dm/core/DelayLine.hpp"
#include "dm/core/Dynamics.hpp"
#include "dm/core/Filters.hpp"

#include <array>

namespace dm {

struct ReverbSettings {
    float size        = 1.0f;     // 0.3 .. 2.5 (scales line lengths)
    float decaySec    = 2.0f;     // RT60, 0.1 .. 100
    float dampHz      = 8000.0f;  // in-loop high damping
    float diffusion   = 0.7f;     // 0 .. 0.85 input allpass gain
    float modDepthMs  = 0.3f;     // line-length modulation depth
    float modRateHz   = 0.5f;
    float earlyLevel  = 0.3f;     // 0 .. 1
    float predelayMs  = 10.0f;    // 0 .. 500
    float width       = 1.0f;     // 0 .. 1.5
};

class FdnReverb {
public:
    static constexpr int kLines = 8;
    static constexpr float kMaxSize = 2.5f;
    static constexpr float kMaxPredelayMs = 500.0f;

    void prepare(float sampleRate) {
        sr_ = sampleRate;
        for (int i = 0; i < kLines; ++i)
            lines_[static_cast<std::size_t>(i)].prepare(msToSamples(kBaseMs[i] * kMaxSize + 4.0f));
        for (int i = 0; i < 4; ++i)
            diff_[static_cast<std::size_t>(i)][0].prepare(msToSamples(kDiffMs[i] * 1.5f + 2.0f)),
            diff_[static_cast<std::size_t>(i)][1].prepare(msToSamples(kDiffMs[i] * 1.5f + 2.0f));
        pre_[0].prepare(msToSamples(kMaxPredelayMs + 120.0f));
        pre_[1].prepare(msToSamples(kMaxPredelayMs + 120.0f));
        lfo_.reset();
        reset();
        apply(settings_);
    }

    void reset() noexcept {
        lfo_.reset();   // modulation restarts at the same phase: same tail after every reset
        for (auto& l : lines_) l.reset();
        for (auto& d : diff_) { d[0].reset(); d[1].reset(); }
        pre_[0].reset(); pre_[1].reset();
        for (auto& f : damp_) f.reset();
    }

    void apply(const ReverbSettings& s) noexcept {
        settings_ = s;
        const float size = clampf(s.size, 0.3f, kMaxSize);
        const float decay = clampf(s.decaySec, 0.1f, 100.0f);
        for (int i = 0; i < kLines; ++i) {
            const float lenSamples = static_cast<float>(msToSamples(kBaseMs[i] * size));
            len_[static_cast<std::size_t>(i)] = lenSamples;
            gain_[static_cast<std::size_t>(i)] = std::pow(10.0f, -3.0f * lenSamples / (decay * sr_));
            damp_[static_cast<std::size_t>(i)].setCutoff(s.dampHz, sr_);
        }
        diffScale_ = 0.5f + 0.5f * size / kMaxSize;
        diffG_ = clampf(s.diffusion, 0.0f, 0.85f);
        modDepth_ = s.modDepthMs * 0.001f * sr_;
        lfo_.setRate(s.modRateHz, sr_);
        predelay_ = clampf(s.predelayMs, 0.0f, kMaxPredelayMs) * 0.001f * sr_;
        early_ = clampf(s.earlyLevel, 0.0f, 1.0f);
        width_ = clampf(s.width, 0.0f, 1.5f);
        sizeScale_ = size;
    }

    // Process one stereo frame; writes wet-only output.
    void process(float inL, float inR, float& outL, float& outR) noexcept {
        pre_[0].push(inL);
        pre_[1].push(inR);
        float l = pre_[0].readFrac(std::max(predelay_, 1.0f));
        float r = pre_[1].readFrac(std::max(predelay_, 1.0f));

        // Early reflections: sparse taps after the pre-delay.
        float eL = 0, eR = 0;
        if (early_ > 0.0f) {
            for (int t = 0; t < 6; ++t) {
                const float d = predelay_ + kEarlyMs[t] * sizeScale_ * 0.001f * sr_;
                const float g = kEarlyGain[t] * early_;
                if (t & 1) eR += g * pre_[1].readFrac(d);
                else       eL += g * pre_[0].readFrac(d);
            }
        }

        // Input diffusion.
        for (int i = 0; i < 4; ++i) {
            const float d = static_cast<float>(msToSamples(kDiffMs[i])) * diffScale_;
            l = diff_[static_cast<std::size_t>(i)][0].process(l, d, diffG_);
            r = diff_[static_cast<std::size_t>(i)][1].process(r, d * 1.07f, diffG_);
        }

        // Read lines (first 4 modulated in quadrature).
        std::array<float, kLines> y{};
        const float m = modDepth_;
        for (int i = 0; i < kLines; ++i) {
            float d = len_[static_cast<std::size_t>(i)];
            if (i < 4 && m > 0.0f) d += m * (1.0f + lfo_.at(0.25f * static_cast<float>(i)));
            y[static_cast<std::size_t>(i)] = lines_[static_cast<std::size_t>(i)].readFrac(d);
        }
        (void)lfo_.next();

        // Householder feedback matrix: y_i - (2/N) * sum(y).
        float sum = 0;
        for (float v : y) sum += v;
        const float hh = sum * (2.0f / kLines);
        for (int i = 0; i < kLines; ++i) {
            const auto k = static_cast<std::size_t>(i);
            float fb = (y[k] - hh) * gain_[k];
            fb = damp_[k].lp(fb);
            const float in = (i & 1) ? r : l;
            lines_[k].push(flushDenormal(fb + in * 0.5f));
        }

        float wl = (y[0] - y[2] + y[4] - y[6]) * 0.5f + eL;
        float wr = (y[1] - y[3] + y[5] - y[7]) * 0.5f + eR;
        const float mid = 0.5f * (wl + wr);
        const float side = 0.5f * (wl - wr) * width_;
        outL = mid + side;
        outR = mid - side;
    }

private:
    [[nodiscard]] int msToSamples(float ms) const noexcept { return static_cast<int>(ms * 0.001f * sr_) + 1; }

    static constexpr float kBaseMs[kLines] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f};
    static constexpr float kDiffMs[4] = {4.7f, 3.6f, 12.7f, 9.3f};
    static constexpr float kEarlyMs[6] = {7.1f, 11.3f, 17.9f, 23.3f, 31.7f, 41.9f};
    static constexpr float kEarlyGain[6] = {0.85f, 0.78f, 0.62f, 0.55f, 0.41f, 0.33f};

    float sr_ = 48000.0f;
    ReverbSettings settings_{};
    std::array<DelayLine, kLines> lines_{};
    std::array<std::array<AllPassDiffuser, 2>, 4> diff_{};
    std::array<DelayLine, 2> pre_{};
    std::array<OnePole, kLines> damp_{};
    std::array<float, kLines> len_{}, gain_{};
    Lfo lfo_{};
    float diffScale_ = 1, diffG_ = 0.7f, modDepth_ = 0, predelay_ = 0, early_ = 0.3f, width_ = 1, sizeScale_ = 1;
};

// Tempo-synced stereo delay with in-loop tone filter, optional ping-pong and tape wobble.
struct DelaySettings {
    float timeMs     = 375.0f;
    float feedback   = 0.35f;   // 0 .. 0.95
    float toneHz     = 6000.0f; // in-loop low-pass
    float lowCutHz   = 150.0f;  // in-loop high-pass
    bool  pingPong   = true;
    float wobbleMs   = 0.0f;    // tape wow depth
    float wobbleHz   = 0.6f;
    float drive      = 1.0f;    // in-loop saturation (1 = clean)
};

class TempoDelay {
public:
    static constexpr float kMaxMs = 4000.0f;

    void prepare(float sampleRate) {
        sr_ = sampleRate;
        const int n = static_cast<int>(kMaxMs * 0.001f * sr_) + 64;
        d_[0].prepare(n);
        d_[1].prepare(n);
        timeSmooth_.prepare(80.0f, sr_);
        timeSmooth_.snap(s_.timeMs * 0.001f * sr_);
        reset();
        apply(s_);
    }
    void reset() noexcept {
        d_[0].reset(); d_[1].reset();
        for (auto& f : lp_) f.reset();
        for (auto& f : hp_) f.reset();
        lfo_.reset();
    }
    void apply(const DelaySettings& s) noexcept {
        s_ = s;
        timeSmooth_.setTarget(clampf(s.timeMs, 1.0f, kMaxMs - 10.0f) * 0.001f * sr_);
        for (auto& f : lp_) f.setCutoff(s.toneHz, sr_);
        for (auto& f : hp_) f.setCutoff(s.lowCutHz, sr_);
        lfo_.setRate(s.wobbleHz, sr_);
        fb_ = clampf(s.feedback, 0.0f, 0.95f);
    }
    void process(float inL, float inR, float& outL, float& outR) noexcept {
        const float t = timeSmooth_.next();
        const float wob = s_.wobbleMs * 0.001f * sr_;
        const float w = wob > 0.0f ? wob * (1.0f + lfo_.next()) : 0.0f;
        const float yl = d_[0].readFrac(t + w);
        const float yr = d_[1].readFrac(t + w * 0.9f);
        float fl = hp_[0].hp(lp_[0].lp(yl));
        float fr = hp_[1].hp(lp_[1].lp(yr));
        if (s_.drive > 1.0f) {
            fl = std::tanh(fl * s_.drive) / s_.drive;
            fr = std::tanh(fr * s_.drive) / s_.drive;
        }
        if (s_.pingPong) {
            d_[0].push(flushDenormal(0.5f * (inL + inR) + fb_ * fr));
            d_[1].push(flushDenormal(fb_ * fl));
        } else {
            d_[0].push(flushDenormal(inL + fb_ * fl));
            d_[1].push(flushDenormal(inR + fb_ * fr));
        }
        outL = yl;
        outR = yr;
    }

private:
    float sr_ = 48000.0f, fb_ = 0.35f;
    DelaySettings s_{};
    std::array<DelayLine, 2> d_{};
    std::array<OnePole, 2> lp_{}, hp_{};
    Lfo lfo_{};
    Smoother timeSmooth_{};
};

// Note-value table for tempo sync (beats in quarter notes).
inline constexpr int kSyncCount = 14;
inline constexpr const char* kSyncNames[kSyncCount] = {
    "1/1", "1/2", "1/2d", "1/2t", "1/4", "1/4d", "1/4t", "1/8", "1/8d", "1/8t", "1/16", "1/16d", "1/16t", "1/32"};
inline constexpr float kSyncBeats[kSyncCount] = {
    4.0f, 2.0f, 3.0f, 4.0f / 3.0f, 1.0f, 1.5f, 2.0f / 3.0f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 0.375f, 1.0f / 6.0f, 0.125f};

[[nodiscard]] inline float syncToMs(int index, float bpm) noexcept {
    const int i = std::min(std::max(index, 0), kSyncCount - 1);
    return kSyncBeats[i] * 60000.0f / clampf(bpm, 20.0f, 400.0f);
}

} // namespace dm
