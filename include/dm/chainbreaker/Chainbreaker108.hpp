#pragma once
// =====================================================================================================
//  HEM CHAINBREAKER 108 - modular multi-FX rack.
//  4 slots in series; each slot's < > switcher picks STUTTER, MULTI-CLIP, GRAIN ENGINE or SPECTRO EQ
//  (dragging a module to another bay = swapping slot types). Every slot keeps its own DSP state for every
//  module type, so two slots can run the same module. Module controls are shared per module type.
//  Master deck: FLUX / DRIVE / TONE / MIX macros, BROADCAST EQ + 5-band graphic trims, MONITORING
//  (CROSSFEED, WIDTH, OUT LEVEL) and UTILITY ROUTING (PRE = master EQ before the slots, no clip;
//  SOFT / HARD / MST = master EQ after the slots + final clipper of that type).
//  Quick-Load Chain Slots 1-8: loadChain(i) recalls a stored chain (slot types, powers, module settings).
// =====================================================================================================
#include "dm/core/Crossover.hpp"
#include "dm/core/EngineShell.hpp"
#include "dm/core/Grain.hpp"
#include "dm/core/Reverb.hpp"

#include <vector>

namespace dm::cb {

namespace a = dm::assumed::cb;
inline constexpr const char* kTypeNames[4] = {"STUTTER", "MULTI-CLIP", "GRAIN ENGINE", "SPECTRO EQ"};
inline constexpr const char* kRouteNames[4] = {"PRE", "SOFT", "HARD", "MST"};

enum P : std::size_t {
    S1Type = G_Count, S2Type, S3Type, S4Type, S1On, S2On, S3On, S4On,
    StutX, StutY, Flux,
    ClipLow, ClipHigh, Drive, ClipShape,
    GrainPitch, GrainReverse, Wide, GrainMix,
    Eq1F, Eq1G, Eq1Q, Eq2F, Eq2G, Eq2Q, Eq3F, Eq3G, Eq3Q, Eq4F, Eq4G, Eq4Q,
    MFlux, MDrive, MTone, MMix,
    BcLow, BcMid, BcHigh,
    GSub, GLow, GMid, GHiMid, GHigh,
    Crossfeed, Width, OutLevel, UtilRoute, Tempo,
    kParamCount
};

using C = std::span<const char* const>;
inline constexpr std::array<ParamDesc, std::size_t(kParamCount) - std::size_t(G_Count)> kOwnParams = {{
    {"s1_type", "SLOT 1", 0, 3, 0, "", C(kTypeNames)}, {"s2_type", "SLOT 2", 0, 3, 1, "", C(kTypeNames)},
    {"s3_type", "SLOT 3", 0, 3, 2, "", C(kTypeNames)}, {"s4_type", "SLOT 4", 0, 3, 3, "", C(kTypeNames)},
    {"s1_on", "SLOT 1 POWER", 0, 1, 1, "toggle"}, {"s2_on", "SLOT 2 POWER", 0, 1, 1, "toggle"},
    {"s3_on", "SLOT 3 POWER", 0, 1, 1, "toggle"}, {"s4_on", "SLOT 4 POWER", 0, 1, 1, "toggle"},
    {"stut_x", "XY PAD X (RATE)", 0, 100, a::kDefStutX, "%"},
    {"stut_y", "XY PAD Y (GATE)", 0, 100, a::kDefStutY, "%"},
    {"flux", "FLUX", 0, 100, a::kDefFlux, "%"},
    {"clip_low", "LOW CROSSOVER", 60, 600, a::kDefClipLow, "Hz"},
    {"clip_high", "HIGH CROSSOVER", 1500, 10000, a::kDefClipHigh, "Hz"},
    {"drive", "DRIVE", 0, 100, a::kDefDrive, "%"},
    {"clip_shape", "SHAPE", 0, 100, 50, "%"},
    {"grain_pitch", "PITCH", -12, 12, 0, "st"},
    {"grain_reverse", "REVERSE", 0, 1, 0, "toggle"},
    {"wide", "WIDE", 0, 100, 50, "%"},
    {"grain_mix", "MIX", 0, 100, a::kDefGrainMix, "%"},
    {"eq1_freq", "EQ 1 FREQ", 20, 20000, a::kEqDefHz[0], "Hz"}, {"eq1_gain", "EQ 1 GAIN", -18, 18, 0, "dB"}, {"eq1_q", "EQ 1 Q", 0.1f, 10, 0.7f, ""},
    {"eq2_freq", "EQ 2 FREQ", 20, 20000, a::kEqDefHz[1], "Hz"}, {"eq2_gain", "EQ 2 GAIN", -18, 18, 0, "dB"}, {"eq2_q", "EQ 2 Q", 0.1f, 10, 1.0f, ""},
    {"eq3_freq", "EQ 3 FREQ", 20, 20000, a::kEqDefHz[2], "Hz"}, {"eq3_gain", "EQ 3 GAIN", -18, 18, 0, "dB"}, {"eq3_q", "EQ 3 Q", 0.1f, 10, 1.0f, ""},
    {"eq4_freq", "EQ 4 FREQ", 20, 20000, a::kEqDefHz[3], "Hz"}, {"eq4_gain", "EQ 4 GAIN", -18, 18, 0, "dB"}, {"eq4_q", "EQ 4 Q", 0.1f, 10, 0.7f, ""},
    {"m_flux", "FLUX (MASTER)", 0, 100, 0, "%"},
    {"m_drive", "DRIVE (MASTER)", 0, 100, 0, "%"},
    {"m_tone", "TONE", -100, 100, 0, "%"},
    {"m_mix", "MIX", 0, 100, 100, "%"},
    {"bc_low", "BROADCAST LOW", -12, 12, 0, "dB"}, {"bc_mid", "BROADCAST MID", -12, 12, 0, "dB"},
    {"bc_high", "BROADCAST HIGH", -12, 12, 0, "dB"},
    {"g_sub", "SUB", -6, 6, 0, "dB"}, {"g_low", "LOW", -6, 6, 0, "dB"}, {"g_mid", "MID", -6, 6, 0, "dB"},
    {"g_himid", "HI MID", -6, 6, 0, "dB"}, {"g_high", "HIGH", -6, 6, 0, "dB"},
    {"crossfeed", "CROSSFEED", 0, 100, 0, "%"},
    {"width", "WIDTH", 0, 200, 100, "%"},
    {"out_level", "OUT LEVEL", -12, 12, 0, "dB"},
    {"util_route", "UTILITY ROUTING", 0, 3, 1, "", C(kRouteNames)},
    {"tempo", "TEMPO", 40, 240, assumed::kHostTempoDefault, "BPM"},
}};
inline constexpr auto kParams = concatAll(kGlobalParams, kOwnParams);

std::span<const Preset> chainbreakerPresets() noexcept;
std::span<const Preset> quickLoadChains() noexcept; // the 8 Quick-Load Chain Slots

// Shared per-type settings, filled from the parameter table once per chunk.
struct Settings {
    float stutDivFrac = 0.125f, stutDuty = 0.6f, flux = 0.3f, bpm = 120;
    float drive = 1, shape = 0.5f, clipLow = 200, clipHigh = 3000;
    float grainRatio = 1, wide = 0.5f, grainMix = 0.25f; bool reverse = false;
    std::array<BiquadCoefs, 4> eq{};
    std::array<bool, 4> eqOn{};
};

class StutterUnit {
public:
    void prepare(float sr, std::uint32_t seed);
    void reset() noexcept;
    void process(float& l, float& r, const Settings& s) noexcept;
    [[nodiscard]] float rateHz() const noexcept { return rateHz_; }
    [[nodiscard]] bool stuttering() const noexcept { return on_; }
private:
    float sr_ = 48000, rateHz_ = 0;
    std::vector<float> bufL_, bufR_;
    std::int64_t t_ = 0, beatStart_ = 0, sliceLen_ = 1;
    double beatPos_ = 0;
    bool on_ = false;
    std::uint32_t seed_ = 1u;
    Noise rng_{};
};

class ClipUnit {
public:
    void prepare(float sr);
    void reset() noexcept;
    void process(float& l, float& r, const Settings& s) noexcept;
    [[nodiscard]] float clipPercent(int b) const noexcept { return pub_[static_cast<std::size_t>(b)]; }
    void endChunk() noexcept;
private:
    float sr_ = 48000, lastLow_ = -1, lastHigh_ = -1;
    ThreeBandSplit split_{};
    std::array<int, 3> clipped_{};
    int count_ = 0;
    std::array<float, 3> pub_{};
};

class GrainUnit {
public:
    void prepare(float sr);
    void reset() noexcept;
    void process(float& l, float& r, const Settings& s) noexcept;
    [[nodiscard]] float energyDb() const noexcept { return energyDb_; }
private:
    GrainCloud<4> cl_{}, cr_{};
    float energyDb_ = -120;
};

class EqUnit {
public:
    void reset() noexcept { for (auto& b : b_) b.reset(); }
    void process(float& l, float& r, const Settings& s) noexcept;
private:
    std::array<StereoBiquad, 4> b_{};
};

class Chainbreaker108 : public EngineShell<Chainbreaker108, kParamCount> {
public:
    Chainbreaker108() : EngineShell(kParams) {}
    bool loadPreset(std::size_t i) noexcept { const auto p = chainbreakerPresets(); return i < p.size() && loadPresetValues(p[i].values); }
    // Quick-Load Chain Slot 1..8 (index 0..7): recalls only the chain (slots + module controls), keeps the
    // master deck. UI thread.
    bool loadChain(std::size_t i) noexcept;

    // Readouts
    [[nodiscard]] float stutterRateHz() const noexcept { return stutHz_.load(std::memory_order_relaxed); }
    [[nodiscard]] float clipPercent(int band) const noexcept { return clipPub_[static_cast<std::size_t>(band)].load(std::memory_order_relaxed); }
    [[nodiscard]] float spectroCurveDb(float hz) const noexcept; // SPECTRO EQ graph

    void prepareCore(float sr);
    void resetCore() noexcept;
    void updateCore(const float* v, const float* old, bool force) noexcept;
    void processCore(float& l, float& r) noexcept;
    [[nodiscard]] int coreLatency() const noexcept { return 0; }
    [[nodiscard]] bool coreActive() const noexcept { return mix_ > 0.0f; }
    void afterChunk(int) noexcept;

private:
    void master(float& l, float& r) noexcept;
    void masterEq(float& l, float& r) noexcept;

    Settings s_{};
    std::array<int, 4> type_{0, 1, 2, 3};
    std::array<bool, 4> on_{true, true, true, true};
    std::array<StutterUnit, 4> stut_{};
    std::array<ClipUnit, 4> clip_{};
    std::array<GrainUnit, 4> grain_{};
    std::array<EqUnit, 4> eq_{};
    std::array<StereoBiquad, 3> bc_{};
    std::array<StereoBiquad, 5> gr_{};
    StereoBiquad tiltLo_{}, tiltHi_{};
    std::array<OnePole, 2> xf_{};
    bool masterEqOn_ = false, tiltOn_ = false;
    float mix_ = 1, crossfeed_ = 0, width_ = 1, outLevel_ = 1, masterDrive_ = 1;
    int route_ = 1;
    std::atomic<float> stutHz_{0};
    std::array<std::atomic<float>, 3> clipPub_{};
    std::array<float, 3> clipHold_{};   // L / M / H meter after hold and release
};

} // namespace dm::cb
