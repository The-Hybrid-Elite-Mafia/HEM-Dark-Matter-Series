#pragma once
// Minimal test harness for the Dark Matter Series. Every check prints its measured number.
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace dmt {

inline int gFails = 0;
inline int gChecks = 0;

inline std::atomic<bool> gTrackAllocs{false};
inline std::atomic<std::uint64_t> gAllocCount{0};

struct AllocScope {
    AllocScope() noexcept { gAllocCount.store(0); gTrackAllocs.store(true); }
    ~AllocScope() { gTrackAllocs.store(false); }
    [[nodiscard]] std::uint64_t count() const noexcept { return gAllocCount.load(); }
};

inline void check(bool ok, const char* what, double measured, const char* unit = "") {
    ++gChecks;
    if (!ok) ++gFails;
    std::printf("  [%s] %-62s %12.4f %s\n", ok ? "PASS" : "FAIL", what, measured, unit);
}

inline void section(const char* name) { std::printf("\n== %s ==\n", name); }

// ---- Signal helpers -----------------------------------------------------------------------------------
struct Stereo {
    std::vector<float> l, r;
    explicit Stereo(std::size_t n = 0) : l(n, 0.0f), r(n, 0.0f) {}
    [[nodiscard]] std::size_t size() const noexcept { return l.size(); }
};

inline double rms(const std::vector<float>& v, std::size_t from = 0, std::size_t to = 0) {
    if (to == 0 || to > v.size()) to = v.size();
    double s = 0;
    for (std::size_t i = from; i < to; ++i) s += double(v[i]) * v[i];
    return std::sqrt(s / double(std::max<std::size_t>(1, to - from)));
}
inline double peak(const std::vector<float>& v) {
    double p = 0;
    for (float x : v) p = std::max(p, double(std::fabs(x)));
    return p;
}
inline double db(double g) { return 20.0 * std::log10(std::max(g, 1e-12)); }
inline bool allFinite(const Stereo& s) {
    for (std::size_t i = 0; i < s.size(); ++i)
        if (!std::isfinite(s.l[i]) || !std::isfinite(s.r[i])) return false;
    return true;
}
inline double diffRms(const Stereo& a, const Stereo& b, std::size_t from = 0) {
    double s = 0;
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = from; i < n; ++i) {
        const double dl = double(a.l[i]) - b.l[i], dr = double(a.r[i]) - b.r[i];
        s += dl * dl + dr * dr;
    }
    return std::sqrt(s / double(std::max<std::size_t>(1, 2 * (n - from))));
}

// Vocal-like test signal: gliding harmonic voice with syllable envelope, sibilant noise bursts,
// and quiet gaps at -70 dBFS (so the gate has something to close on). Peak about `peakDb`.
inline Stereo vocalSignal(float sr, double seconds, double peakDb = -6.0) {
    const std::size_t n = static_cast<std::size_t>(sr * seconds);
    Stereo s(n);
    std::uint32_t seed = 12345u;
    auto rnd = [&]() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (int32_t(seed) / 2147483648.0); };
    double ph = 0, hpState = 0, prevN = 0;
    const double twoPi = 6.283185307179586;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = double(i) / sr;
        const double cycle = std::fmod(t, 1.0);
        const double f0 = 160.0 + 70.0 * std::sin(twoPi * 0.3 * t);
        ph += twoPi * f0 / sr;
        double voice = 0;
        for (int h = 1; h <= 12; ++h) voice += std::sin(ph * h) / (h * 1.2);
        // syllables 0.0-0.55 s voiced, 0.55-0.7 sibilant, 0.7-1.0 quiet gap
        double env = 0, sib = 0;
        if (cycle < 0.55) env = std::sin(3.14159265 * cycle / 0.55) * (0.6 + 0.4 * std::sin(twoPi * 4.0 * t));
        const double nz = rnd();
        const double hp = nz - prevN + 0.6 * hpState; // crude high-pass for "sss"
        prevN = nz; hpState = hp;
        if (cycle >= 0.55 && cycle < 0.7) sib = hp * 0.5 * std::sin(3.14159265 * (cycle - 0.55) / 0.15);
        const double floorNoise = nz * 0.0003; // about -70 dBFS
        const double x = 0.35 * env * voice + sib + floorNoise;
        s.l[i] = static_cast<float>(x);
        s.r[i] = static_cast<float>(x * 0.95 + 0.02 * std::sin(ph * 2.0) * env);
    }
    const double p = std::max(peak(s.l), peak(s.r));
    const double g = std::pow(10.0, peakDb / 20.0) / p;
    for (std::size_t i = 0; i < n; ++i) { s.l[i] = float(s.l[i] * g); s.r[i] = float(s.r[i] * g); }
    return s;
}

inline Stereo sine(float sr, double seconds, double hz, double amp) {
    const std::size_t n = static_cast<std::size_t>(sr * seconds);
    Stereo s(n);
    for (std::size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = float(amp * std::sin(6.283185307179586 * hz * double(i) / sr));
    return s;
}

// Run any engine with process(float*, float*, int) over a signal in host-sized blocks.
template <class Engine>
Stereo render(Engine& e, const Stereo& in, int block = 256) {
    Stereo out = in;
    for (std::size_t pos = 0; pos < out.size(); pos += std::size_t(block)) {
        const int n = int(std::min<std::size_t>(std::size_t(block), out.size() - pos));
        e.process(out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}

// Same, with an external sidechain key fed alongside each block.
template <class Engine>
Stereo renderKeyed(Engine& e, const Stereo& in, const Stereo& key, int block = 256) {
    Stereo out = in;
    for (std::size_t pos = 0; pos < out.size(); pos += std::size_t(block)) {
        const int n = int(std::min<std::size_t>(std::size_t(block), out.size() - pos));
        e.setSidechain(key.l.data() + pos, key.r.data() + pos);
        e.process(out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}
// Sidechain key: a 100 Hz tone pumping on and off 4 times a second at about -6 dBFS (nothing like the
// test vocal / mix, so a detector that follows it behaves visibly differently).
inline Stereo pulseKey(float sr, std::size_t n) {
    Stereo k(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = double(i) / sr;
        const double gate = std::fmod(t, 0.25) < 0.1 ? 1.0 : 0.0;
        const float v = float(0.5 * gate * std::sin(6.283185307179586 * 100.0 * t));
        k.l[i] = v; k.r[i] = v;
    }
    return k;
}

// K-weighted loudness of a stereo buffer in dB (same pre-filter as the engine's auto-gain).
double kLoudnessDb(const Stereo& s, float sr, std::size_t from = 0);

} // namespace dmt
