#pragma once
// =====================================================================================================
//  HEM OUTLAW RIDER - real-time vocal gain rider.
//  The vocal's RMS level (window set by SPEED) is compared with TARGET; the rider moves gain toward it,
//  limited to +MAX boost / -MIN cut (both scaled by RANGE). Below SENSITIVITY (noise floor) it holds.
//  The TRUEGAIN trace (gain in dB) plus input/output peaks are published per 256 samples for the
//  scrolling viewport. OUTPUT is the rider's own post-ride gain (the bottom bar keeps its global trim).
//  Auto-gain here uses a long 1.5 s window: overall loudness is kept, phrase-to-phrase riding remains.
// =====================================================================================================
#include "dm/core/EngineShell.hpp"

#include <vector>

namespace dm::rd {

namespace a = dm::assumed::rd;

enum P : std::size_t { Target = G_Count, MaxBoost, MinCut, Range, Speed, Sensitivity, RiderOut, kParamCount };

inline constexpr std::array<ParamDesc, std::size_t(kParamCount) - std::size_t(G_Count)> kOwnParams = {{
    {"target", "TARGET", -40, 0, a::kDefTarget, "dB"},
    {"max_boost", "MAX", 0, 24, a::kDefMaxBoost, "dB"},
    {"min_cut", "MIN", 0, 24, a::kDefMinCut, "dB"},
    {"range", "RANGE", 0, 100, 100, "%"},
    {"speed", "SPEED", 0, 100, 50, "%"},
    {"sensitivity", "SENSITIVITY", -80, -20, a::kDefSensitivity, "dB"},
    {"rider_out", "OUTPUT", -12, 12, 0, "dB"},
}};
inline constexpr auto kParams = concatAll(kGlobalParams, kOwnParams);

std::span<const Preset> riderPresets() noexcept;

// One column of the scrolling TRUEGAIN viewport.
struct RideFrame { float inDb, outDb, gainDb; };

class OutlawRider : public EngineShell<OutlawRider, kParamCount> {
public:
    OutlawRider() : EngineShell(kParams) {}
    bool loadPreset(std::size_t i) noexcept { const auto p = riderPresets(); return i < p.size() && loadPresetValues(p[i].values); }

    [[nodiscard]] float gainDb() const noexcept { return gainPub_.load(std::memory_order_relaxed); }
    // UI thread: copy out up to `max` newest frames (oldest first). Returns count.
    std::size_t readFrames(RideFrame* dst, std::size_t max) const noexcept;

    void prepareCore(float sr);
    void resetCore() noexcept;
    void updateCore(const float* v, const float* old, bool force) noexcept;
    void processCore(float& l, float& r) noexcept;
    [[nodiscard]] int coreLatency() const noexcept { return 0; }
    [[nodiscard]] bool coreActive() const noexcept { return range_ > 0.0f || out_ != 1.0f; }
    [[nodiscard]] bool coreClean(const float* v) const noexcept {   // nothing to ride: no boost and no cut (or RANGE 0)
        return (v[Range] <= 0.0f || (v[MaxBoost] <= 0.0f && v[MinCut] <= 0.0f)) && v[RiderOut] == 0.0f;
    }
    [[nodiscard]] std::span<const PresetValue> presetBase() const noexcept {   // the old defaults the presets were written over
        static constexpr PresetValue kBase[] = {{"max_boost", 6}, {"min_cut", 12}};
        return kBase;
    }
    [[nodiscard]] float autoGainWindowMs() const noexcept { return a::kAutoGainWindowMs; }

private:
    static constexpr std::size_t kFrames = 1024;
    float target_ = -18, maxBoost_ = 6, minCut_ = 12, range_ = 1, sens_ = -50, out_ = 1;
    float powCoef_ = 0, pow_ = 0, desiredDb_ = 0;
    Smoother gain_{};
    std::vector<RideFrame> frames_;
    std::atomic<std::size_t> frameWrite_{0};
    std::atomic<float> gainPub_{0};
    int hop_ = 0;
    float inPk_ = 0, outPk_ = 0;
};

} // namespace dm::rd
