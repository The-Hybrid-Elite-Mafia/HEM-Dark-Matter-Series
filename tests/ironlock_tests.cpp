// HEM IRON LOCK - quality gates + tuning accuracy measured on real audio.
#include "Suite.hpp"

#include <algorithm>

#include "dm/core/Analyzer.hpp"
#include "dm/ironlock/IronLock.hpp"

using namespace dmt;
using namespace dm::il;

namespace {

// Pitch of the output: normalised autocorrelation peak (50..1000 Hz) with parabolic refinement,
// median over several 4096-sample windows in [from, to).
double measureHz(const std::vector<float>& x, float sr, std::size_t from, std::size_t to) {
    std::vector<double> est;
    const int W = 4096, tMin = int(sr / 1000), tMax = int(sr / 50);
    for (std::size_t s0 = from; s0 + std::size_t(W + tMax) < to; s0 += 4096) {
        std::vector<double> r(std::size_t(tMax + 2), 0.0);
        double e0 = 0;
        for (int j = 0; j < W; ++j) e0 += double(x[s0 + j]) * x[s0 + j];
        for (int t = tMin; t <= tMax + 1; ++t) {
            double acc = 0, e1 = 0;
            for (int j = 0; j < W; ++j) { acc += double(x[s0 + j]) * x[s0 + j + t]; e1 += double(x[s0 + j + t]) * x[s0 + j + t]; }
            r[std::size_t(t)] = acc / std::sqrt(std::max(e0 * e1, 1e-20));
        }
        double best = 0; int bt = 0;
        for (int t = tMin + 1; t <= tMax; ++t) if (r[t] > best && r[t] >= r[t - 1] && r[t] >= r[t + 1]) { best = r[t]; bt = t; }
        // prefer the shortest lag whose peak is within 3% of the best (avoids octave-down picks)
        for (int t = tMin + 1; t <= tMax; ++t)
            if (r[t] > 0.97 * best && r[t] >= r[t - 1] && r[t] >= r[t + 1]) { bt = t; break; }
        if (bt == 0) continue;
        const double a = r[bt - 1], b = r[bt], c = r[bt + 1], den = a - 2 * b + c;
        const double lag = bt + (std::fabs(den) > 1e-12 ? 0.5 * (a - c) / den : 0.0);
        est.push_back(sr / lag);
    }
    if (est.empty()) return 0.0;
    std::sort(est.begin(), est.end());
    return est[est.size() / 2];
}
double cents(double hz, double ref) { return 1200.0 * std::log2(hz / ref); }

std::unique_ptr<IronLock> make(float sr) { auto e = std::make_unique<IronLock>(); e->prepare(sr, 512); return e; }

// Harmonic tone (richer than a sine, like a voice) at hz.
Stereo tone(float sr, double secs, double hz, double amp = 0.3) {
    const std::size_t n = std::size_t(sr * secs);
    Stereo s(n);
    for (std::size_t i = 0; i < n; ++i) {
        double v = 0;
        for (int h = 1; h <= 6; ++h) v += std::sin(6.283185307179586 * hz * h * double(i) / sr) / h;
        s.l[i] = s.r[i] = float(amp * v);
    }
    return s;
}

void testAccuracy() {
    constexpr float sr = 48000;
    section("IRON LOCK tuning accuracy (measured on the output)");
    {
        auto e = make(sr);
        e->params().set(Strength, 100); e->params().set(Transition, 0); e->params().set(dm::G_AutoGain, 0);
        e->reset();
        const double inHz = 440.0 * std::pow(2.0, 35.0 / 1200.0); // A4 +35 cents
        const auto out = render(*e, tone(sr, 3.0, inHz));
        const double hz = measureHz(out.l, sr, std::size_t(sr * 1.5), std::size_t(sr * 2.9));
        check(std::fabs(cents(hz, 440.0)) < 5.0, "A4 +35 c in, STRENGTH 100: output error vs A4 (cents)", cents(hz, 440.0), "c");
        check(e->detectedHz() > 400 && e->detectedHz() < 500, "detected pitch readout (Hz)", e->detectedHz(), "Hz");
    }
    {
        auto e = make(sr);
        e->params().set(Strength, 50); e->params().set(Transition, 0); e->params().set(dm::G_AutoGain, 0);
        e->reset();
        const double inHz = 440.0 * std::pow(2.0, 40.0 / 1200.0);
        const auto out = render(*e, tone(sr, 3.0, inHz));
        const double c = cents(measureHz(out.l, sr, std::size_t(sr * 1.5), std::size_t(sr * 2.9)), 440.0);
        check(std::fabs(c - 20.0) < 6.0, "A4 +40 c in, STRENGTH 50: output should sit near +20 c (cents)", c, "c");
    }
    {
        auto e = make(sr);
        e->params().set(Strength, 0); e->params().set(PitchShift, 12); e->params().set(dm::G_AutoGain, 0);
        e->reset();
        const auto out = render(*e, tone(sr, 3.0, 220.0));
        const double c = cents(measureHz(out.l, sr, std::size_t(sr * 1.5), std::size_t(sr * 2.9)), 440.0);
        check(std::fabs(c) < 8.0, "PITCH SHIFT +12 on 220 Hz -> 440 Hz (error, cents)", c, "c");
    }
    {
        auto e = make(sr);
        e->params().set(Strength, 0); e->params().set(PitchShift, -7); e->params().set(dm::G_AutoGain, 0);
        e->reset();
        const auto out = render(*e, tone(sr, 3.0, 330.0));
        const double want = 330.0 * std::pow(2.0, -7.0 / 12.0);
        const double c = cents(measureHz(out.l, sr, std::size_t(sr * 1.5), std::size_t(sr * 2.9)), want);
        check(std::fabs(c) < 8.0, "PITCH SHIFT -7 on 330 Hz (error, cents)", c, "c");
    }
    {
        // FORMANT moves the spectral envelope, not the pitch.
        auto spectralCentroid = [&](const Stereo& s) {
            dm::AnalyzerTap t; t.prepare(sr);
            for (std::size_t i = 0; i < s.size(); ++i) t.push(s.l[i], s.r[i]);
            const auto& m = t.compute();
            double num = 0, den = 0;
            for (std::size_t k = 1; k < m.size(); ++k) { const double p = std::pow(10.0, m[k] / 10.0); num += p * t.binHz(int(k)); den += p; }
            return num / den;
        };
        auto run = [&](float formant) {
            auto e = make(sr);
            e->params().set(Strength, 0); e->params().set(Formant, formant); e->params().set(dm::G_AutoGain, 0);
            e->reset();
            return render(*e, tone(sr, 2.0, 200.0));
        };
        const auto up = run(6), flat = run(0);
        const double hzUp = measureHz(up.l, sr, std::size_t(sr * 1.0), std::size_t(sr * 1.9));
        check(std::fabs(cents(hzUp, 200.0)) < 10.0, "FORMANT +6 keeps the pitch (error, cents)", cents(hzUp, 200.0), "c");
        const double cu = spectralCentroid(up), cf = spectralCentroid(flat);
        check(cu > cf * 1.15, "FORMANT +6 raises the spectral centroid (ratio)", cu / cf);
    }
    {
        section("QUANTUM KEY + GROUPS");
        auto e = make(sr);
        e->params().set(QuantumKey, 1);
        e->reset();
        // A minor-key melody: A C E G D F -> detector should land on A minor (21) or its relative C major (0).
        Stereo mel(std::size_t(sr * 8));
        const double notes[6] = {220.0, 261.63, 329.63, 392.0, 293.66, 349.23};
        for (std::size_t i = 0; i < mel.size(); ++i) {
            const double hz = notes[(i / std::size_t(sr / 3)) % 6];
            double v = 0;
            for (int h = 1; h <= 4; ++h) v += std::sin(6.283185307179586 * hz * h * double(i) / sr) / h;
            mel.l[i] = mel.r[i] = float(0.3 * v);
        }
        (void)render(*e, mel);
        const int k = e->detectedKey();
        check(k == 21 || k == 0, "QUANTUM KEY finds A minor / C major from an A-C-E-G-D-F melody (key id)", k);
        check(e->noteConfidence(9) > 0.3f, "note confidence grid shows A", e->noteConfidence(9));

        // Two instances with GROUPS on share the scale written by the last one to change it.
        IronLock a1, b1;
        a1.prepare(sr, 512); b1.prepare(sr, 512);
        a1.params().set(Groups, 1); b1.params().set(Groups, 1);
        a1.reset(); b1.reset();
        Stereo blk(256);
        a1.process(blk.l.data(), blk.r.data(), 256); b1.process(blk.l.data(), blk.r.data(), 256);
        for (std::size_t n = NoteC; n <= NoteB; ++n) a1.params().set(n, n == NoteD ? 1.0f : 0.0f); // only D
        a1.process(blk.l.data(), blk.r.data(), 256);
        b1.process(blk.l.data(), blk.r.data(), 256);
        check(b1.activeScaleMask() == (1u << 2), "GROUPS: instance B adopts instance A's scale (mask)", double(b1.activeScaleMask()));
    }
}

} // namespace

void runIronLockTests() {
    SuiteCfg<IronLock> c;
    c.name = "HEM IRON LOCK";
    c.vocal = true;
    c.make = make;
    // Voice that sweeps every pitch class (110-260 Hz) while voiced, with vibrato, gaps and sibilants.
    c.customSignal = [](float sr, double secs, double pk) {
        auto s = vocalSignal(sr, secs, pk);
        const std::size_t n = s.size();
        double ph = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const double t = double(i) / sr, cyc = std::fmod(t, 1.0);
            const double f0 = 110.0 * std::pow(2.0, 1.25 * (0.5 + 0.5 * std::sin(6.283185307179586 * 0.37 * t))) *
                              (1.0 + 0.006 * std::sin(6.283185307179586 * 5.5 * t));
            ph += 6.283185307179586 * f0 / sr;
            if (cyc < 0.55) {
                double v = 0;
                for (int h = 1; h <= 10; ++h) v += std::sin(ph * h) / (h * 1.2);
                const double env = std::sin(3.14159265 * cyc / 0.55);
                s.l[i] = float(0.12 * env * v);
                s.r[i] = float(0.115 * env * v);
            }
        }
        const double p = std::max(peak(s.l), peak(s.r)), g = std::pow(10.0, pk / 20.0) / p;
        for (std::size_t i = 0; i < n; ++i) { s.l[i] = float(s.l[i] * g); s.r[i] = float(s.r[i] * g); }
        return s;
    };
    c.allOff = [](IronLock& e) { e.params().set(Mix, 0); };
    c.skipWired = {Groups};
    Ctx onlyC;
    for (std::size_t n = NoteCs; n <= NoteB; ++n) onlyC.push_back({n, 0.0f});
    c.overrides = {
        {QuantumKey, onlyC},
        {Range, {}, 300, 20},
        {Link, {{PitchShift, 5}}},
    };
    c.presetCount = ironLockPresets().size();
    c.loadPreset = [](IronLock& e, std::size_t i) { e.loadPreset(i); };
    c.presetName = [](std::size_t i) { return ironLockPresets()[i].name.data(); };
    runSuite(c);
    testAccuracy();
}
