#pragma once
// HEM Dark Matter Series - shared plug-in shell. Every engine after PHANTOM VOX derives from this, so the
// brief's global rules live in exactly one place:
//   top bar BYPASS (10 ms crossfade, bit-exact when settled), bottom INPUT / OUTPUT trims + LED meters,
//   K-weighted AUTO-GAIN, -0.1 dBFS look-ahead brickwall limiter, latency-aligned dry path,
//   chunked parameter updates (no allocation, no locks on the audio thread).
//
// Convention: parameter indices 0..3 of every engine are INPUT, OUTPUT, BYPASS, AUTO GAIN.
//
// Derived must provide:
//   void prepareCore(float sr);                 // allocate here
//   void resetCore() noexcept;
//   void updateCore(const float* v, const float* old, bool force) noexcept;  // old == nullptr on force
//   void processCore(float& l, float& r) noexcept;                          // one stereo frame
//   [[nodiscard]] int coreLatency() const noexcept;                         // samples
//   [[nodiscard]] bool coreActive() const noexcept;                         // false -> core skipped (CPU)
#include "dm/Assumed.hpp"
#include "dm/core/Dynamics.hpp"
#include "dm/core/Params.hpp"
#include "dm/core/Safety.hpp"

#include <array>

namespace dm {

enum GlobalParam : std::size_t { G_Input = 0, G_Output = 1, G_Bypass = 2, G_AutoGain = 3, G_Count = 4 };

struct ShellMeters {
    PeakMeter inL, inR, outL, outR;
    DbReadout autoGainDb, limiterDb;
};

// Global parameter descriptors shared by every engine (prepended to each table).
inline constexpr std::array<ParamDesc, 4> kGlobalParams = {{
    {"input",    "INPUT",     -24, 24, 0, "dB"},
    {"output",   "OUTPUT",    -24, 24, 0, "dB"},
    {"bypass",   "BYPASS",      0,  1, 0, "toggle"},
    {"autogain", "AUTO GAIN",   0,  1, 1, "toggle"},
}};

// Concatenate parameter tables at compile time.
template <std::size_t A, std::size_t B>
constexpr std::array<ParamDesc, A + B> concatParams(const std::array<ParamDesc, A>& a, const std::array<ParamDesc, B>& b) {
    std::array<ParamDesc, A + B> out{};
    for (std::size_t i = 0; i < A; ++i) out[i] = a[i];
    for (std::size_t i = 0; i < B; ++i) out[A + i] = b[i];
    return out;
}

// Variadic form: concatAll(a, b, c, ...).
template <std::size_t A>
constexpr std::array<ParamDesc, A> concatAll(const std::array<ParamDesc, A>& a) { return a; }
template <std::size_t A, std::size_t B, class... Rest>
constexpr auto concatAll(const std::array<ParamDesc, A>& a, const std::array<ParamDesc, B>& b, const Rest&... rest) {
    return concatAll(concatParams(a, b), rest...);
}

template <class Derived, std::size_t N>
class EngineShell {
public:
    explicit EngineShell(const std::array<ParamDesc, N>& table) : params_(table) { cached_.fill(-1.0e30f); }

    void prepare(float sampleRate, int maxBlock) {
        sr_ = sampleRate;
        maxBlock_ = std::max(1, maxBlock);
        inTrim_.prepare(20.0f, sr_);
        outTrim_.prepare(20.0f, sr_);
        bypassMix_.prepare(assumed::kBypassFadeMs / 4.6f, sr_);
        autoGain_.prepare(sr_, self().autoGainWindowMs(), assumed::kAutoGainMaxDb);
        limiter_.prepare(sr_, self().limiterLookaheadMs(), assumed::kLimiterReleaseMs);
        self().prepareCore(sr_);
        const int maxLat = self().maxCoreLatency() + limiter_.latency();
        for (auto& d : dryDelay_) d.prepare(maxLat + 8);
        for (auto& d : refDelay_) d.prepare(self().maxCoreLatency() + 8);
        for (PeakMeter* m : {&meters_.inL, &meters_.inR, &meters_.outL, &meters_.outR}) m->prepare(sr_, assumed::kChunk);
        reset();
    }

    void reset() noexcept {
        autoGain_.reset();
        limiter_.reset();
        for (auto& d : dryDelay_) d.reset();
        for (auto& d : refDelay_) d.reset();
        self().resetCore();
        cached_.fill(-1.0e30f);
        update(true);
        inTrim_.snap(inTrim_.target);
        outTrim_.snap(outTrim_.target);
        bypassMix_.snap(bypassMix_.target);
    }

    // External sidechain (key) for the NEXT process() call only: two buffers of numSamples, not trimmed,
    // aligned with the input. nullptr = no key. Engines read it through keyFrame().
    void setSidechain(const float* left, const float* right) noexcept { keyIn_[0] = left; keyIn_[1] = right; }

    void process(float* left, float* right, int numSamples) noexcept {
        ScopedNoDenormals noDenormals;
        keyValid_ = keyIn_[0] != nullptr && keyIn_[1] != nullptr;
        ranCore_ = false;
        const int lat = latencySamples();
        const int coreLat = self().coreLatency();
        int pos = 0;
        while (pos < numSamples) {
            const int n = std::min(assumed::kChunk, numSamples - pos);
            update(false);
            const bool fullyBypassed = bypassMix_.value >= 1.0f && bypassMix_.target >= 1.0f;
            const bool core = self().coreActive();
            float inPk[2] = {0, 0}, outPk[2] = {0, 0}, agDb = 0;
            for (int i = pos; i < pos + n; ++i) {
                dryDelay_[0].push(left[i]);
                dryDelay_[1].push(right[i]);
                const float alignedL = dryDelay_[0].read(lat), alignedR = dryDelay_[1].read(lat);
                const float bp = bypassMix_.next();
                if (fullyBypassed) { left[i] = alignedL; right[i] = alignedR; continue; }

                const float it = inTrim_.next();
                float l = left[i] * it, r = right[i] * it;
                inPk[0] = std::max(inPk[0], std::fabs(l));
                inPk[1] = std::max(inPk[1], std::fabs(r));
                refDelay_[0].push(l);
                refDelay_[1].push(r);
                const float refL = refDelay_[0].read(coreLat), refR = refDelay_[1].read(coreLat);
                if (keyValid_) { keyNow_[0] = keyIn_[0][i]; keyNow_[1] = keyIn_[1][i]; }
                if (core) self().processCore(l, r);
                else { l = refL; r = refR; }

                if (autoGainOn_) {
                    const float g = autoGain_.process(refL, refR, l, r);
                    l *= g; r *= g;
                    agDb = gainToDb(g);
                }
                const float ot = outTrim_.next();
                l *= ot; r *= ot;
                limiter_.process(l, r);
                outPk[0] = std::max(outPk[0], std::fabs(l));
                outPk[1] = std::max(outPk[1], std::fabs(r));
                if (bp > 0.0f) { l = lerp(l, alignedL, bp); r = lerp(r, alignedR, bp); }
                left[i] = l;
                right[i] = r;
            }
            ranCore_ |= core && !fullyBypassed;
            meters_.inL.push(inPk[0]); meters_.inR.push(inPk[1]);
            meters_.outL.push(outPk[0]); meters_.outR.push(outPk[1]);
            meters_.autoGainDb.set(agDb);
            meters_.limiterDb.set(limiter_.gainReductionDb());
            self().afterChunk(n);
            pos += n;
        }
        keyIn_[0] = keyIn_[1] = nullptr;   // a key is good for one call only
        keyValid_ = false;
    }

    [[nodiscard]] int latencySamples() const noexcept { return self().coreLatency() + limiter_.latency(); }
    // Current key frame {l, r} while process() runs with a sidechain, else nullptr.
    [[nodiscard]] const float* keyFrame() const noexcept { return keyValid_ ? keyNow_ : nullptr; }
    [[nodiscard]] ParamSet<N>& params() noexcept { return params_; }
    [[nodiscard]] const ParamSet<N>& params() const noexcept { return params_; }
    [[nodiscard]] const ShellMeters& shellMeters() const noexcept { return meters_; }
    [[nodiscard]] bool ranCore() const noexcept { return ranCore_; }
    [[nodiscard]] float sampleRate() const noexcept { return sr_; }

    bool loadPresetValues(std::span<const PresetValue> values) noexcept {
        params_.resetToDefaults();
        bool ok = true;
        for (const auto& v : values) {
            const int i = params_.indexOf(v.id);
            if (i >= 0) params_.set(static_cast<std::size_t>(i), v.value); else ok = false;
        }
        return ok;
    }

    // Hooks with defaults (override in Derived when needed).
    [[nodiscard]] int maxCoreLatency() const noexcept { return self().coreLatency(); }
    // 0 = zero-latency safety limiter (instant gain + hard ceiling); > 0 = look-ahead limiter.
    [[nodiscard]] float limiterLookaheadMs() const noexcept { return assumed::kLimiterLookaheadMs; }
    [[nodiscard]] float autoGainWindowMs() const noexcept { return assumed::kAutoGainWindowMs; }
    void afterChunk(int) noexcept {}

protected:
    Derived& self() noexcept { return static_cast<Derived&>(*this); }
    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }

    void update(bool force) noexcept {
        std::array<float, N> v{};
        bool changed = force;
        for (std::size_t i = 0; i < N; ++i) {
            v[i] = params_.get(i);
            if (v[i] != cached_[i]) changed = true;
        }
        if (!changed) return;
        inTrim_.setTarget(dbToGain(v[G_Input]));
        outTrim_.setTarget(dbToGain(v[G_Output]));
        bypassMix_.setTarget(v[G_Bypass] >= 0.5f ? 1.0f : 0.0f);
        autoGainOn_ = v[G_AutoGain] >= 0.5f;
        self().updateCore(v.data(), force ? nullptr : cached_.data(), force);
        cached_ = v;
    }

    // Helper for updateCore: did any of these indices change?
    static bool changed(const float* v, const float* old, std::initializer_list<std::size_t> ids) noexcept {
        if (!old) return true;
        for (auto i : ids) if (v[i] != old[i]) return true;
        return false;
    }

    ParamSet<N> params_;
    const float* keyIn_[2]{};
    float keyNow_[2]{};
    bool keyValid_ = false;
    std::array<float, N> cached_{};
    float sr_ = 48000.0f;
    int maxBlock_ = 512;

private:
    ShellMeters meters_{};
    Smoother inTrim_{}, outTrim_{}, bypassMix_{};
    AutoGain autoGain_{};
    BrickwallLimiter limiter_{};
    std::array<DelayLine, 2> dryDelay_{}, refDelay_{};
    bool autoGainOn_ = true, ranCore_ = false;
};

} // namespace dm
