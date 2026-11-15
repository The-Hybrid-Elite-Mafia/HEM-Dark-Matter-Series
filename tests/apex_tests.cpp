// HEM APEX CORE - quality gates.
#include "Suite.hpp"

#include "dm/apex/ApexCore.hpp"

using namespace dmt;
using namespace dm::ax;

void runApexTests() {
    SuiteCfg<ApexCore> c;
    c.name = "HEM APEX CORE";
    c.vocal = false;
    c.make = [](float sr) { auto e = std::make_unique<ApexCore>(); e->prepare(sr, 512); return e; };
    c.allOff = [](ApexCore& e) { e.params().set(Parallel, 0); };
    c.overrides = {
        {MonoSafe, {{Width, 200}}},
        {Attack, {{Peaks, 90}}, 25, 400},
        {Release, {{Peaks, 90}}, 25, 400},
    };
    c.modes = {{Engine, {{Amount, 160}}, "ENGINE"}};
    c.presetCount = apexPresets().size();
    c.loadPreset = [](ApexCore& e, std::size_t i) { e.loadPreset(i); };
    c.presetName = [](std::size_t i) { return apexPresets()[i].name.data(); };
    runSuite(c);

    section("APEX meters + display helpers");
    ApexCore e;
    e.prepare(48000, 512);
    e.params().set(Width, 150);
    e.reset();
    auto in = musicSignal(48000, 3.0, -6.0);
    float gr = 0;
    for (std::size_t p = 0; p < in.size(); p += 256) {
        e.process(in.l.data() + p, in.r.data() + p, int(std::min<std::size_t>(256, in.size() - p)));
        gr = std::max(gr, e.grDb());
    }
    check(gr > 0.5f, "GR meter live at defaults (dB)", gr, "dB");
    check(e.widthPercent() > 120.0f, "WIDTH readout follows WIDTH 150% (measured %)", e.widthPercent(), "%");
    check(e.transferDb(0.0f) < -2.0f, "transfer curve holds a 0 dBFS peak down (dB out)", e.transferDb(0.0f), "dB");
    check(std::fabs(e.transferDb(-40.0f) - (-40.0f + 6.0f * 0.27f)) < 0.5f, "transfer curve is linear far below threshold", e.transferDb(-40.0f), "dB");
    check(e.attackMs() == 10.0f && e.releaseMs() == 150.0f, "ATTACK / RELEASE 100% = PRIMA timings (ms)", e.attackMs(), "ms");
}
