// HEM SYNDICATE RACK + 6 split plug-ins - quality gates.
#include "Suite.hpp"

#include "dm/syndicate/SyndicateRack.hpp"

using namespace dmt;
using namespace dm::sr;

namespace {

SuiteCfg<SyndicateRack> rackCfg() {
    SuiteCfg<SyndicateRack> c;
    c.name = "HEM SYNDICATE RACK";
    c.vocal = false;
    c.make = [](float sr) { auto e = std::make_unique<SyndicateRack>(); e->prepare(sr, 512); return e; };
    c.allOff = [](SyndicateRack& e) { for (auto i : {kSatOn, kDynOn, kToneOn, kModOn, kMaxOn}) e.params().set(i, 0); };
    const std::size_t maxClip = kMax + MaxModule::Clip, maxMax = kMax + MaxModule::Max;
    c.overrides = {
        {kTrans + TransientModule::Shape, {{kTrans + TransientModule::Amount, 60}}},
        {kComp + CompModule::Sidechain, {{kComp + CompModule::Amount, 80}}},
        {maxClip, {{maxMax, 100}}},
        {kRoute, {{kTone + ToneModule::Mid, 8}, {kComp + CompModule::Amount, 80}}},
        {kTone + ToneModule::Slope, {{kTone + ToneModule::Hpf, 200}}},
        {kMod + ModModule::Depth, {{kMod + ModModule::Amount, 80}}},
        {kMod + ModModule::Drift, {{kMod + ModModule::Amount, 80}}},
        {kMax + MaxModule::XoverLow, {{kMax + MaxModule::Multi, 80}}},
        {kMax + MaxModule::XoverHigh, {{kMax + MaxModule::Multi, 80}}},
    };
    c.modes = {
        {kSat + SatModule::Mode, {{kSat + SatModule::Amount, 80}}, "SATURATE"},
        {kComp + CompModule::Mode, {{kComp + CompModule::Amount, 80}}, "DYNAMICS"},
        {kTone + ToneModule::Mode, {{kTone + ToneModule::Low, 6}, {kTone + ToneModule::HiMid, 6}, {kTone + ToneModule::Air, 6}}, "TONE"},
        {kMod + ModModule::Mode, {{kMod + ModModule::Amount, 80}}, "MODULATE"},
        {kMax + MaxModule::Mode, {{kMax + MaxModule::Multi, 80}}, "MAXIMIZE"},
        {maxClip, {{maxMax, 100}}, "CLIP"},
    };
    c.presetCount = rackPresets().size();
    c.loadPreset = [](SyndicateRack& e, std::size_t i) { e.loadPreset(i); };
    c.presetName = [](std::size_t i) { return rackPresets()[i].name.data(); };
    return c;
}

template <class Plugin>
SuiteCfg<Plugin> splitCfg(const char* name, std::vector<WiredOverride> ov, std::vector<ModeCheck> modes) {
    SuiteCfg<Plugin> c;
    c.name = name;
    c.vocal = false;
    c.make = [](float sr) { auto e = std::make_unique<Plugin>(); e->prepare(sr, 512); return e; };
    c.allOff = [](Plugin& e) { e.params().set(Plugin::kOn, 0); };
    c.overrides = std::move(ov);
    c.modes = std::move(modes);
    return c;
}

void testRackMeters() {
    section("SYNDICATE meters read real DSP state");
    SyndicateRack e;
    e.prepare(48000, 512);
    e.params().set(kComp + CompModule::Amount, 80);
    e.params().set(kSat + SatModule::Amount, 80);
    e.params().set(kMax + MaxModule::Multi, 80);
    e.reset();
    auto in = musicSignal(48000, 3.0, -6.0);
    float gr = 0, sat = -200, act = 0;
    for (std::size_t p = 0; p < in.size(); p += 256) {
        e.process(in.l.data() + p, in.r.data() + p, int(std::min<std::size_t>(256, in.size() - p)));
        gr = std::max(gr, e.grDb()); sat = std::max(sat, e.satMeterDb());
        for (int b = 0; b < 3; ++b) act = std::max(act, e.bandActivity(b));
    }
    check(gr > 2.0f, "GR meter at COMP 80% (dB)", gr, "dB");
    check(sat > -60.0f, "SAT meter at SATURATE 80% (harmonics re signal, dB)", sat, "dB");
    check(act > 5.0f, "MAXIMIZE band meters at MULTI 80% (%)", act, "%");
    const float c = e.correlation();
    check(c > -1.01f && c < 1.01f, "stereo phase meter in range", c);
    e.params().set(kTone + ToneModule::Air, 6);
    check(e.toneCurveDb(16000) > 4.0f, "TONE curve shows AIR+ +6 dB at 16 kHz (dB)", e.toneCurveDb(16000), "dB");
}

void testToneRouting() {
    section("TONE ROUTING moves the TONE column");
    // With heavy compression after a big EQ boost (PRE) vs before it (POST), level balance must differ.
    auto in = musicSignal(48000, 3.0, -6.0);
    auto run = [&](float route) {
        SyndicateRack e; e.prepare(48000, 512);
        e.params().set(dm::G_AutoGain, 0);
        e.params().set(kTone + ToneModule::Low, 10);
        e.params().set(kComp + CompModule::Amount, 90);
        e.params().set(kRoute, route);
        e.reset();
        return render(e, in);
    };
    const auto pre = run(0), post = run(2);
    check(diffRms(pre, post) > 1e-3, "PRE vs POST outputs differ (diff RMS dBFS)", db(diffRms(pre, post)), "dBFS");
}

} // namespace

void runSyndicateTests() {
    runSuite(rackCfg());
    testRackMeters();
    testToneRouting();

    runSuite(splitCfg<HemSat>("HEM SAT (split)", {},
        {{HemSat::kFirst + SatModule::Mode, {{HemSat::kFirst + SatModule::Amount, 80}}, "SATURATE"}}));
    runSuite(splitCfg<HemComp>("HEM COMP (split)",
        {{HemComp::kFirst + CompModule::Sidechain, {{HemComp::kFirst + CompModule::Amount, 80}}}},
        {{HemComp::kFirst + CompModule::Mode, {{HemComp::kFirst + CompModule::Amount, 80}}, "DYNAMICS"}}));
    {
        auto c = splitCfg<HemTransient>("HEM TRANSIENT (split)",
            {{HemTransient::kOn, {{HemTransient::kFirst + TransientModule::Amount, 60}}},
             {HemTransient::kFirst + TransientModule::Shape, {{HemTransient::kFirst + TransientModule::Amount, 60}}}}, {});
        c.activeCtx = {{HemTransient::kFirst + TransientModule::Amount, 60}}; // default 0% = neutral (does nothing, by design)
        runSuite(c);
    }
    runSuite(splitCfg<HemTone>("HEM TONE (split)",
        {{HemTone::kFirst + ToneModule::Slope, {{HemTone::kFirst + ToneModule::Hpf, 200}}}},
        {{HemTone::kFirst + ToneModule::Mode, {{HemTone::kFirst + ToneModule::Low, 6}, {HemTone::kFirst + ToneModule::Air, 6}}, "TONE"}}));
    runSuite(splitCfg<HemMod>("HEM MOD (split)",
        {{HemMod::kFirst + ModModule::Depth, {{HemMod::kFirst + ModModule::Amount, 80}}},
         {HemMod::kFirst + ModModule::Drift, {{HemMod::kFirst + ModModule::Amount, 80}}}},
        {{HemMod::kFirst + ModModule::Mode, {{HemMod::kFirst + ModModule::Amount, 80}}, "MODULATE"}}));
    runSuite(splitCfg<HemMax>("HEM MAX (split)",
        {{HemMax::kFirst + MaxModule::Clip, {{HemMax::kFirst + MaxModule::Max, 100}}},
         {HemMax::kFirst + MaxModule::XoverLow, {{HemMax::kFirst + MaxModule::Multi, 80}}},
         {HemMax::kFirst + MaxModule::XoverHigh, {{HemMax::kFirst + MaxModule::Multi, 80}}}},
        {{HemMax::kFirst + MaxModule::Mode, {{HemMax::kFirst + MaxModule::Multi, 80}}, "MAXIMIZE"}}));
}
