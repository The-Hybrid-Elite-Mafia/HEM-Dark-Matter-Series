#pragma once
// Shared quality-gate suite for every EngineShell-based plug-in (protocol section 4).
#include "TestHarness.hpp"

#include "dm/core/EngineShell.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace dmt {

using Ctx = std::vector<std::pair<std::size_t, float>>;

struct WiredOverride {
    std::size_t id;
    Ctx ctx;                 // context applied before the A/B values
    float a = NAN, b = NAN;  // explicit A/B values (NAN = default vs max/min/next choice)
};
struct ModeCheck { std::size_t id; Ctx ctx; const char* name; };

template <class E>
struct SuiteCfg {
    const char* name = "";
    std::function<std::unique_ptr<E>(float)> make;
    std::function<void(E&)> allOff;            // state in which no DSP may run (dry must pass bit-exact)
    std::vector<WiredOverride> overrides;      // per-control context / values
    std::vector<std::size_t> skipWired;        // controls proven elsewhere (printed)
    std::vector<ModeCheck> modes;
    bool vocal = true;                         // test material: vocal or full mix
    double levelTol = 1.0;
    std::size_t presetCount = 0;
    std::function<void(E&, std::size_t)> loadPreset;
    std::function<const char*(std::size_t)> presetName;
    Ctx activeCtx;
    std::function<Stereo(float, std::size_t)> keySignal;      // when set: every-control renders also feed this sidechain key
    std::function<void(E&)> colour;                           // extra settings for the coloured starting point (after presetBase)
    std::function<Stereo(float, double, double)> customSignal; // overrides vocal/music material when set                             // settings that make the engine do something (for bypass/autogain checks)
};

// Full-mix test signal: kick, snare, hats, bass and a chord pad at 120 BPM, peaks about `peakDb`.
inline Stereo musicSignal(float sr, double seconds, double peakDb = -6.0) {
    const std::size_t n = std::size_t(sr * seconds);
    Stereo s(n);
    std::uint32_t seed = 4242u;
    auto rnd = [&]() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (int32_t(seed) / 2147483648.0); };
    const double twoPi = 6.283185307179586, beat = 0.5;
    const double bassNotes[4] = {55.0, 55.0, 65.41, 49.0};
    const double chord[3] = {220.0, 261.63, 329.63};
    double kPh = 0, bPh = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = double(i) / sr, tb = std::fmod(t, beat);
        const int beatIdx = int(t / beat);
        const double kf = 50.0 + 120.0 * std::exp(-tb * 30.0);
        kPh += twoPi * kf / sr;
        const double kick = std::sin(kPh) * std::exp(-tb * 9.0);
        const double sn = (beatIdx % 2 == 1) ? rnd() * std::exp(-tb * 18.0) * 0.6 : 0.0;
        const double th = std::fmod(t, beat / 2.0);
        const double hat = rnd() * std::exp(-th * 60.0) * 0.15 * ((i & 1) ? 1.0 : -1.0);
        const double bf = bassNotes[(beatIdx / 4) % 4];
        bPh += twoPi * bf / sr;
        double bass = 0;
        for (int h = 1; h <= 6; ++h) bass += std::sin(bPh * h) / h;
        bass *= 0.35 * (0.6 + 0.4 * std::exp(-tb * 4.0));
        double pad = 0;
        for (double f : chord) pad += std::sin(twoPi * f * t) + 0.3 * std::sin(twoPi * f * 2.0 * t + 0.4);
        pad *= 0.08;
        const double m = kick * 0.9 + bass + sn + hat;
        s.l[i] = float(m + pad * 1.1);
        s.r[i] = float(m * 0.97 + pad * 0.9 + hat * 0.3);
    }
    const double p = std::max(peak(s.l), peak(s.r));
    const double g = std::pow(10.0, peakDb / 20.0) / p;
    for (std::size_t i = 0; i < n; ++i) { s.l[i] = float(s.l[i] * g); s.r[i] = float(s.r[i] * g); }
    return s;
}

inline Stereo delayedBy(const Stereo& in, int lat) {
    Stereo d(in.size());
    for (std::size_t i = std::size_t(lat); i < in.size(); ++i) { d.l[i] = in.l[i - std::size_t(lat)]; d.r[i] = in.r[i - std::size_t(lat)]; }
    return d;
}

// Steven's zero-colour rule: every plug-in LOADS with its colour at zero and passes the sound untouched.
// The quality checks below still run on the old (coloured) starting point - the engine's presetBase(), the
// values the factory presets were written over - so every control, limiter and meter is still exercised.
template <class E>
void applyColouredBase(E& e) {
    for (const auto& v : e.presetBase()) {
        const int i = e.params().indexOf(v.id);
        if (i >= 0) e.params().set(static_cast<std::size_t>(i), v.value);
    }
    e.reset();
}

template <class E>
void runSuite(const SuiteCfg<E>& cIn) {
    constexpr float sr = 48000.0f;
    SuiteCfg<E> c = cIn;
    c.make = [raw = cIn.make, col = cIn.colour](float rate) {
        auto e = raw(rate);
        applyColouredBase(*e);
        if (col) { col(*e); e->reset(); }
        return e;
    };
    const double ceiling = std::pow(10.0, -0.1 / 20.0);
    auto signalAt = [&](float rate, double secs, double pk) {
        if (c.customSignal) return c.customSignal(rate, secs, pk);
        return c.vocal ? vocalSignal(rate, secs, pk) : musicSignal(rate, secs, pk);
    };
    auto signal = [&](double secs, double pk) { return signalAt(sr, secs, pk); };
    std::printf("\n################ %s ################\n", c.name);

    // ---- Unity / CPU hygiene -------------------------------------------------------------------------
    section("Unity gain: processing OFF (dry must pass bit-exact, latency aligned)");
    {
        auto e = c.make(sr);
        c.allOff(*e);
        e->reset();
        const auto in = signal(3.0, -6.0);
        const auto out = render(*e, in);
        const auto ref = delayedBy(in, e->latencySamples());
        double maxErr = 0;
        for (std::size_t i = 0; i < in.size(); ++i)
            maxErr = std::max({maxErr, double(std::fabs(out.l[i] - ref.l[i])), double(std::fabs(out.r[i] - ref.r[i]))});
        check(maxErr <= 1e-7, "max |out - dry| with processing off", maxErr);
        check(!e->ranCore(), "core DSP skipped while off (CPU hygiene)", e->ranCore() ? 1 : 0);
        check(true, "reported latency (samples)", e->latencySamples());
    }
    section("Global BYPASS");
    {
        auto e = c.make(sr);
        e->params().set(dm::G_Bypass, 1);
        e->reset();
        const auto in = signal(2.0, -3.0);
        const auto out = render(*e, in);
        const auto ref = delayedBy(in, e->latencySamples());
        double maxErr = 0;
        for (std::size_t i = 4800; i < in.size(); ++i) maxErr = std::max(maxErr, double(std::fabs(out.l[i] - ref.l[i])));
        check(maxErr == 0.0, "bypassed output == latency-aligned input", maxErr);
    }

    section("Loads untouched: every colour control at zero on load (Steven's rule)");
    {
        for (double pk : {-6.0, -1.0, 0.0}) {
            auto e = cIn.make(sr);   // the plug-in exactly as a host loads it
            e->reset();
            const auto in = signal(4.0, pk);
            const auto out = render(*e, in);
            const auto ref = delayedBy(in, e->latencySamples());
            double maxErr = 0;
            for (std::size_t i = 0; i < in.size(); ++i)
                maxErr = std::max({maxErr, double(std::fabs(out.l[i] - ref.l[i])), double(std::fabs(out.r[i] - ref.r[i]))});
            check(maxErr == 0.0, ("on load, " + std::to_string(int(pk)) + " dBFS peaks: max |out - aligned input|").c_str(), maxErr);
        }
        // and the colour still comes in when a control is turned up (the coloured starting point differs)
        auto plain = cIn.make(sr), coloured = c.make(sr);
        const auto in = signal(3.0, -6.0);
        const auto a = render(*plain, in), b = render(*coloured, in);
        const double dr = diffRms(a, b);
        bool hasBase = false;
        for (const auto& v : plain->presetBase()) hasBase |= plain->params().indexOf(v.id) >= 0;
        if (hasBase) check(dr > 1e-5, "turning the colour up changes the sound (diff RMS dBFS)", db(dr), "dBFS");
    }

    // ---- Default load + presets ----------------------------------------------------------------------
    section("Default load + factory presets: no clip, level within tolerance");
    auto levelCheck = [&](E& e, const std::string& label) {
        const auto in = signal(8.0, -6.0);
        const auto out = render(e, in);
        const std::size_t from = std::size_t(sr * 3.0);
        const double d = kLoudnessDb(out, sr, from) - kLoudnessDb(in, sr, from);
        auto st = [&](const Stereo& s) { return 0.5 * (db(rms(s.l, from)) + db(rms(s.r, from))); };
        const double dp = st(out) - st(in);
        const double pk = std::max(peak(out.l), peak(out.r));
        check(std::fabs(d) <= c.levelTol, (label + ": K-weighted level change (dB)").c_str(), d, "dB");
        check(std::fabs(dp) <= c.levelTol, (label + ": plain RMS level change (dB)").c_str(), dp, "dB");
        check(pk <= ceiling + 1e-6 && allFinite(out), (label + ": peak (dBFS)").c_str(), db(pk), "dBFS");
    };
    { auto e = c.make(sr); levelCheck(*e, "defaults"); }
    {
        auto e = c.make(sr);
        const auto hot = signal(4.0, 0.0);
        const auto ho = render(*e, hot);
        const auto href = delayedBy(hot, e->latencySamples());
        double pk = 0;   // every sample is either held under the ceiling or the untouched input itself
        for (std::size_t i = 0; i < ho.size(); ++i)
            for (int ch = 0; ch < 2; ++ch) {
                const float o = ch ? ho.r[i] : ho.l[i], x = ch ? href.r[i] : href.l[i];
                if (o != x) pk = std::max(pk, double(std::fabs(o)));
            }
        check(pk <= ceiling + 1e-6, "defaults with 0 dBFS input: peak of processed samples (dBFS)", db(pk), "dBFS");
    }
    for (std::size_t p = 0; p < c.presetCount; ++p) {
        auto e = c.make(sr);
        c.loadPreset(*e, p);
        e->reset();
        levelCheck(*e, std::string("preset ") + c.presetName(p));
    }

    // ---- Extreme sweep -------------------------------------------------------------------------------
    section("Extreme parameter sweep, +12 dB over-driven input: never above -0.1 dBFS, never NaN");
    {
        auto e = c.make(sr);
        std::uint32_t seed = 991u;
        auto rnd = [&]() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return double(seed) / 4294967296.0; };
        auto in = signal(15.0, 12.0);
        Stereo out = in;
        const auto& P = e->params();
        for (std::size_t pos = 0; pos < out.size(); pos += 256) {
            if ((pos / 256) % 20 == 0) {
                for (std::size_t i = 0; i < P.size(); ++i) {
                    if (i == dm::G_Bypass) continue;
                    const auto& d = P.desc(i);
                    const double r = rnd();
                    e->params().set(i, float(d.min + (d.max - d.min) * (r < 0.3 ? (rnd() < 0.5 ? 0.0 : 1.0) : rnd())));
                }
            }
            e->process(out.l.data() + pos, out.r.data() + pos, int(std::min<std::size_t>(256, out.size() - pos)));
        }
        const auto sref = delayedBy(in, e->latencySamples());
        // never louder than the ceiling, except where the over-driven input itself passes untouched (all colour at zero,
        // and the 10 ms crossfade out of that): no sample may ever be louder than both the ceiling and the input
        double over = 0, pk = 0;
        for (std::size_t i = 0; i < out.size(); ++i)
            for (int ch = 0; ch < 2; ++ch) {
                const double o = std::fabs(ch ? out.r[i] : out.l[i]), x = std::fabs(ch ? sref.r[i] : sref.l[i]);
                over = std::max(over, o - std::max(ceiling, x));
                if (o > x) pk = std::max(pk, o);
            }
        check(over <= 1e-6, "sweep: no sample louder than both -0.1 dBFS and the input (excess)", over);
        check(pk <= ceiling + 1e-6, "max output peak over the sweep where louder than the input (dBFS)", db(std::max(pk, 1e-12)), "dBFS");
        check(allFinite(out), "all samples finite", 1);
    }

    // ---- Every control wired -------------------------------------------------------------------------
    section("Every control changes the audio (auto-gain off)");
    {
        const auto in = signal(3.0, -6.0);
        auto proto = c.make(sr);
        auto renderWith = [&](const Ctx& ctx, std::size_t id, float value) {
            auto e = c.make(sr);
            e->params().set(dm::G_AutoGain, 0);
            for (auto [k, v] : ctx) e->params().set(k, v);
            e->params().set(id, value);
            e->reset();
            if constexpr (requires(E& x) { x.setSidechain(nullptr, nullptr); })
                if (c.keySignal) return renderKeyed(*e, in, c.keySignal(sr, in.size()));
            return render(*e, in);
        };
        for (std::size_t i = 0; i < proto->params().size(); ++i) {
            if (i == dm::G_AutoGain) continue;
            const auto& d = proto->params().desc(i);
            bool skip = false;
            for (auto s : c.skipWired) skip |= (s == i);
            if (skip) { std::printf("  [ -- ] %-62s (proven in its own test)\n", std::string(d.id).c_str()); continue; }
            Ctx ctx;
            float a = d.def, b = (d.def == d.max) ? d.min : d.max;
            if (d.isChoice()) b = float((int(d.def) + 1) % int(d.choices.size()));
            if (i == dm::G_Bypass) ctx = c.activeCtx;
            for (const auto& o : c.overrides)
                if (o.id == i) { ctx = o.ctx; if (!std::isnan(o.a)) a = o.a; if (!std::isnan(o.b)) b = o.b; }
            const double dr = diffRms(renderWith(ctx, i, a), renderWith(ctx, i, b));
            check(dr > 1e-5, (std::string(d.id) + " changes output (diff RMS dBFS)").c_str(), db(dr), "dBFS");
        }
        // AUTO GAIN itself: compare on vs off with the engine's default processing.
        auto on = c.make(sr), off = c.make(sr);
        off->params().set(dm::G_AutoGain, 0);
        off->params().set(dm::G_Input, 12);
        on->params().set(dm::G_Input, 12);
        for (auto [k, v] : c.activeCtx) { on->params().set(k, v); off->params().set(k, v); }
        on->reset(); off->reset();
        const auto ao = render(*on, in), af = render(*off, in);
        check(diffRms(ao, af) > 1e-5, "autogain changes output with +12 dB input trim (diff RMS dBFS)", db(diffRms(ao, af)), "dBFS");
    }

    // ---- Modes distinct ------------------------------------------------------------------------------
    if (!c.modes.empty()) section("Every mode differs from every other mode");
    for (const auto& m : c.modes) {
        const auto in = signal(2.0, -6.0);
        auto proto = c.make(sr);
        const auto& d = proto->params().desc(m.id);
        std::vector<Stereo> outs;
        for (std::size_t k = 0; k < d.choices.size(); ++k) {
            auto e = c.make(sr);
            e->params().set(dm::G_AutoGain, 0);
            for (auto [id, v] : m.ctx) e->params().set(id, v);
            e->params().set(m.id, float(k));
            e->reset();
            outs.push_back(render(*e, in));
        }
        double minDiff = 1e9; std::size_t wa = 0, wb = 0;
        for (std::size_t a = 0; a < outs.size(); ++a)
            for (std::size_t b = a + 1; b < outs.size(); ++b) {
                const double dr = diffRms(outs[a], outs[b]);
                if (dr < minDiff) { minDiff = dr; wa = a; wb = b; }
            }
        std::printf("  closest pair in %s: %s vs %s\n", m.name, d.choices[wa], d.choices[wb]);
        check(minDiff > 1e-4, (std::string(m.name) + ": smallest difference between two modes (dBFS)").c_str(), db(minDiff), "dBFS");
    }

    // ---- Zero allocation -----------------------------------------------------------------------------
    section("Zero heap allocation on the audio thread (random control changes every block)");
    {
        auto e = c.make(sr);
        auto in = signal(3.0, -6.0);
        std::uint32_t seed = 31337u;
        auto rnd = [&]() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return double(seed) / 4294967296.0; };
        std::uint64_t count = 0;
        {
            AllocScope scope;
            for (std::size_t pos = 0; pos < in.size(); pos += 256) {
                const std::size_t i = std::size_t(rnd() * double(e->params().size())) % e->params().size();
                if (i != dm::G_Bypass) {
                    const auto& d = e->params().desc(i);
                    e->params().set(i, float(d.min + (d.max - d.min) * rnd()));
                }
                e->process(in.l.data() + pos, in.r.data() + pos, int(std::min<std::size_t>(256, in.size() - pos)));
            }
            count = scope.count();
        }
        check(count == 0, "heap allocations during process()", double(count));
    }

    // ---- CPU -----------------------------------------------------------------------------------------
    section("CPU");
    {
        const auto in = signal(10.0, -6.0);
        auto timeIt = [&](bool on) {
            auto e = c.make(sr);
            if (!on) c.allOff(*e);
            e->reset();
            Stereo o = in;
            const auto t0 = std::chrono::steady_clock::now();
            for (std::size_t p = 0; p < o.size(); p += 256)
                e->process(o.l.data() + p, o.r.data() + p, int(std::min<std::size_t>(256, o.size() - p)));
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        };
        const double on = timeIt(true), off = timeIt(false);
        check(true, "10 s of audio, defaults (ms)", on, "ms");
        check(true, "10 s of audio, processing off (ms)", off, "ms");
        check(on < 10000.0 * 0.25, "defaults realtime load (fraction of one core)", on / 10000.0);
    }

    // ---- Sample rates --------------------------------------------------------------------------------
    section("Sample rates 44.1 / 96 kHz: defaults safe");
    for (float rate : {44100.0f, 96000.0f}) {
        auto e = c.make(rate);
        const auto in = signalAt(rate, 6.0, -6.0);
        const auto out = render(*e, in, 480);
        const std::size_t from = std::size_t(rate * 3.0);
        const double d = kLoudnessDb(out, rate, from) - kLoudnessDb(in, rate, from);
        const double pk = std::max(peak(out.l), peak(out.r));
        char label[96];
        std::snprintf(label, sizeof label, "%.1f kHz: level change (dB)", rate / 1000.0);
        check(std::fabs(d) <= c.levelTol && allFinite(out), label, d, "dB");
        std::snprintf(label, sizeof label, "%.1f kHz: peak (dBFS)", rate / 1000.0);
        check(pk <= ceiling + 1e-6, label, db(pk), "dBFS");
    }
}

} // namespace dmt
