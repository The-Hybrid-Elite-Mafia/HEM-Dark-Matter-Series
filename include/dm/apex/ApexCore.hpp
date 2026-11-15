#pragma once
// =====================================================================================================
//  HEM APEX CORE - adaptive multi-engine master finalizer.
//  Flow: [MONO-SAFE low lock] -> 3-band split (engine crossovers)
//        -> per band: CONTAIN (pulls a band down when it pokes above the band average)
//                     BOOST   (upward lift of quiet detail)
//        -> sum -> COLOR PROFILE (LOW / HIGH harmonics) -> AMOUNT drive
//        -> PEAKS / CURVE transfer stage (ATTACK / RELEASE multipliers) -> WIDTH (M/S) -> PARALLEL mix
//  AMOUNT (0..200 %) scales CONTAIN, BOOST and the transfer stage; above 100 % it also adds drive.
// =====================================================================================================
#include "dm/core/Crossover.hpp"
#include "dm/core/EngineShell.hpp"
#include "dm/core/Saturation.hpp"

namespace dm::ax {

namespace a = dm::assumed::ax;
inline constexpr const char* kEngineNames[a::kEngines] = {
    a::kEngineTable[0].name, a::kEngineTable[1].name, a::kEngineTable[2].name,
    a::kEngineTable[3].name, a::kEngineTable[4].name, a::kEngineTable[5].name};

enum P : std::size_t {
    Engine = G_Count, Amount, Contain, Boost, Parallel, MonoSafe, Width, Peaks, Curve, Attack, Release, ColorLow, ColorHigh,
    kParamCount
};

inline constexpr std::array<ParamDesc, std::size_t(kParamCount) - std::size_t(G_Count)> kOwnParams = {{
    {"engine", "ENGINE", 0, a::kEngines - 1, 0, "", std::span<const char* const>(kEngineNames)},
    {"amount", "AMOUNT", 0, 200, a::kDefAmount, "%"},
    {"contain", "CONTAIN", 0, 100, a::kDefContain, "%"},
    {"boost", "BOOST", 0, 100, a::kDefBoost, "%"},
    {"parallel", "PARALLEL", 0, 100, 100, "%"},
    {"mono_safe", "MONO-SAFE", 0, 1, 0, "toggle"},
    {"width", "WIDTH", 0, 200, 100, "%"},
    {"peaks", "PEAKS", 0, 100, a::kDefPeaks, "%"},
    {"curve", "CURVE", 0, 100, a::kDefCurve, "%"},
    {"attack", "ATTACK", 25, 400, 100, "%"},
    {"release", "RELEASE", 25, 400, 100, "%"},
    {"color_low", "LOW", 0, 100, a::kDefColorLow, "%"},
    {"color_high", "HIGH", 0, 100, a::kDefColorHigh, "%"},
}};
inline constexpr auto kParams = concatAll(kGlobalParams, kOwnParams);

std::span<const Preset> apexPresets() noexcept;

class ApexCore : public EngineShell<ApexCore, kParamCount> {
public:
    ApexCore() : EngineShell(kParams) {}
    bool loadPreset(std::size_t i) noexcept { const auto p = apexPresets(); return i < p.size() && loadPresetValues(p[i].values); }

    // Meters
    [[nodiscard]] float grDb() const noexcept { return grPub_; }          // GR bay
    [[nodiscard]] float widthPercent() const noexcept { return widthPub_; } // measured side/mid energy re input
    // Display helpers (UI thread): transfer curve, envelope times, colour tilt.
    [[nodiscard]] float transferDb(float inDb) const noexcept;
    [[nodiscard]] float attackMs() const noexcept;
    [[nodiscard]] float releaseMs() const noexcept;

    void prepareCore(float sr);
    void resetCore() noexcept;
    void updateCore(const float* v, const float* old, bool force) noexcept;
    void processCore(float& l, float& r) noexcept;
    [[nodiscard]] int coreLatency() const noexcept { return 0; }
    [[nodiscard]] bool coreActive() const noexcept { return active_; }
    void afterChunk(int) noexcept;

private:
    const a::ApexEngineTune* e_ = &a::kEngineTable[0];
    bool active_ = true, monoSafe_ = false;
    float amt_ = 1.27f, contain_ = 0, boost_ = 0, parallel_ = 1, width_ = 1, drive_ = 1;
    float thr_ = 0, ratio_ = 1, knee_ = 0, colorLow_ = 0, colorHigh_ = 0;
    ThreeBandSplit split_{};
    Lr4Split mono_{};
    std::array<EnvFollower, 3> bandEnv_{};
    EnvFollower avgEnv_{}, peakEnv_{};
    StereoBiquad lowTap_{}, highTap_{}, air_{}, sideHp_{};
    std::array<DcBlocker, 2> dc_{};
    float grMax_ = 0, grPub_ = 0, widthPub_ = 100;
    double inSide_ = 0, inMid_ = 0, outSide_ = 0, outMid_ = 0;
};

} // namespace dm::ax
