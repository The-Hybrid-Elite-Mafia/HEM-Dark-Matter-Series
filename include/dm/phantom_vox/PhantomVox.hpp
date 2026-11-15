#pragma once
// =====================================================================================================
//  HEM PHANTOM VOX - 4-bay vocal channel strip engine (DYNAMICS / TONE / SPACE / SFX + ENHANCER)
//  Signal flow (per sample, stereo):
//    INPUT trim -> [filters if FLT POST off] -> DYNAMICS (gate -> comp -> de-ess -> color) -> trim
//    -> TONE (LOW/MID/HIGH/AIR+ EQ, mode color) -> [filters if FLT POST on] -> trim
//    -> SFX (11 modes) -> ENHANCER -> trim
//    -> SPACE: send taken pre-SFX (SPC POST off) or post-SFX (SPC POST on); reverb + delay returns,
//       each ducked by the dry vocal, SPACE TONE filtered, mixed back in -> trim
//    -> AUTO-GAIN (K-weighted RMS match to input) -> OUTPUT trim -> brickwall limiter (-0.1 dBFS)
//    -> BYPASS crossfade against latency-aligned dry.
//  Audio thread: process() never allocates or locks. All buffers are sized in prepare().
// =====================================================================================================
#include "dm/Assumed.hpp"
#include "dm/core/Analyzer.hpp"
#include "dm/core/DelayLine.hpp"
#include "dm/core/Dynamics.hpp"
#include "dm/core/Filters.hpp"
#include "dm/core/Params.hpp"
#include "dm/core/Reverb.hpp"
#include "dm/core/Safety.hpp"

#include <array>
#include <span>

namespace dm::pv {

enum P : std::size_t {
    InputTrim, OutputTrim, Bypass, AutoGainOn,
    DynOn, DynSolo, DynMode, DynTrim, Gate, Comp, Color, DeEss, Focus,
    ToneOn, ToneSolo, ToneMode, ToneTrim, Low, Mid, High, Air, VoiceMV, Hpf, Lpf,
    SpaceOn, SpaceSolo, SpaceMode, SpaceTrim, ReverbMix, ReverbTime, DelayMix, DelaySync, VrbDuck, DlyDuck,
    SpaceTone, Tempo,
    SfxOn, SfxSolo, SfxMode, SfxTrim, Sfx, Enhancer,
    SpcPost, FltPost,
    kParamCount
};

const std::array<ParamDesc, kParamCount>& paramTable() noexcept;
std::span<const Preset> factoryPresets() noexcept;

// Values published for faceplate meters (read on the UI thread).
struct Meters {
    PeakMeter inL, inR, outL, outR;
    DbReadout gr, ds, gateOpen, autoGainDb, limiterDb;
    PeakMeter revIn, revOut, dlyIn, dlyOut;
    DbReadout revDuck, dlyDuck;
};

// SFX bay: one class, eleven real algorithms selected by mode.
class SfxUnit {
public:
    void prepare(float sampleRate);
    void reset() noexcept;
    void setMode(int mode) noexcept;
    void setAmount(float amount01) noexcept;   // SFX macro 0..1
    void setEnhancer(float amount01) noexcept; // ENHANCER macro 0..1
    void process(float& l, float& r) noexcept;

private:
    struct Shifter { // two-tap crossfaded micro pitch shifter
        DelayLine d; float phase = 0;
        void prepare(int maxSamples) { d.prepare(maxSamples); }
        void reset() noexcept { d.reset(); phase = 0; }
        [[nodiscard]] float process(float x, float ratio, float window) noexcept;
    };
    void updateFilters() noexcept;
    [[nodiscard]] float tape(int ch, float x, float drive, float satMix, float wowMs, float flutterMs) noexcept;

    float sr_ = 48000.0f, amt_ = 0.25f, enh_ = 0.15f;
    int mode_ = 0;
    std::array<DelayLine, 2> mod_{};                 // chorus / wow delay
    std::array<std::array<DelayLine, 1>, 2> uni_{};  // unison voices share one line per channel
    std::array<Shifter, 2> shift_{};
    std::array<std::array<AllPass1, 6>, 2> ph_{};
    std::array<float, 2> phFb_{};
    Lfo lfoA_{}, lfoB_{}, lfoC_{}, lfoWow_{}, lfoFlut_{};
    std::array<Lfo, 4> lfoUni_{};
    std::array<OnePole, 2> hfLoss_{};
    StereoBiquad bandHp_{}, bandLp_{}, bandHp2_{}, bandLp2_{}, ampMid_{}, ampCab_{}, ampCab2_{}, lowShelf_{}, presence_{};
    std::array<DcBlocker, 2> dc_{};
    StereoBiquad enhHp_{}, enhHp2_{}, enhBody_{};
};

class PhantomVox {
public:
    PhantomVox();

    void prepare(float sampleRate, int maxBlock);
    void reset() noexcept;
    void process(float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] int latencySamples() const noexcept { return limiter_.latency(); }
    [[nodiscard]] ParamSet<kParamCount>& params() noexcept { return params_; }
    [[nodiscard]] const Meters& meters() const noexcept { return meters_; }
    [[nodiscard]] AnalyzerTap& analyzer() noexcept { return analyzer_; }

    // UI helpers (UI thread): combined TONE curve (EQ + filters) at frequency f, in dB.
    [[nodiscard]] float toneCurveDb(float hz) const noexcept;
    // Load a factory preset (defaults + overrides). Returns false if index is out of range.
    bool loadPreset(std::size_t index) noexcept;

    // Diagnostics for tests: which modules actually ran DSP in the last process() call.
    struct Activity { bool dyn = false, tone = false, space = false, sfx = false; };
    [[nodiscard]] Activity lastActivity() const noexcept { return activity_; }

private:
    void updateFromParams(bool force) noexcept;
    void processDynamics(float& l, float& r) noexcept;
    void processTone(float& l, float& r, bool filtersHere) noexcept;
    void processFilters(float& l, float& r) noexcept;
    void processSpace(float sendL, float sendR, float duckRef, float& retL, float& retR) noexcept;

    ParamSet<kParamCount> params_;
    std::array<float, kParamCount> cached_{};
    Meters meters_{};
    AnalyzerTap analyzer_{};
    float sr_ = 48000.0f;
    int maxBlock_ = 512;
    Activity activity_{};

    // Global
    Smoother inTrim_{}, outTrim_{}, bypassMix_{};
    AutoGain autoGain_{};
    BrickwallLimiter limiter_{};
    std::array<DelayLine, 2> dryDelay_{};

    // Module enable (after solo logic) and trims
    bool dynOn_ = true, toneOn_ = true, spaceOn_ = true, sfxOn_ = true, spaceSoloOnly_ = false;
    bool fltPost_ = false, spcPost_ = false, autoGainOn_ = true;
    Smoother dynTrim_{}, toneTrim_{}, spaceTrim_{}, sfxTrim_{};

    // Dynamics state
    const assumed::pv::DynModeTune* dyn_ = &assumed::pv::kDynModeTable[0];
    float gateThrDb_ = -96.0f, gateGain_ = 1.0f, gateHold_ = 0, gateAtt_ = 0, gateRel_ = 0;
    bool gateOpen_ = true;
    EnvFollower gateEnv_{};
    float compThrDb_ = 0, compRatio_ = 1, fetRatio_ = 1, upRange_ = 0, makeupDb_ = 0;
    float color_ = 1.27f, satDrive_ = 1.0f, satMix_ = 1.0f;
    EnvFollower envA_{}, envFet_{};
    float optoFast_ = 0, optoSlow_ = 0, lastGrA_ = 0;
    float deEssAmt_ = 0.3f, deEssRelThrDb_ = -12.0f, deEssMaxGr_ = 0;
    StereoBiquad deEssDetect_{}, deEssSplit_{};
    EnvFollower deEssBand_{}, deEssBroad_{};
    float grBlockMax_ = 0, dsBlockMax_ = 0;

    // Tone state
    const assumed::pv::ToneModeTune* tone_ = &assumed::pv::kToneModeTable[0];
    StereoBiquad eqLow_{}, eqMid_{}, eqHigh_{}, eqAir_{}, hpf_{}, lpf_{};
    bool eqLowActive_ = false, eqMidActive_ = false, eqHighActive_ = false, eqAirActive_ = false;

    // Space state
    FdnReverb reverb_{};
    TempoDelay delay_{};
    StereoBiquad sendHp_{}, retHp_{}, retLp_{};
    EnvFollower duckEnv_{};
    Smoother revMix_{}, dlyMix_{};
    float vrbDuck_ = 0.32f, dlyDuck_ = 0.17f;
    float revInPk_ = 0, revOutPk_ = 0, dlyInPk_ = 0, dlyOutPk_ = 0, revDuckDb_ = 0, dlyDuckDb_ = 0;

    // SFX
    SfxUnit sfx_{};
};

} // namespace dm::pv
