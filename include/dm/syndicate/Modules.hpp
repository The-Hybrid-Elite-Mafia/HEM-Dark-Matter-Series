#pragma once
// HEM SYNDICATE RACK - the six processing modules. Each module is used twice: inside the 5-column rack and
// as its own standalone plug-in (HEM SAT, HEM COMP, HEM TRANSIENT, HEM TONE, HEM MOD, HEM MAX).
// A module owns a contiguous slice of its host engine's parameter table: `update(v, old)` receives a
// pointer to the first parameter of that slice (old == nullptr means "force everything").
#include "dm/Assumed.hpp"
#include "dm/core/Analyzer.hpp"
#include "dm/core/Crossover.hpp"
#include "dm/core/DelayLine.hpp"
#include "dm/core/Dynamics.hpp"
#include "dm/core/Filters.hpp"
#include "dm/core/Grain.hpp"
#include "dm/core/Params.hpp"

#include <array>

namespace dm::sr {

namespace a = dm::assumed::sr;
using C = std::span<const char* const>;

inline constexpr const char* kSatNames[a::kSatModes] = {
    a::kSatModeTable[0].name, a::kSatModeTable[1].name, a::kSatModeTable[2].name, a::kSatModeTable[3].name,
    a::kSatModeTable[4].name, a::kSatModeTable[5].name, a::kSatModeTable[6].name, a::kSatModeTable[7].name};
inline constexpr const char* kCompNames[a::kCompModes] = {
    a::kCompModeTable[0].name, a::kCompModeTable[1].name, a::kCompModeTable[2].name, a::kCompModeTable[3].name,
    a::kCompModeTable[4].name, a::kCompModeTable[5].name, a::kCompModeTable[6].name, a::kCompModeTable[7].name};
inline constexpr const char* kToneNames[a::kToneModes] = {
    a::kToneModeTable[0].name, a::kToneModeTable[1].name, a::kToneModeTable[2].name, a::kToneModeTable[3].name,
    a::kToneModeTable[4].name, a::kToneModeTable[5].name, a::kToneModeTable[6].name, a::kToneModeTable[7].name};
inline constexpr const char* kModNames[a::kModModes] = {
    a::kModModeTable[0].name, a::kModModeTable[1].name, a::kModModeTable[2].name,
    a::kModModeTable[3].name, a::kModModeTable[4].name, a::kModModeTable[5].name};
inline constexpr const char* kMaxNames[a::kMaxModes] = {
    a::kMaxModeTable[0].name, a::kMaxModeTable[1].name, a::kMaxModeTable[2].name, a::kMaxModeTable[3].name,
    a::kMaxModeTable[4].name, a::kMaxModeTable[5].name, a::kMaxModeTable[6].name};
inline constexpr const char* kSlopeNames[4] = {"x1", "x2", "x4", "x8"}; // 6 / 12 / 24 / 48 dB per octave
inline constexpr const char* kClipNames[4] = {"OFF", "SOFT", "HARD", "MST"};

// ===================================== SATURATE ========================================================
class SatModule {
public:
    enum : std::size_t { Mode, Amount, Character, kN };
    static constexpr std::array<ParamDesc, kN> kParams = {{
        {"sat_mode", "SATURATE MODE", 0, a::kSatModes - 1, 2, "", C(kSatNames)},
        {"saturate", "SATURATE", 0, 100, a::kSatDefault, "%"},
        {"character", "CRISP / WARM", -100, 100, 0, "%"},
    }};
    void prepare(float sr);
    void reset() noexcept;
    void update(const float* v, const float* old) noexcept;
    void process(float& l, float& r) noexcept;
    [[nodiscard]] float satMeterDb() const noexcept { return satDb_; } // harmonic energy added (UI SAT meter)
    void endChunk() noexcept;
private:
    float sr_ = 48000, drive_ = 1, satAcc_ = 0, sigAcc_ = 0, satDb_ = -120;
    const a::SatModeTune* m_ = &a::kSatModeTable[2];
    StereoBiquad pre_, de_, bump_, tiltLo_, tiltHi_;
    std::array<OnePole, 2> hf_{};
    std::array<DelayLine, 2> wow_{};
    Lfo wowLfo_{}, flutLfo_{};
};

// ===================================== COMP ============================================================
class CompModule {
public:
    enum : std::size_t { Mode, Amount, Color, Sidechain, kN };
    static constexpr std::array<ParamDesc, kN> kParams = {{
        {"comp_mode", "DYNAMICS MODE", 0, a::kCompModes - 1, 1, "", C(kCompNames)},
        {"comp", "COMP", 0, 100, a::kCompDefault, "%"},
        {"color", "COLOR", 0, 100, a::kCompColorDefault, "%"},
        {"sidechain", "SIDECHAIN", 0, 100, a::kSidechainDefault, "%"},
    }};
    void prepare(float sr);
    void reset() noexcept;
    void update(const float* v, const float* old) noexcept;
    void process(float& l, float& r) noexcept;
    [[nodiscard]] float grDb() const noexcept { return grPublished_; }
    void endChunk() noexcept { grPublished_ = grMax_; grMax_ = 0; }
private:
    float sr_ = 48000, thr_ = 0, ratio_ = 1, makeup_ = 0, colorMix_ = 0, colorDrive_ = 1, lastGr_ = 0;
    float optoFast_ = 0, optoSlow_ = 0, grMax_ = 0, grPublished_ = 0;
    const a::CompModeTune* m_ = &a::kCompModeTable[1];
    EnvFollower env_{};
    StereoBiquad scHp_{};
};

// ===================================== TRANSIENT =======================================================
class TransientModule {
public:
    enum : std::size_t { Amount, Shape, kN };
    static constexpr std::array<ParamDesc, kN> kParams = {{
        {"transient", "TRANSIENT", -100, 100, 0, "%"},
        {"shape", "SHAPE", 0, 100, 50, "%"},
    }};
    void prepare(float sr);
    void reset() noexcept;
    void update(const float* v, const float* old) noexcept;
    void process(float& l, float& r) noexcept;
    [[nodiscard]] bool active() const noexcept { return amount_ != 0.0f; }
    [[nodiscard]] float envelopeDb() const noexcept { return envDb_; } // live TRANSIENT curve value
private:
    float sr_ = 48000, amount_ = 0, envDb_ = 0;
    EnvFollower fast_{}, slow_{};
    Smoother gain_{};
};

// ===================================== TONE ============================================================
class ToneModule {
public:
    enum : std::size_t { Mode, Sub, Low, Mid, HiMid, High, Air, Hpf, Lpf, Slope,
                         SubF, LowF, MidF, HiMidF, HighF, AirF, kN };
    static constexpr std::array<ParamDesc, kN> kParams = {{
        {"tone_mode", "TONE MODE", 0, a::kToneModes - 1, 4, "", C(kToneNames)},
        {"sub", "SUB", -12, 12, 0, "dB"},
        {"low", "LOW", -12, 12, 0, "dB"},
        {"mid", "MID", -12, 12, 0, "dB"},
        {"hi_mid", "HI MID", -12, 12, 0, "dB"},
        {"high", "HIGH", -12, 12, 0, "dB"},
        {"air", "AIR+", -12, 12, 0, "dB"},
        {"hpf", "HPF", 10, 500, a::kToneHpfDefault, "Hz"},
        {"lpf", "LPF", 2000, 22000, a::kToneLpfDefault, "Hz"},
        {"slope", "SLOPE", 0, 3, 1, "", C(kSlopeNames)},
        // TONE HANDLES: drag a band left/right on the curve = move it up to +/- 1 octave from the mode's frequency.
        {"sub_freq", "SUB HANDLE", -100, 100, 0, "%"},
        {"low_freq", "LOW HANDLE", -100, 100, 0, "%"},
        {"mid_freq", "MID HANDLE", -100, 100, 0, "%"},
        {"hi_mid_freq", "HI MID HANDLE", -100, 100, 0, "%"},
        {"high_freq", "HIGH HANDLE", -100, 100, 0, "%"},
        {"air_freq", "AIR+ HANDLE", -100, 100, 0, "%"},
    }};
    void prepare(float sr);
    void reset() noexcept;
    void update(const float* v, const float* old) noexcept;
    void process(float& l, float& r) noexcept;
    [[nodiscard]] AnalyzerTap& analyzer() noexcept { return analyzer_; }
    // UI curve: combined EQ + filter response at hz for the given parameter slice.
    [[nodiscard]] static float curveDb(const float* v, float hz, float sr) noexcept;
private:
    static BiquadCoefs bandCoefs(const a::RackToneModeTune& m, int b, float gain, float shift, float sr) noexcept;
    static int slopeSections(int slope, float* qs) noexcept;
    float sr_ = 48000;
    const a::RackToneModeTune* m_ = &a::kToneModeTable[4];
    std::array<StereoBiquad, 6> band_{};
    std::array<bool, 6> bandOn_{};
    std::array<StereoBiquad, 4> hpf_{}, lpf_{};
    std::array<OnePole, 2> hp1_{}, lp1_{};
    bool hpfOn_ = false, lpfOn_ = false;
    int slope_ = 1, sections_ = 1;
    // AIR+ exciter (dynamic part)
    float airDyn_ = 0;
    StereoBiquad airHp_{};
    EnvFollower airEnv_{};
    AnalyzerTap analyzer_{};
};

// ===================================== MODULATE ========================================================
class ModModule {
public:
    enum : std::size_t { Mode, Amount, Depth, Drift, Analog, kN };
    static constexpr std::array<ParamDesc, kN> kParams = {{
        {"mod_mode", "MODULATE MODE", 0, a::kModModes - 1, 0, "", C(kModNames)},
        {"mod", "MOD", 0, 100, a::kModDefault, "%"},
        {"depth", "DEPTH", 0, 100, a::kModDepthDefault, "%"},
        {"drift", "DRIFT", 0, 100, 0, "%"},
        {"analog", "ANALOG", 0, 100, 0, "%"},
    }};
    void prepare(float sr);
    void reset() noexcept;
    void update(const float* v, const float* old) noexcept;
    void process(float& l, float& r) noexcept;
    [[nodiscard]] float correlation() const noexcept { return corr_; } // stereo phase meter (-1..+1)
    void endChunk() noexcept;
private:
    float sr_ = 48000, mix_ = 0, depth_ = 0.5f, drift_ = 0, analog_ = 0;
    const a::ModModeTune* m_ = &a::kModModeTable[0];
    std::array<DelayLine, 2> d_{}, wow_{};
    std::array<GrainShifter, 2> dbl_{};
    std::array<std::array<AllPass1, 4>, 2> ph_{};
    std::array<float, 2> fb_{};
    std::array<Lfo, 4> lfo_{};
    Lfo driftLfo_{}, wowLfo_{}, flutLfo_{};
    StereoBiquad sideHp_{};
    double xl_ = 0, xr_ = 0, xlr_ = 0;
    float corr_ = 1;
};

// ===================================== MAXIMIZE ========================================================
class MaxModule {
public:
    enum : std::size_t { Mode, Multi, Max, XoverLow, XoverHigh, Clip, BandLow, BandMid, BandHigh, kN };
    static constexpr std::array<ParamDesc, kN> kParams = {{
        {"max_mode", "MAXIMIZE MODE", 0, a::kMaxModes - 1, 0, "", C(kMaxNames)},
        {"multi", "MULTI", 0, 100, a::kMultiDefault, "%"},
        {"max", "MAX", 0, 100, a::kMaxDefault, "%"},
        {"xover_low", "LOW X-OVER", 60, 600, a::kXoverLowDef, "Hz"},
        {"xover_high", "HIGH X-OVER", 2000, 12000, a::kXoverHighDef, "Hz"},
        {"clip", "CLIP", 0, 3, 1, "", C(kClipNames)},
        {"band_low", "LOW AMOUNT", 0, 100, 100, "%"},
        {"band_mid", "MID AMOUNT", 0, 100, 100, "%"},
        {"band_high", "HIGH AMOUNT", 0, 100, 100, "%"},
    }};
    void prepare(float sr);
    void reset() noexcept;
    void update(const float* v, const float* old) noexcept;
    void process(float& l, float& r) noexcept;
    // Per-band activity 0..100 % (LOW / MID / HIGH meters).
    [[nodiscard]] float bandActivity(int b) const noexcept { return act_[static_cast<std::size_t>(b)]; }
    void endChunk() noexcept;
private:
    float sr_ = 48000, multi_ = 0, push_ = 1, downThr_ = 0, upThr_ = -60, downRatio_ = 1, upRatio_ = 1, upRange_ = 0;
    int clip_ = 1;
    const a::MaxModeTune* m_ = &a::kMaxModeTable[0];
    ThreeBandSplit split_{};
    std::array<EnvFollower, 3> env_{};
    std::array<float, 3> bandGain_{}, bandAmt_{1, 1, 1}, actAcc_{}, act_{};
    float limG_ = 1, limRel_ = 0, limThr_ = 1, limGr_ = 0, limGrMax_ = 0;
public:
    [[nodiscard]] float limiterGrDb() const noexcept { return limGr_; }
};

} // namespace dm::sr
