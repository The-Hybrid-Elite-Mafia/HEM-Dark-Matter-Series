// HEM PHANTOM VOX - quality gates (protocol section 4) proven with real audio.
#include "TestHarness.hpp"

#include "dm/core/Safety.hpp"
#include "dm/phantom_vox/PhantomVox.hpp"

#include <chrono>
#include <memory>
#include <string>

using namespace dmt;
using namespace dm::pv;

namespace dmt {
double kLoudnessDb(const Stereo& s, float sr, std::size_t from) {
    dm::KWeight k;
    k.prepare(sr);
    double sum = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const float a = k.process(0, s.l[i]), b = k.process(1, s.r[i]);
        if (i >= from) sum += double(a) * a + double(b) * b;
    }
    return 10.0 * std::log10(std::max(sum / double(std::max<std::size_t>(1, s.size() - from)), 1e-20));
}
} // namespace dmt

namespace {

constexpr float kSr = 48000.0f;
const double kCeiling = std::pow(10.0, -0.1 / 20.0);

std::unique_ptr<PhantomVox> make(float sr = kSr) {
    auto e = std::make_unique<PhantomVox>();
    e->prepare(sr, 512);
    return e;
}

void allModulesOff(PhantomVox& e) {
    for (auto id : {DynOn, ToneOn, SpaceOn, SfxOn}) e.params().set(id, 0);
}

// Shift `in` by `lat` samples (what a latency-compensated host would line up).
Stereo delayed(const Stereo& in, int lat) {
    Stereo d(in.size());
    for (std::size_t i = std::size_t(lat); i < in.size(); ++i) { d.l[i] = in.l[i - lat]; d.r[i] = in.r[i - lat]; }
    return d;
}

void testUnityAllOff() {
    section("Unity gain: all four bays OFF (dry must pass bit-exact, latency aligned)");
    auto e = make();
    allModulesOff(*e);
    e->reset();
    const auto in = vocalSignal(kSr, 3.0, -6.0);
    const auto out = render(*e, in);
    const auto ref = delayed(in, e->latencySamples());
    double maxErr = 0;
    for (std::size_t i = 0; i < in.size(); ++i)
        maxErr = std::max({maxErr, double(std::fabs(out.l[i] - ref.l[i])), double(std::fabs(out.r[i] - ref.r[i]))});
    check(maxErr <= 1e-7, "max |out - dry| with every module off", maxErr);
    check(true, "reported latency (samples)", e->latencySamples());
    const auto a = e->lastActivity();
    check(!a.dyn && !a.tone && !a.space && !a.sfx, "no module DSP ran while all off (CPU hygiene)", 0);
}

void testBypassExact() {
    section("Global BYPASS");
    auto e = make();
    e->params().set(Bypass, 1);
    e->reset();
    const auto in = vocalSignal(kSr, 2.0, -3.0);
    const auto out = render(*e, in);
    const auto ref = delayed(in, e->latencySamples());
    double maxErr = 0;
    for (std::size_t i = 4800; i < in.size(); ++i)
        maxErr = std::max(maxErr, double(std::fabs(out.l[i] - ref.l[i])));
    check(maxErr == 0.0, "bypassed output == latency-aligned input", maxErr);
}

void testDefaultLoad() {
    section("Default load: no clip, level within +/-1.0 dB (after auto-gain settles)");
    auto e = make();
    const auto in = vocalSignal(kSr, 8.0, -6.0);
    const auto out = render(*e, in);
    const std::size_t from = std::size_t(kSr * 3.0);
    const double dIn = kLoudnessDb(in, kSr, from), dOut = kLoudnessDb(out, kSr, from);
    check(std::fabs(dOut - dIn) <= 1.0, "K-weighted level change, defaults (dB)", dOut - dIn, "dB");
    auto stRms = [&](const Stereo& s) { return 0.5 * (db(rms(s.l, from)) + db(rms(s.r, from))); };
    check(std::fabs(stRms(out) - stRms(in)) <= 1.0, "plain stereo RMS level change, defaults (dB)",
          stRms(out) - stRms(in), "dB");
    const double pk = std::max(peak(out.l), peak(out.r));
    check(pk <= kCeiling, "output peak, defaults (dBFS)", db(pk), "dBFS");

    // Hot input at full scale must still never exceed the ceiling.
    auto h = make();
    const auto hot = vocalSignal(kSr, 4.0, 0.0);
    const auto ho = render(*h, hot);
    const double hpk = std::max(peak(ho.l), peak(ho.r));
    check(hpk <= kCeiling, "output peak with 0 dBFS input (dBFS)", db(hpk), "dBFS");

    // Protocol defaults.
    const auto& t = paramTable();
    check(t[ReverbMix].def >= 20 && t[ReverbMix].def <= 35, "REVERB default mix within 20-35%", t[ReverbMix].def, "%");
    check(t[DelayMix].def >= 20 && t[DelayMix].def <= 35, "DELAY default mix within 20-35%", t[DelayMix].def, "%");
    check(dm::assumed::pv::kChorusRateHz > 0 && dm::assumed::pv::kPhaserRateHz > 0 &&
          dm::assumed::pv::kTapeWowHz > 0, "all SFX LFO rates active (> 0 Hz)", dm::assumed::pv::kChorusRateHz, "Hz");
}

void testPresets() {
    section("Factory presets: no clip, level within +/-1.0 dB on load");
    const auto presets = factoryPresets();
    for (std::size_t p = 0; p < presets.size(); ++p) {
        auto e = make();
        e->loadPreset(p);
        e->reset();
        const auto in = vocalSignal(kSr, 8.0, -6.0);
        const auto out = render(*e, in);
        const std::size_t from = std::size_t(kSr * 3.0);
        const double d = kLoudnessDb(out, kSr, from) - kLoudnessDb(in, kSr, from);
        const double pk = std::max(peak(out.l), peak(out.r));
        std::string name = std::string(presets[p].name);
        check(std::fabs(d) <= 1.0, (name + ": level change (dB)").c_str(), d, "dB");
        check(pk <= kCeiling && allFinite(out), (name + ": peak (dBFS)").c_str(), db(pk), "dBFS");
    }
}

void testExtremeSweep() {
    section("Extreme parameter sweep with +12 dB over-driven input: never above -0.1 dBFS, never NaN");
    auto e = make();
    std::uint32_t seed = 777u;
    auto rnd = [&]() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return double(seed) / 4294967296.0; };
    auto in = vocalSignal(kSr, 20.0, 12.0);
    Stereo out = in;
    double pk = 0;
    for (std::size_t pos = 0; pos < out.size(); pos += 256) {
        if ((pos / 256) % 20 == 0) {
            for (std::size_t i = 0; i < kParamCount; ++i) {
                if (i == Bypass || i == AutoGainOn) continue;
                const auto& d = paramTable()[i];
                e->params().set(i, float(d.min + (d.max - d.min) * (rnd() < 0.3 ? (rnd() < 0.5 ? 0.0 : 1.0) : rnd())));
            }
            if (rnd() < 0.3) e->params().set(AutoGainOn, 0);
            else e->params().set(AutoGainOn, 1);
        }
        const int n = int(std::min<std::size_t>(256, out.size() - pos));
        e->process(out.l.data() + pos, out.r.data() + pos, n);
    }
    pk = std::max(peak(out.l), peak(out.r));
    check(pk <= kCeiling + 1e-6, "max output peak over 20 s random sweep (dBFS)", db(pk), "dBFS");
    check(allFinite(out), "all samples finite", 1);
}

// Render with defaults + overrides, auto-gain OFF so level-changing controls are visible.
Stereo renderWith(std::initializer_list<std::pair<P, float>> ctx, P id, float value, const Stereo& in) {
    auto e = make();
    e->params().set(AutoGainOn, 0);
    for (auto [k, v] : ctx) e->params().set(k, v);
    e->params().set(id, value);
    e->reset();
    return render(*e, in);
}

void testEveryControlWired() {
    section("Every control changes the audio (min vs max / next choice), auto-gain off");
    const auto in = vocalSignal(kSr, 3.0, -6.0);
    for (std::size_t i = 0; i < kParamCount; ++i) {
        const auto id = static_cast<P>(i);
        if (id == AutoGainOn) continue; // tested separately below (it is the thing turned off here)
        const auto& d = paramTable()[i];
        std::initializer_list<std::pair<P, float>> ctx = {};
        // Context so the control has something to act on.
        static const std::initializer_list<std::pair<P, float>> kVoice = {{Low, 6}, {Mid, 6}};
        static const std::initializer_list<std::pair<P, float>> kFlt = {{Hpf, 400}, {Comp, 80}};
        static const std::initializer_list<std::pair<P, float>> kGate = {{Gate, -40}};
        if (id == VoiceMV) ctx = kVoice;
        if (id == FltPost) ctx = kFlt;
        static const std::initializer_list<std::pair<P, float>> kEq = {{Low, 6}, {Mid, 6}, {High, 6}, {Air, 6}};
        if (id == DynMode) ctx = kGate;
        if (id == ToneMode) ctx = kEq; // with flat faders the clean voicings are (correctly) identical
        float a = d.def, b = d.max;
        if (d.def == d.max) b = d.min;
        if (d.isChoice()) b = float((int(d.def) + 1) % int(d.choices.size()));
        if (id == Gate) { a = -96; b = -30; }
        const auto oa = renderWith(ctx, id, a, in);
        const auto ob = renderWith(ctx, id, b, in);
        const double dr = diffRms(oa, ob);
        check(dr > 1e-5, (std::string(d.id) + " changes output (diff RMS dBFS)").c_str(), db(dr), "dBFS");
    }
    // Auto-gain itself: with a +12 dB EQ boost it must pull the level back.
    auto on = make(); on->params().set(Low, 12); on->params().set(Mid, 12); on->params().set(High, 12); on->reset();
    auto off = make(); off->params().set(AutoGainOn, 0); off->params().set(Low, 12); off->params().set(Mid, 12);
    off->params().set(High, 12); off->reset();
    const auto ao = render(*on, in), af = render(*off, in);
    const std::size_t from = std::size_t(kSr * 1.5);
    check(kLoudnessDb(af, kSr, from) - kLoudnessDb(ao, kSr, from) > 3.0, "autogain pulls a +12 dB EQ boost back (dB)",
          kLoudnessDb(af, kSr, from) - kLoudnessDb(ao, kSr, from), "dB");
}

void testModesDistinct() {
    section("Every mode in every bay sounds different from its neighbours");
    const auto in = vocalSignal(kSr, 2.0, -6.0);
    struct Bay { P mode; int count; std::initializer_list<std::pair<P, float>> ctx; const char* name; };
    static const std::initializer_list<std::pair<P, float>> dynCtx = {{Comp, 70}, {Gate, -40}};
    static const std::initializer_list<std::pair<P, float>> toneCtx = {{Low, 6}, {Mid, 6}, {High, 6}, {Air, 6}};
    static const std::initializer_list<std::pair<P, float>> spaceCtx = {{ReverbMix, 50}, {DelayMix, 30}};
    static const std::initializer_list<std::pair<P, float>> sfxCtx = {{Sfx, 70}};
    const Bay bays[] = {
        {DynMode, dm::assumed::pv::kDynModes, dynCtx, "DYNAMICS"},
        {ToneMode, dm::assumed::pv::kToneModes, toneCtx, "TONE"},
        {SpaceMode, dm::assumed::pv::kSpaceModes, spaceCtx, "SPACE"},
        {SfxMode, dm::assumed::pv::kSfxModes, sfxCtx, "SFX"},
    };
    for (const auto& bay : bays) {
        std::vector<Stereo> outs;
        for (int m = 0; m < bay.count; ++m) outs.push_back(renderWith(bay.ctx, bay.mode, float(m), in));
        double minDiff = 1e9;
        int worstA = 0, worstB = 0;
        for (int a = 0; a < bay.count; ++a)
            for (int b = a + 1; b < bay.count; ++b) {
                const double dr = diffRms(outs[std::size_t(a)], outs[std::size_t(b)]);
                if (dr < minDiff) { minDiff = dr; worstA = a; worstB = b; }
            }
        const auto& ch = paramTable()[bay.mode].choices;
        std::printf("  closest pair in %s: %s vs %s\n", bay.name, ch[std::size_t(worstA)], ch[std::size_t(worstB)]);
        check(minDiff > 1e-4, (std::string(bay.name) + ": smallest difference between two modes (dBFS)").c_str(),
              db(minDiff), "dBFS");
    }
}

void testMeters() {
    section("Meters read real DSP state");
    const auto in = vocalSignal(kSr, 3.0, -6.0);
    {
        auto e = make(); e->params().set(Comp, 90); e->reset();
        Stereo o = in; float maxGr = 0;
        for (std::size_t p = 0; p < o.size(); p += 256) {
            e->process(o.l.data() + p, o.r.data() + p, int(std::min<std::size_t>(256, o.size() - p)));
            maxGr = std::max(maxGr, e->meters().gr.read());
        }
        check(maxGr > 3.0f, "GR meter shows compression at COMP 90% (dB)", maxGr, "dB");
    }
    {
        auto e = make(); e->params().set(DeEss, 100); e->reset();
        Stereo o = in; float maxDs = 0;
        for (std::size_t p = 0; p < o.size(); p += 256) {
            e->process(o.l.data() + p, o.r.data() + p, int(std::min<std::size_t>(256, o.size() - p)));
            maxDs = std::max(maxDs, e->meters().ds.read());
        }
        check(maxDs > 1.0f, "DS meter shows de-essing on sibilants at 100% (dB)", maxDs, "dB");
    }
    {
        auto e = make(); e->params().set(Gate, -40); e->reset();
        Stereo o = in; bool closed = false;
        for (std::size_t p = 0; p < o.size(); p += 256) {
            e->process(o.l.data() + p, o.r.data() + p, int(std::min<std::size_t>(256, o.size() - p)));
            closed |= e->meters().gateOpen.read() < 0.5f;
        }
        check(closed, "gate closes in the -70 dBFS gaps at GATE -40 dB", closed ? 1 : 0);
    }
    {
        auto e = make(); e->reset();
        Stereo o = in; float ri = 0, ro = 0, di = 0, dO = 0, rd = 0, dd = 0;
        for (std::size_t p = 0; p < o.size(); p += 256) {
            e->process(o.l.data() + p, o.r.data() + p, int(std::min<std::size_t>(256, o.size() - p)));
            const auto& m = e->meters();
            ri = std::max(ri, m.revIn.read()); ro = std::max(ro, m.revOut.read());
            di = std::max(di, m.dlyIn.read()); dO = std::max(dO, m.dlyOut.read());
            rd = std::max(rd, m.revDuck.read()); dd = std::max(dd, m.dlyDuck.read());
        }
        check(ri > 0.01f && ro > 0.001f, "REVERB IN / OUT meters live (OUT peak dBFS)", db(ro), "dBFS");
        check(di > 0.01f && dO > 0.001f, "DELAY IN / OUT meters live (OUT peak dBFS)", db(dO), "dBFS");
        check(rd > 1.0f, "REVERB DUCK meter at VRB DUCK 32% (dB)", rd, "dB");
        check(dd > 0.5f, "DELAY DUCK meter at DLY DUCK 17% (dB)", dd, "dB");
        check(rd > dd, "32% reverb duck deeper than 17% delay duck", rd - dd, "dB");
    }
}

void testZeroAllocation() {
    section("Zero heap allocation on the audio thread (incl. mode + preset changes)");
    auto e = make();
    auto in = vocalSignal(kSr, 4.0, -6.0);
    std::uint64_t count = 0;
    {
        AllocScope scope;
        for (std::size_t pos = 0, k = 0; pos < in.size(); pos += 256, ++k) {
            e->params().set(DynMode, float(k % 7));
            e->params().set(ToneMode, float(k % 7));
            e->params().set(SpaceMode, float(k % 14));
            e->params().set(SfxMode, float(k % 11));
            e->params().set(DelaySync, float(k % 14));
            e->process(in.l.data() + pos, in.r.data() + pos, int(std::min<std::size_t>(256, in.size() - pos)));
        }
        count = scope.count();
    }
    check(count == 0, "heap allocations during process()", double(count));
}

void testCpuHygiene() {
    section("CPU hygiene: disabled bays skip their DSP loops");
    const auto in = vocalSignal(kSr, 10.0, -6.0);
    auto timeIt = [&](bool allOn) {
        auto e = make();
        if (!allOn) allModulesOff(*e);
        e->reset();
        Stereo o = in;
        const auto t0 = std::chrono::steady_clock::now();
        for (std::size_t p = 0; p < o.size(); p += 256)
            e->process(o.l.data() + p, o.r.data() + p, int(std::min<std::size_t>(256, o.size() - p)));
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    };
    const double on = timeIt(true), off = timeIt(false);
    check(true, "10 s of audio, all bays ON (ms)", on, "ms");
    check(true, "10 s of audio, all bays OFF (ms)", off, "ms");
    check(off < on * 0.35, "bays-off cost as a fraction of bays-on", off / on);
    check(on < 10000.0 * 0.25, "all-on realtime load (fraction of realtime, single core)", on / 10000.0);
}

void testAnalyzer() {
    section("FFT ANALYZER reads the real signal");
    auto e = make();
    allModulesOff(*e);
    e->params().set(ToneOn, 1);
    e->params().set(Hpf, 20);
    e->reset();
    const auto in = sine(kSr, 1.0, 1000.0, 0.5);
    (void)render(*e, in);
    const auto& mag = e->analyzer().compute();
    std::size_t best = 1;
    for (std::size_t k = 1; k < mag.size(); ++k) if (mag[k] > mag[best]) best = k;
    const double hz = e->analyzer().binHz(int(best));
    check(std::fabs(hz - 1000.0) < 15.0, "analyzer peak frequency for a 1 kHz sine (Hz)", hz, "Hz");
    check(std::fabs(mag[best] - db(0.5)) < 1.6, "analyzer peak level for a -6.02 dBFS sine (dBFS)", mag[best], "dBFS");
}

void testToneCurve() {
    section("TONE curve helper matches the EQ settings");
    auto e = make();
    e->params().set(Hpf, 20);
    const float flatAir = e->toneCurveDb(16000.0f);
    e->params().set(Air, 6);
    const float boostAir = e->toneCurveDb(16000.0f);
    check(std::fabs((boostAir - flatAir) - 6.0f) < 1.5f, "AIR+ +6 dB shows on the curve at 16 kHz (dB)", boostAir - flatAir, "dB");
    e->params().set(Low, -6);
    const float lowCut = e->toneCurveDb(40.0f);
    check(lowCut < -4.0f, "LOW -6 dB shows on the curve at 40 Hz (dB)", lowCut, "dB");
}

void testSampleRates() {
    section("Sample rates 44.1 / 48 / 96 kHz: defaults safe");
    for (float sr : {44100.0f, 48000.0f, 96000.0f}) {
        auto e = make(sr);
        const auto in = vocalSignal(sr, 6.0, -6.0);
        const auto out = render(*e, in, 480);
        const std::size_t from = std::size_t(sr * 3.0);
        const double d = kLoudnessDb(out, sr, from) - kLoudnessDb(in, sr, from);
        const double pk = std::max(peak(out.l), peak(out.r));
        char label[96];
        std::snprintf(label, sizeof label, "%.1f kHz: level change (dB)", sr / 1000.0);
        check(std::fabs(d) <= 1.0 && allFinite(out), label, d, "dB");
        std::snprintf(label, sizeof label, "%.1f kHz: peak (dBFS)", sr / 1000.0);
        check(pk <= kCeiling, label, db(pk), "dBFS");
    }
}

void testUndoRedo() {
    section("Undo / redo (top bar arrows)");
    PhantomVox e;
    dm::History<kParamCount> h;
    h.commit(e.params().snapshot());
    e.params().set(Comp, 80); h.commit(e.params().snapshot());
    e.params().set(Comp, 10); h.commit(e.params().snapshot());
    auto s = e.params().snapshot();
    h.undo(s); e.params().restore(s);
    check(e.params().get(Comp) == 80.0f, "undo restores COMP 80", e.params().get(Comp));
    h.redo(s); e.params().restore(s);
    check(e.params().get(Comp) == 10.0f, "redo restores COMP 10", e.params().get(Comp));
}

} // namespace

void runPhantomVoxTests() {
    std::printf("\n################ HEM PHANTOM VOX ################\n");
    testUnityAllOff();
    testBypassExact();
    testDefaultLoad();
    testPresets();
    testExtremeSweep();
    testEveryControlWired();
    testModesDistinct();
    testMeters();
    testZeroAllocation();
    testCpuHygiene();
    testAnalyzer();
    testToneCurve();
    testSampleRates();
    testUndoRedo();
}
