#pragma once
// =====================================================================================================
//  HEM SYNDICATE RACK - 5-column analog console / mix-bus strip, plus its 6 standalone split plug-ins.
//  Column order: SATURATE -> DYNAMICS (COMP + TRANSIENT) -> [TONE per ROUTING] -> MODULATE -> MAXIMIZE
//  TONE ROUTING: PRE (before SATURATE) | POST DYN (after DYNAMICS, default) | POST (after MODULATE).
//  Each column has POWER and SOLO (solo = only soloed columns run).
// =====================================================================================================
#include "dm/core/EngineShell.hpp"
#include "dm/syndicate/Modules.hpp"

namespace dm::sr {

inline constexpr const char* kRouteNames[3] = {"PRE", "POST DYN", "POST"};

namespace detail {
constexpr std::array<ParamDesc, 2> bay(std::string_view on, std::string_view solo, std::string_view label) {
    return {{{on, label, 0, 1, 1, "toggle"}, {solo, "S", 0, 1, 0, "toggle"}}};
}
inline constexpr std::array<ParamDesc, 1> kRoute = {{{"tone_route", "TONE ROUTING", 0, 2, 1, "", C(kRouteNames)}}};
} // namespace detail

// ---- Rack parameter layout --------------------------------------------------------------------------
inline constexpr std::size_t kSatOn   = G_Count;
inline constexpr std::size_t kSat     = kSatOn + 2;
inline constexpr std::size_t kDynOn   = kSat + SatModule::kN;
inline constexpr std::size_t kComp    = kDynOn + 2;
inline constexpr std::size_t kTrans   = kComp + CompModule::kN;
inline constexpr std::size_t kToneOn  = kTrans + TransientModule::kN;
inline constexpr std::size_t kTone    = kToneOn + 2;
inline constexpr std::size_t kRoute   = kTone + ToneModule::kN;
inline constexpr std::size_t kModOn   = kRoute + 1;
inline constexpr std::size_t kMod     = kModOn + 2;
inline constexpr std::size_t kMaxOn   = kMod + ModModule::kN;
inline constexpr std::size_t kMax     = kMaxOn + 2;
inline constexpr std::size_t kRackN   = kMax + MaxModule::kN;

inline constexpr auto kRackParams = concatAll(
    kGlobalParams,
    detail::bay("sat_on", "sat_solo", "SATURATE"), SatModule::kParams,
    detail::bay("dyn_on", "dyn_solo", "DYNAMICS"), CompModule::kParams, TransientModule::kParams,
    detail::bay("tone_on", "tone_solo", "TONE"), ToneModule::kParams, detail::kRoute,
    detail::bay("mod_on", "mod_solo", "MODULATE"), ModModule::kParams,
    detail::bay("max_on", "max_solo", "MAXIMIZE"), MaxModule::kParams);
static_assert(kRackParams.size() == kRackN);

std::span<const Preset> rackPresets() noexcept;

class SyndicateRack : public EngineShell<SyndicateRack, kRackN> {
public:
    SyndicateRack() : EngineShell(kRackParams) {}
    bool loadPreset(std::size_t i) noexcept {
        const auto p = rackPresets();
        return i < p.size() && loadPresetValues(p[i].values);
    }

    // Meters (UI thread).
    [[nodiscard]] float satMeterDb() const noexcept { return sat_.satMeterDb(); }
    [[nodiscard]] float grDb() const noexcept { return comp_.grDb(); }
    [[nodiscard]] float transientDb() const noexcept { return trans_.envelopeDb(); }
    [[nodiscard]] float correlation() const noexcept { return mod_.correlation(); }
    [[nodiscard]] float bandActivity(int b) const noexcept { return max_.bandActivity(b); }
    [[nodiscard]] AnalyzerTap& analyzer() noexcept { return tone_.analyzer(); }
    [[nodiscard]] float toneCurveDb(float hz) const noexcept {
        std::array<float, ToneModule::kN> v{};
        for (std::size_t i = 0; i < v.size(); ++i) v[i] = params_.get(kTone + i);
        return ToneModule::curveDb(v.data(), hz, sr_);
    }

    // EngineShell hooks
    void prepareCore(float sr);
    void resetCore() noexcept;
    void updateCore(const float* v, const float* old, bool force) noexcept;
    void processCore(float& l, float& r) noexcept;
    [[nodiscard]] int coreLatency() const noexcept { return 0; }
    [[nodiscard]] bool coreActive() const noexcept { return satOn_ || dynOn_ || toneOn_ || modOn_ || maxOn_; }
    void afterChunk(int) noexcept { sat_.endChunk(); comp_.endChunk(); mod_.endChunk(); max_.endChunk(); }

private:
    SatModule sat_{};
    CompModule comp_{};
    TransientModule trans_{};
    ToneModule tone_{};
    ModModule mod_{};
    MaxModule max_{};
    bool satOn_ = true, dynOn_ = true, toneOn_ = true, modOn_ = true, maxOn_ = true;
    int route_ = 1;
};

// ---- Standalone split plug-ins: globals + POWER + the module's own controls ---------------------------
template <class Module>
inline constexpr auto kSplitParams = concatParams(
    concatParams(kGlobalParams, std::array<ParamDesc, 1>{{{"on", "POWER", 0, 1, 1, "toggle"}}}), Module::kParams);

template <class Module>
class SplitPlugin : public EngineShell<SplitPlugin<Module>, G_Count + 1 + Module::kN> {
    using Base = EngineShell<SplitPlugin<Module>, G_Count + 1 + Module::kN>;
public:
    static constexpr std::size_t kOn = G_Count, kFirst = G_Count + 1;
    SplitPlugin() : Base(kSplitParams<Module>) {}
    Module& module() noexcept { return m_; }
    void prepareCore(float sr) { m_.prepare(sr); }
    void resetCore() noexcept { m_.reset(); }
    void updateCore(const float* v, const float* old, bool) noexcept {
        on_ = v[kOn] >= 0.5f;
        m_.update(v + kFirst, old ? old + kFirst : nullptr);
    }
    void processCore(float& l, float& r) noexcept { m_.process(l, r); }
    [[nodiscard]] int coreLatency() const noexcept { return 0; }
    [[nodiscard]] bool coreActive() const noexcept {
        if constexpr (requires(const Module& m) { m.active(); }) return on_ && m_.active();
        else return on_;
    }
    void afterChunk(int) noexcept { if constexpr (requires(Module& m) { m.endChunk(); }) m_.endChunk(); }
private:
    Module m_{};
    bool on_ = true;
};

using HemSat       = SplitPlugin<SatModule>;
using HemComp      = SplitPlugin<CompModule>;
using HemTransient = SplitPlugin<TransientModule>;
using HemTone      = SplitPlugin<ToneModule>;
using HemMod       = SplitPlugin<ModModule>;
using HemMax       = SplitPlugin<MaxModule>;

} // namespace dm::sr
