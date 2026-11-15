// HEM OUTLAW RIDER + HEM DARK MATTER 4D - quality gates.
#include "Suite.hpp"

#include "dm/darkmatter/DarkMatter4D.hpp"
#include "dm/rider/OutlawRider.hpp"

using namespace dmt;

namespace {

void riderBehaviour() {
    using namespace dm::rd;
    constexpr float sr = 48000;
    section("OUTLAW RIDER rides real level changes toward TARGET");
    // Two phrases 12 dB apart (-24 and -12 dBFS RMS); TARGET -18 sits between them, inside +6 / -12 limits.
    Stereo in(std::size_t(sr * 8));
    for (std::size_t i = 0; i < in.size(); ++i) {
        const double t = double(i) / sr;
        double v = 0;
        for (int h = 1; h <= 5; ++h) v += std::sin(6.283185307179586 * 180.0 * h * t) / h;
        in.l[i] = in.r[i] = float(v);
    }
    const std::size_t half = in.size() / 2;
    const double r0 = rms(in.l, 0, half);
    for (std::size_t i = 0; i < in.size(); ++i) {
        const double g = std::pow(10.0, (i < half ? -24.0 : -12.0) / 20.0) / r0;
        in.l[i] = in.r[i] = float(in.l[i] * g);
    }
    OutlawRider e;
    e.prepare(sr, 512);
    e.params().set(dm::G_AutoGain, 0);
    e.reset();
    const auto out = render(e, in);
    const double a = db(rms(out.l, std::size_t(sr * 2.5), std::size_t(sr * 3.9)));
    const double b = db(rms(out.l, std::size_t(sr * 6.5), std::size_t(sr * 7.9)));
    const double ia = db(rms(in.l, std::size_t(sr * 2.5), std::size_t(sr * 3.9)));
    const double ib = db(rms(in.l, std::size_t(sr * 6.5), std::size_t(sr * 7.9)));
    check(std::fabs(b - a) < 0.5 * std::fabs(ib - ia), "phrase level gap after riding vs before (dB)", std::fabs(b - a), "dB");
    check(std::fabs(a - (-18.0)) < 3.0, "quiet phrase lands near TARGET -18 (dBFS RMS)", a, "dB");
    RideFrame f[64];
    const std::size_t n = e.readFrames(f, 64);
    check(n == 64 && f[63].gainDb < -3.0f, "TRUEGAIN display frames flow (latest gain dB, loud phrase)", f[63].gainDb, "dB");
    check(e.gainDb() <= 0.0f && e.gainDb() >= -12.01f, "gain readout stays inside MIN cut (dB)", e.gainDb(), "dB");
}

void darkMatterBehaviour() {
    using namespace dm::d4;
    constexpr float sr = 48000;
    section("DARK MATTER 4D: decay, ducking meters, visualizers");
    auto tailDb = [&](float decay) {
        DarkMatter4D e; e.prepare(sr, 512);
        e.params().set(dm::G_AutoGain, 0); e.params().set(Reverb, 50); e.params().set(Delay, 0);
        e.params().set(Decay, decay); e.params().set(Ducking, 0);
        e.reset();
        Stereo s(std::size_t(sr * 4));
        for (std::size_t i = 0; i < std::size_t(sr * 0.05); ++i) s.l[i] = s.r[i] = float(0.5 * std::sin(0.3 * double(i)));
        const auto out = render(e, s);
        return db(rms(out.l, std::size_t(sr * 3.0), std::size_t(sr * 3.5)));
    };
    const double shortT = tailDb(0.5f), longT = tailDb(20.0f);
    check(longT - shortT > 30.0, "tail at 3 s: DECAY 20 s vs 0.5 s (dB louder)", longT - shortT, "dB");
    DarkMatter4D e; e.prepare(sr, 512);
    e.params().set(Ducking, 80);
    e.reset();
    auto in = vocalSignal(sr, 3.0, -6.0);
    float dk = 0, rv = -200, dl = -200;
    for (std::size_t p = 0; p < in.size(); p += 256) {
        e.process(in.l.data() + p, in.r.data() + p, int(std::min<std::size_t>(256, in.size() - p)));
        dk = std::max(dk, std::max(e.duckDb(0), e.duckDb(1)));
        rv = std::max(rv, e.reverbEnergyDb()); dl = std::max(dl, e.delayTapDb());
    }
    check(dk > 6.0f, "L/R duck meters at DUCKING 80% (dB)", dk, "dB");
    check(rv > -60.0f && dl > -60.0f, "reverb + delay visualizer feeds live (reverb dBFS)", rv, "dBFS");
    check(e.shapeCurve(0) < e.shapeCurve(500), "SHAPE curve recovers over time", e.shapeCurve(500));
}

} // namespace

void runRiderDarkMatterTests() {
    {
        using namespace dm::rd;
        SuiteCfg<OutlawRider> c;
        c.name = "HEM OUTLAW RIDER";
        c.make = [](float sr) { auto e = std::make_unique<OutlawRider>(); e->prepare(sr, 512); return e; };
        c.allOff = [](OutlawRider& e) { e.params().set(Range, 0); };
        c.overrides = {{Sensitivity, {}, -80, -20}, {MinCut, {{Target, -40}}, 12, 0}};
        c.presetCount = riderPresets().size();
        c.loadPreset = [](OutlawRider& e, std::size_t i) { e.loadPreset(i); };
        c.presetName = [](std::size_t i) { return riderPresets()[i].name.data(); };
        runSuite(c);
        riderBehaviour();
    }
    {
        using namespace dm::d4;
        SuiteCfg<DarkMatter4D> c;
        c.name = "HEM DARK MATTER 4D";
        c.make = [](float sr) { auto e = std::make_unique<DarkMatter4D>(); e->prepare(sr, 512); return e; };
        c.allOff = [](DarkMatter4D& e) { e.params().set(Reverb, 0); e.params().set(Delay, 0); };
        c.overrides = {{Shape, {{Ducking, 80}}}};
        c.modes = {{Routing, {{Reverb, 50}, {Delay, 50}}, "ROUTING"}};
        c.presetCount = darkMatterPresets().size();
        c.loadPreset = [](DarkMatter4D& e, std::size_t i) { e.loadPreset(i); };
        c.presetName = [](std::size_t i) { return darkMatterPresets()[i].name.data(); };
        runSuite(c);
        darkMatterBehaviour();
    }
}
