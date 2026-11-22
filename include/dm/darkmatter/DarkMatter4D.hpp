#pragma once
// =====================================================================================================
//  HEM DARK MATTER 4D - hybrid reverb + delay suite.
//  REVERB: 8-line FDN, DECAY 0.2-75 s, DEPTH places the source front-to-back (room size, early level,
//          distance darkening), PRE-DELAY, LOW / HIGH COLOR return shelves.
//  DELAY:  tempo-synced ping-pong, FEEDBACK; 400% MULTIPLIER runs the repeats through a 4-grain cloud.
//  ROUTING: REV > DLY (delay is fed the reverb too) | PARALLEL | DLY > REV (reverb is fed the delay too).
//  DUCKING: both returns duck under the dry signal (left/right detected separately); SHAPE sets the
//           release time and curve.
// =====================================================================================================
#include "dm/core/EngineShell.hpp"
#include "dm/core/Grain.hpp"
#include "dm/core/Reverb.hpp"

namespace dm::d4 {

namespace a = dm::assumed::d4;
inline constexpr const char* kRouteNames[3] = {"REV > DLY", "PARALLEL", "DLY > REV"};

enum P : std::size_t {
    Reverb = G_Count, Decay, Depth, Predelay, LowColor, HighColor, Routing, Ducking, Shape, ExtKey, Delay, Time, Feedback,
    Multiplier, Tempo, kParamCount
};

inline constexpr std::array<ParamDesc, std::size_t(kParamCount) - std::size_t(G_Count)> kOwnParams = {{
    {"reverb", "REVERB", 0, 100, a::kDefReverb, "%"},
    {"decay", "DECAY", 0.2f, 75, a::kDefDecay, "s"},
    {"depth", "DEPTH", 0, 100, a::kDefDepth, "%"},
    {"predelay", "PRE-DELAY", 0, 500, a::kDefPredelay, "ms"},
    {"low_color", "LOW COLOR", -100, 100, 0, "%"},
    {"high_color", "HIGH COLOR", -100, 100, 0, "%"},
    {"routing", "ROUTING", 0, 2, 1, "", std::span<const char* const>(kRouteNames)},
    {"ducking", "DUCKING", 0, 100, a::kDefDucking, "%"},
    {"shape", "SHAPE", 0, 100, a::kDefShape, "%"},
    {"ext_key", "EXT KEY", 0, 1, 0, "toggle"},   // ducking follows the plug-in's sidechain input
    {"delay", "DELAY", 0, 100, a::kDefDelay, "%"},
    {"time", "TIME", 0, kSyncCount - 1, a::kDefTime, "", std::span<const char* const>(kSyncNames)},
    {"feedback", "FEEDBACK", 0, 95, a::kDefFeedback, "%"},
    {"multiplier", "400% MULTIPLIER", 0, 1, 0, "toggle"},
    {"tempo", "TEMPO", 40, 240, assumed::kHostTempoDefault, "BPM"},
}};
inline constexpr auto kParams = concatAll(kGlobalParams, kOwnParams);

std::span<const Preset> darkMatterPresets() noexcept;

class DarkMatter4D : public EngineShell<DarkMatter4D, kParamCount> {
public:
    DarkMatter4D() : EngineShell(kParams) {}
    bool loadPreset(std::size_t i) noexcept { const auto p = darkMatterPresets(); return i < p.size() && loadPresetValues(p[i].values); }

    // Meters / visualizers (UI thread)
    [[nodiscard]] float duckDb(int ch) const noexcept { return duckPub_[static_cast<std::size_t>(ch)].load(std::memory_order_relaxed); }
    [[nodiscard]] float reverbEnergyDb() const noexcept { return revPub_.load(std::memory_order_relaxed); }
    [[nodiscard]] float delayTapDb() const noexcept { return dlyPub_.load(std::memory_order_relaxed); }
    // SHAPE curve display: duck gain (0..1) t ms after the dry signal stops, at full ducking.
    [[nodiscard]] float shapeCurve(float tMs) const noexcept;

    void prepareCore(float sr);
    void resetCore() noexcept;
    void updateCore(const float* v, const float* old, bool force) noexcept;
    void processCore(float& l, float& r) noexcept;
    [[nodiscard]] int coreLatency() const noexcept { return 0; }
    [[nodiscard]] bool coreActive() const noexcept { return revMix_.target > 0.0f || dlyMix_.target > 0.0f || revMix_.value > 0.0f || dlyMix_.value > 0.0f; }
    void afterChunk(int) noexcept;

private:
    FdnReverb rev_{};
    TempoDelay dly_{};
    GrainCloud<4> cloudL_{}, cloudR_{};
    StereoBiquad lowSh_{}, highSh_{};
    std::array<EnvFollower, 2> duckEnv_{};
    Smoother revMix_{}, dlyMix_{};
    int route_ = 1;
    bool multiplier_ = false;
    float ducking_ = 0, shapeExp_ = 1;
    bool extKey_ = false;
    float revFbL_ = 0, revFbR_ = 0, dlyFbL_ = 0, dlyFbR_ = 0;
    float duckMax_[2] = {0, 0}, revPk_ = 0, dlyPk_ = 0;
    std::array<std::atomic<float>, 2> duckPub_{};
    std::atomic<float> revPub_{-120}, dlyPub_{-120};
};

} // namespace dm::d4
