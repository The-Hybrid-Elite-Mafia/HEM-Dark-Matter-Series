// HEM CHAINBREAKER 108 - quality gates.
#include "Suite.hpp"

#include "dm/chainbreaker/Chainbreaker108.hpp"

using namespace dmt;
using namespace dm::cb;

void runChainbreakerTests() {
    SuiteCfg<Chainbreaker108> c;
    c.name = "HEM CHAINBREAKER 108";
    c.vocal = false;
    c.make = [](float sr) { auto e = std::make_unique<Chainbreaker108>(); e->prepare(sr, 512); return e; };
    c.allOff = [](Chainbreaker108& e) { e.params().set(MMix, 0); };
    c.overrides = {
        {StutX, {{Flux, 100}}}, {StutY, {{Flux, 100}}}, {Tempo, {{Flux, 100}}},
        {Eq1F, {{Eq1G, 6}}}, {Eq1Q, {{Eq1G, 6}}}, {Eq2F, {{Eq2G, 6}}}, {Eq2Q, {{Eq2G, 6}}},
        {Eq3F, {{Eq3G, 6}}}, {Eq3Q, {{Eq3G, 6}}}, {Eq4F, {{Eq4G, 6}}}, {Eq4Q, {{Eq4G, 6}}},
        {ClipShape, {{Drive, 80}}},
        {S4On, {{Eq1G, 6}}},                 // slot 4 defaults to a flat SPECTRO EQ
        {UtilRoute, {{OutLevel, 10}}},       // clip modes need peaks to act on
    };
    c.modes = {
        {S1Type, {{Flux, 100}, {Drive, 60}, {Eq1G, 8}, {GrainMix, 60}}, "SLOT 1 MODULE"},
        {UtilRoute, {{OutLevel, 10}, {BcLow, 6}}, "UTILITY ROUTING"},
    };
    c.presetCount = chainbreakerPresets().size();
    c.loadPreset = [](Chainbreaker108& e, std::size_t i) { e.loadPreset(i); };
    c.presetName = [](std::size_t i) { return chainbreakerPresets()[i].name.data(); };
    runSuite(c);

    section("CHAINBREAKER readouts, Quick-Load chains, slot re-ordering");
    constexpr float sr = 48000;
    auto in = musicSignal(sr, 4.0, -6.0);
    {
        Chainbreaker108 e; e.prepare(sr, 512); applyColouredBase(e);
        e.params().set(Flux, 100); e.params().set(Drive, 90);
        e.reset();
        float hz = 0, cp = 0;
        Stereo o = in;
        for (std::size_t p = 0; p < o.size(); p += 256) {
            e.process(o.l.data() + p, o.r.data() + p, int(std::min<std::size_t>(256, o.size() - p)));
            hz = std::max(hz, e.stutterRateHz());
            for (int b = 0; b < 3; ++b) cp = std::max(cp, e.clipPercent(b));
        }
        check(hz > 1.0f, "STUTTER Hz readout live at FLUX 100% (Hz)", hz, "Hz");
        check(cp > 1.0f, "MULTI-CLIP band clip meters at DRIVE 90% (%)", cp, "%");
        e.params().set(Eq2G, 9);
        check(std::fabs(e.spectroCurveDb(400.0f) - 9.0f) < 0.6f, "SPECTRO EQ graph shows +9 dB at 400 Hz (dB)", e.spectroCurveDb(400.0f), "dB");
    }
    {
        // Every Quick-Load chain recalls a different chain (different audio).
        std::vector<Stereo> outs;
        for (std::size_t q = 0; q < quickLoadChains().size(); ++q) {
            Chainbreaker108 e; e.prepare(sr, 512); applyColouredBase(e);
            e.params().set(dm::G_AutoGain, 0);
            e.params().set(Flux, 60); e.params().set(Drive, 40); e.params().set(Eq1G, 4); e.params().set(GrainMix, 40);
            e.loadChain(q);
            e.reset();
            outs.push_back(render(e, in));
        }
        double minD = 1e9;
        for (std::size_t a = 0; a < outs.size(); ++a)
            for (std::size_t b = a + 1; b < outs.size(); ++b) minD = std::min(minD, diffRms(outs[a], outs[b]));
        check(minD > 1e-4, "8 Quick-Load chains all sound different (smallest diff, dBFS)", db(minD), "dBFS");
    }
    {
        // Dragging modules into a different order changes the result (order matters: clip before vs after EQ).
        auto run = [&](float t1, float t2) {
            Chainbreaker108 e; e.prepare(sr, 512); applyColouredBase(e);
            e.params().set(dm::G_AutoGain, 0);
            e.params().set(Drive, 80); e.params().set(Eq1G, 12);
            e.params().set(S1Type, t1); e.params().set(S2Type, t2); e.params().set(S3On, 0); e.params().set(S4On, 0);
            e.reset();
            return render(e, in);
        };
        const auto clipFirst = run(1, 3), eqFirst = run(3, 1);
        check(diffRms(clipFirst, eqFirst) > 1e-3, "CLIP->EQ vs EQ->CLIP order differs (dBFS)", db(diffRms(clipFirst, eqFirst)), "dBFS");
    }
}
