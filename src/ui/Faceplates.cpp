// HEM Dark Matter Series - per-plug-in bindings (meters + display feeds).
#include "dm/ui/Faceplates.hpp"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace dm::ui {

namespace {
float db(float lin) { return 20.0f * std::log10(std::max(lin, 1.0e-6f)); }

// Spectrum (dBFS) + curve (dB) sampled at K log-spaced points 20 Hz .. 22 kHz into out[0..K) and out[K..2K).
template <class CurveFn>
void spectrumAndCurve(dm::AnalyzerTap& tap, CurveFn curve, std::vector<float>& out) {
    const std::size_t K = out.size() / 2;
    const auto& mag = tap.compute();
    const float binHz = tap.binHz(1);
    for (std::size_t i = 0; i < K; ++i) {
        const float f0 = 20.0f * std::pow(1100.0f, static_cast<float>(i) / static_cast<float>(K));
        const float f1 = 20.0f * std::pow(1100.0f, static_cast<float>(i + 1) / static_cast<float>(K));
        auto b0 = static_cast<std::size_t>(f0 / binHz), b1 = static_cast<std::size_t>(f1 / binHz);
        b1 = std::min(std::max(b1, b0 + 1), mag.size());
        float m = -140.0f;
        for (std::size_t k = std::min(b0, mag.size() - 1); k < b1; ++k) m = std::max(m, mag[k]);
        out[i] = m;
        out[K + i] = curve(std::sqrt(f0 * f1));
    }
}

// Bottom-bar + column level meters every EngineShell engine has.
template <class E>
float shellMeter(const E& e, int s) {
    const auto& m = e.shellMeters();
    switch (s) {
    case SrInL: return db(m.inL.read());
    case SrInR: return db(m.inR.read());
    case SrOutL: return db(m.outL.read());
    case SrOutR: return db(m.outR.read());
    case SrIn: return db(std::max(m.inL.read(), m.inR.read()));
    case SrOut: return db(std::max(m.outL.read(), m.outR.read()));
    case SrLimiter: return -std::fabs(m.limiterDb.read());
    default: return -120.0f;
    }
}
} // namespace

std::vector<int> shellHidden() { return {static_cast<int>(dm::G_AutoGain)}; }

std::unique_ptr<PanelBinding> bindSyndicate(dm::sr::SyndicateRack& e) {
    auto meter = [](dm::sr::SyndicateRack& v, int s) -> float {
        switch (s) {
        case SrSat: return v.satMeterDb();
        case SrGr: return -std::fabs(v.grDb());
        case SrTransient: return v.transientDb();
        case SrCorr: return v.correlation();
        case SrBandL: return v.bandActivity(0);
        case SrBandM: return v.bandActivity(1);
        case SrBandH: return v.bandActivity(2);
        default: return shellMeter(v, s);
        }
    };
    auto feed = [](dm::sr::SyndicateRack& v, DisplayKind k, int src, std::vector<float>& out) {
        if (k == DisplayKind::Analyzer) spectrumAndCurve(v.analyzer(), [&](float hz) { return v.toneCurveDb(hz); }, out);
        else if (k == DisplayKind::Handles && !out.empty()) out[0] = v.toneBandHz(src);
    };
    return std::make_unique<EngineBinding<dm::sr::SyndicateRack>>(e, dm::sr::rackPresets(), meter, feed);
}

template <class M>
std::unique_ptr<PanelBinding> bindSplit(dm::sr::SplitPlugin<M>& e) {
    using E = dm::sr::SplitPlugin<M>;
    constexpr std::size_t first = E::kFirst;
    auto meter = [](E& v, int s) -> float {
        M& m = v.module();
        if constexpr (std::is_same_v<M, dm::sr::SatModule>) { if (s == SrSat) return m.satMeterDb(); }
        if constexpr (std::is_same_v<M, dm::sr::CompModule>) { if (s == SrGr) return -std::fabs(m.grDb()); }
        if constexpr (std::is_same_v<M, dm::sr::TransientModule>) { if (s == SrTransient) return m.envelopeDb(); }
        if constexpr (std::is_same_v<M, dm::sr::ModModule>) { if (s == SrCorr) return m.correlation(); }
        if constexpr (std::is_same_v<M, dm::sr::MaxModule>) { if (s >= SrBandL && s <= SrBandH) return m.bandActivity(s - SrBandL); }
        return shellMeter(v, s);
    };
    auto feed = [](E& v, DisplayKind k, int src, std::vector<float>& out) {
        if constexpr (std::is_same_v<M, dm::sr::ToneModule>) {
            std::array<float, M::kN> vals{};
            for (std::size_t i = 0; i < vals.size(); ++i) vals[i] = v.params().get(first + i);
            if (k == DisplayKind::Analyzer)
                spectrumAndCurve(v.module().analyzer(), [&](float hz) { return M::curveDb(vals.data(), hz, v.sampleRate()); }, out);
            else if (k == DisplayKind::Handles && !out.empty()) out[0] = M::bandHz(vals.data(), src);
        } else {
            (void)v; (void)k; (void)src; (void)out;
        }
    };
    return std::make_unique<EngineBinding<E>>(e, dm::sr::rackPresets(), meter, feed);
}
template std::unique_ptr<PanelBinding> bindSplit(dm::sr::HemSat&);
template std::unique_ptr<PanelBinding> bindSplit(dm::sr::HemComp&);
template std::unique_ptr<PanelBinding> bindSplit(dm::sr::HemTransient&);
template std::unique_ptr<PanelBinding> bindSplit(dm::sr::HemTone&);
template std::unique_ptr<PanelBinding> bindSplit(dm::sr::HemMod&);
template std::unique_ptr<PanelBinding> bindSplit(dm::sr::HemMax&);

std::vector<int> phantomVoxHidden() { return {dm::pv::AutoGainOn, dm::pv::Tempo}; }

std::unique_ptr<PanelBinding> bindPhantomVox(dm::pv::PhantomVox& e) {
    auto meter = [](dm::pv::PhantomVox& v, int s) -> float {
        const auto& m = v.meters();
        switch (s) {
        case PvInL: return db(m.inL.read());
        case PvInR: return db(m.inR.read());
        case PvOutL: return db(m.outL.read());
        case PvOutR: return db(m.outR.read());
        case PvGr: return -m.gr.read();
        case PvDs: return -m.ds.read();
        case PvRevIn: return db(m.revIn.read());
        case PvRevDuck: return -m.revDuck.read();
        case PvRevOut: return db(m.revOut.read());
        case PvDlyIn: return db(m.dlyIn.read());
        case PvDlyDuck: return -m.dlyDuck.read();
        case PvDlyOut: return db(m.dlyOut.read());
        default: return -120.0f;
        }
    };
    auto feed = [](dm::pv::PhantomVox& v, DisplayKind k, int, std::vector<float>& out) {
        if (k != DisplayKind::Analyzer) return;
        spectrumAndCurve(v.analyzer(), [&](float hz) { return v.toneCurveDb(hz); }, out);
    };
    return std::make_unique<EngineBinding<dm::pv::PhantomVox>>(e, dm::pv::factoryPresets(), meter, feed);
}

} // namespace dm::ui
