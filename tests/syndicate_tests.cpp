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
        {kComp + CompModule::ExtKey, {{kComp + CompModule::Amount, 80}}},
        {maxClip, {{maxMax, 100}}},
        {kRoute, {{kTone + ToneModule::Mid, 8}, {kComp + CompModule::Amount, 80}}},
        {kTone + ToneModule::Slope, {{kTone + ToneModule::Hpf, 200}}},
        {kMod + ModModule::Depth, {{kMod + ModModule::Amount, 80}}},
        {kMod + ModModule::Drift, {{kMod + ModModule::Amount, 80}}},
        {kMax + MaxModule::XoverLow, {{kMax + MaxModule::Multi, 80}}},
        {kMax + MaxModule::XoverHigh, {{kMax + MaxModule::Multi, 80}}},
    };
    for (std::size_t b = 0; b < 6; ++b) // tone handles need a boosted band to move
        c.overrides.push_back({kTone + ToneModule::SubF + b, {{kTone + ToneModule::Sub + b, 8}}});
    for (std::size_t b = 0; b < 3; ++b) // amount per band needs MULTI working
        c.overrides.push_back({kMax + MaxModule::BandLow + b, {{kMax + MaxModule::Multi, 80}}});
    c.modes = {
        {kTone + ToneModule::Slope, {{kTone + ToneModule::Hpf, 300}, {kTone + ToneModule::Lpf, 4000}}, "FILTER SLOPE"},
        {kSat + SatModule::Mode, {{kSat + SatModule::Amount, 80}}, "SATURATE"},
        {kComp + CompModule::Mode, {{kComp + CompModule::Amount, 80}}, "DYNAMICS"},
        {kTone + ToneModule::Mode, {{kTone + ToneModule::Low, 6}, {kTone + ToneModule::HiMid, 6}, {kTone + ToneModule::Air, 6}}, "TONE"},
        {kMod + ModModule::Mode, {{kMod + ModModule::Amount, 80}}, "MODULATE"},
        {kMax + MaxModule::Mode, {{kMax + MaxModule::Multi, 80}}, "MAXIMIZE"},
        {maxClip, {{maxMax, 100}}, "CLIP"},
    };
    c.keySignal = pulseKey;
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

void testOriginalFeatures() {
    section("SYNDICATE: zero latency, MAX limiter, amount per band, tone handles, AIR+ exciter, slopes");
    SyndicateRack e; e.prepare(48000, 512);
    check(e.latencySamples() == 0, "rack latency (samples)", e.latencySamples());
    HemMax hm; hm.prepare(48000, 512);
    check(hm.latencySamples() == 0, "HEM MAX latency (samples)", hm.latencySamples());
    // MAX limiter holds peaks at the MAX threshold even with +12 dB push.
    {
        HemMax m; m.prepare(48000, 512);
        m.params().set(dm::G_AutoGain, 0);
        m.params().set(HemMax::kFirst + MaxModule::Max, 100);
        m.params().set(HemMax::kFirst + MaxModule::Clip, 0); // clipper OFF: only the limiter holds the peaks
        m.reset();
        auto out = musicSignal(48000, 3.0, -6.0);
        float gr = 0;
        for (std::size_t p = 0; p < out.size(); p += 256) {
            m.process(out.l.data() + p, out.r.data() + p, int(std::min<std::size_t>(256, out.size() - p)));
            gr = std::max(gr, m.module().limiterGrDb());
        }
        const double pk = std::max(peak(out.l), peak(out.r));
        check(pk <= 0.99, "MAX 100% (+12 dB push), clip OFF: peak held by the limiter (dBFS)", db(pk), "dBFS");
        check(gr > 3.0f, "MAX limiter gain reduction readout (dB)", gr, "dB");
    }
    // Tone handle moves the band: a +8 dB MID boost shifted +100% (one octave up) peaks an octave higher.
    {
        std::array<float, ToneModule::kN> v{};
        for (std::size_t i = 0; i < v.size(); ++i) v[i] = ToneModule::kParams[i].def;
        v[ToneModule::Mode] = 0; v[ToneModule::Mid] = 8;
        const float at800 = ToneModule::curveDb(v.data(), 800, 48000), at1600 = ToneModule::curveDb(v.data(), 1600, 48000);
        v[ToneModule::MidF] = 100;
        const float moved800 = ToneModule::curveDb(v.data(), 800, 48000), moved1600 = ToneModule::curveDb(v.data(), 1600, 48000);
        check(at800 > at1600 && moved1600 > moved800, "MID handle +100% moves the peak from 800 Hz to 1.6 kHz", moved1600, "dB");
        // Slopes: 1 kHz below a 4 kHz LPF... use HPF at 1 kHz, measure at 250 Hz (2 octaves below).
        v[ToneModule::Mid] = 0; v[ToneModule::Hpf] = 1000;
        float att[4];
        for (int sl = 0; sl < 4; ++sl) { v[ToneModule::Slope] = float(sl); att[sl] = ToneModule::curveDb(v.data(), 250, 48000); }
        check(att[0] > att[1] && att[1] > att[2] && att[2] > att[3], "HPF x1 / x2 / x4 / x8 get steeper (dB at 2 oct below, x8)", att[3], "dB");
        check(std::fabs(att[1] - (-24.0f)) < 1.5f && std::fabs(att[3] - (-96.0f)) < 3.0f, "x2 = 12 dB/oct, x8 = 48 dB/oct (x2 dB)", att[1], "dB");
    }
    // AIR+ exciter lifts quiet top end more than loud top end.
    {
        auto lift = [&](double amp) {
            auto run = [&](float air) {
                HemTone t; t.prepare(48000, 512);
                t.params().set(dm::G_AutoGain, 0);
                t.params().set(HemTone::kFirst + ToneModule::Mode, 0);
                t.params().set(HemTone::kFirst + ToneModule::Air, air);
                t.reset();
                return render(t, sine(48000, 1.0, 14000.0, amp));
            };
            return db(rms(run(10).l, 24000)) - db(rms(run(0).l, 24000));
        };
        const double quiet = lift(0.003), loud = lift(0.5);
        check(quiet > loud + 1.0, "AIR+ +10: quiet 14 kHz lifted more than loud 14 kHz (quiet lift dB)", quiet, "dB");
    }
}

} // namespace

// EXT KEY: the compressor follows the sidechain input, not the track.
void testSidechain() {
    section("HEM SYNDICATE RACK - external sidechain (EXT KEY)");
    const float sr = 48000.0f;
    const auto in = musicSignal(sr, 3.0, -6.0);
    const Stereo silent(in.size());
    const auto loudKey = pulseKey(sr, in.size());
    auto grWith = [&](bool ext, const Stereo* key) {
        SyndicateRack e;
        e.prepare(sr, 512);
        e.params().set(kComp + CompModule::Amount, 80);
        e.params().set(kComp + CompModule::ExtKey, ext ? 1.0f : 0.0f);
        e.reset();
        double grMax = 0;
        Stereo out = in;
        for (std::size_t pos = 0; pos < out.size(); pos += 256) {
            const int n = int(std::min<std::size_t>(256, out.size() - pos));
            if (key) e.setSidechain(key->l.data() + pos, key->r.data() + pos);
            e.process(out.l.data() + pos, out.r.data() + pos, n);
            grMax = std::max(grMax, double(std::fabs(e.grDb())));
        }
        return grMax;
    };
    const double internal = grWith(false, &silent), keySilent = grWith(true, &silent), keyLoud = grWith(true, &loudKey);
    const double noKeyBus = grWith(true, nullptr);
    check(internal > 3.0, "EXT KEY off: compresses on the track itself (max GR dB)", internal, "dB");
    check(keySilent < 0.5, "EXT KEY on + silent key: no compression (max GR dB)", keySilent, "dB");
    check(keyLoud > 3.0, "EXT KEY on + loud key: compresses (max GR dB)", keyLoud, "dB");
    check(std::fabs(noKeyBus - internal) < 0.05, "EXT KEY on, host sends no sidechain: falls back to the track (GR diff dB)",
          std::fabs(noKeyBus - internal), "dB");
}

void runSyndicateTests() {
    testOriginalFeatures();
    runSuite(rackCfg());
    testRackMeters();
    testToneRouting();

    runSuite(splitCfg<HemSat>("HEM SAT (split)", {},
        {{HemSat::kFirst + SatModule::Mode, {{HemSat::kFirst + SatModule::Amount, 80}}, "SATURATE"}}));
    {
        auto c = splitCfg<HemComp>("HEM COMP (split)",
            {{HemComp::kFirst + CompModule::Sidechain, {{HemComp::kFirst + CompModule::Amount, 80}}},
             {HemComp::kFirst + CompModule::ExtKey, {{HemComp::kFirst + CompModule::Amount, 80}}}},
            {{HemComp::kFirst + CompModule::Mode, {{HemComp::kFirst + CompModule::Amount, 80}}, "DYNAMICS"}});
        c.keySignal = pulseKey;
        runSuite(c);
    }
    testSidechain();
    {
        auto c = splitCfg<HemTransient>("HEM TRANSIENT (split)",
            {{HemTransient::kOn, {{HemTransient::kFirst + TransientModule::Amount, 60}}},
             {HemTransient::kFirst + TransientModule::Shape, {{HemTransient::kFirst + TransientModule::Amount, 60}}}}, {});
        c.activeCtx = {{HemTransient::kFirst + TransientModule::Amount, 60}}; // default 0% = neutral (does nothing, by design)
        runSuite(c);
    }
    {
        std::vector<WiredOverride> ov = {{HemTone::kFirst + ToneModule::Slope, {{HemTone::kFirst + ToneModule::Hpf, 200}}}};
        for (std::size_t b = 0; b < 6; ++b)
            ov.push_back({HemTone::kFirst + ToneModule::SubF + b, {{HemTone::kFirst + ToneModule::Sub + b, 8}}});
        runSuite(splitCfg<HemTone>("HEM TONE (split)", ov,
        {{HemTone::kFirst + ToneModule::Mode, {{HemTone::kFirst + ToneModule::Low, 6}, {HemTone::kFirst + ToneModule::Air, 6}}, "TONE"}}));
    }
    runSuite(splitCfg<HemMod>("HEM MOD (split)",
        {{HemMod::kFirst + ModModule::Depth, {{HemMod::kFirst + ModModule::Amount, 80}}},
         {HemMod::kFirst + ModModule::Drift, {{HemMod::kFirst + ModModule::Amount, 80}}}},
        {{HemMod::kFirst + ModModule::Mode, {{HemMod::kFirst + ModModule::Amount, 80}}, "MODULATE"}}));
    runSuite(splitCfg<HemMax>("HEM MAX (split)",
        {{HemMax::kFirst + MaxModule::Clip, {{HemMax::kFirst + MaxModule::Max, 100}}},
         {HemMax::kFirst + MaxModule::XoverLow, {{HemMax::kFirst + MaxModule::Multi, 80}}},
         {HemMax::kFirst + MaxModule::XoverHigh, {{HemMax::kFirst + MaxModule::Multi, 80}}},
         {HemMax::kFirst + MaxModule::BandLow, {{HemMax::kFirst + MaxModule::Multi, 80}}},
         {HemMax::kFirst + MaxModule::BandMid, {{HemMax::kFirst + MaxModule::Multi, 80}}},
         {HemMax::kFirst + MaxModule::BandHigh, {{HemMax::kFirst + MaxModule::Multi, 80}}}},
        {{HemMax::kFirst + MaxModule::Mode, {{HemMax::kFirst + MaxModule::Multi, 80}}, "MAXIMIZE"}}));
}
