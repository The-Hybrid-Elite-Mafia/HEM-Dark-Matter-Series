#pragma once
// =====================================================================================================
//  HEM IRON LOCK - pitch correction, pitch shift and formant engine.
//  Detection: YIN on a 2x-decimated mono sum, every 128 samples.
//  Correction: centre pitch (smoothed by SUSTAIN, so vibrato survives) -> nearest note allowed by the
//              12-note scale lock (or QUANTUM KEY's detected scale) within RANGE -> correction glides at
//              TRANSITION speed and is scaled by STRENGTH. PITCH SHIFT adds whole semitones.
//  Resynthesis: pitch-synchronous overlap-add (PSOLA). Grains are taken one period wide around analysis
//              epochs and re-spaced at the new period, so formants stay put; FORMANT re-samples each grain
//              (LINK makes formants follow PITCH SHIFT).
//  Latency: REALTIME tracks down to 100 Hz (40 ms at 48 kHz); HQ tracks down to 60 Hz (67 ms).
//  GROUPS: instances with GROUPS on share one scale (the last instance to change it wins).
// =====================================================================================================
#include "dm/core/EngineShell.hpp"

#include <vector>

namespace dm::il {

namespace a = dm::assumed::il;
inline constexpr const char* kModeNames[2] = {"REALTIME", "HQ"};
inline constexpr const char* kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

enum P : std::size_t {
    Groups = G_Count, Quality, QuantumKey,
    NoteC, NoteCs, NoteD, NoteDs, NoteE, NoteF, NoteFs, NoteG, NoteGs, NoteA, NoteAs, NoteB,
    Strength, Transition, Sustain, Range, PitchShift, Formant, Link, Mix,
    kParamCount
};

inline constexpr std::array<ParamDesc, std::size_t(kParamCount) - std::size_t(G_Count)> kOwnParams = {{
    {"groups", "GROUPS", 0, 1, 0, "toggle"},
    {"quality", "REALTIME / HQ", 0, 1, 0, "", std::span<const char* const>(kModeNames)},
    {"quantum_key", "QUANTUM KEY", 0, 1, 0, "toggle"},
    {"note_c", "C", 0, 1, 1, "toggle"}, {"note_cs", "C#", 0, 1, 0, "toggle"}, {"note_d", "D", 0, 1, 1, "toggle"},
    {"note_ds", "D#", 0, 1, 0, "toggle"}, {"note_e", "E", 0, 1, 1, "toggle"}, {"note_f", "F", 0, 1, 1, "toggle"},
    {"note_fs", "F#", 0, 1, 0, "toggle"}, {"note_g", "G", 0, 1, 1, "toggle"}, {"note_gs", "G#", 0, 1, 0, "toggle"},
    {"note_a", "A", 0, 1, 1, "toggle"}, {"note_as", "A#", 0, 1, 0, "toggle"}, {"note_b", "B", 0, 1, 1, "toggle"},
    {"strength", "STRENGTH", 0, 100, a::kDefStrength, "%"},
    {"transition", "TRANSITION", 0, 400, a::kDefTransitionMs, "ms"},
    {"sustain", "SUSTAIN", 0, 100, a::kDefSustain, "%"},
    {"range", "RANGE", 0, 600, a::kDefRangeCents, "cents"},
    {"pitch_shift", "PITCH SHIFT", -24, 24, 0, "st"},
    {"formant", "FORMANT", -12, 12, 0, "st"},
    {"link", "LINK", 0, 1, 0, "toggle"},
    {"mix", "MIX", 0, 100, 100, "%"},
}};
inline constexpr auto kParams = concatAll(kGlobalParams, kOwnParams);

std::span<const Preset> ironLockPresets() noexcept;

// Process-wide scale bus for GROUPS.
struct GroupBus {
    std::atomic<std::uint32_t> mask{0};
    std::atomic<std::uint32_t> version{0};
    static GroupBus& instance() noexcept { static GroupBus b; return b; }
};

class IronLock : public EngineShell<IronLock, kParamCount> {
public:
    IronLock() : EngineShell(kParams) {}
    bool loadPreset(std::size_t i) noexcept { const auto p = ironLockPresets(); return i < p.size() && loadPresetValues(p[i].values); }

    // Readouts (UI thread)
    [[nodiscard]] float detectedHz() const noexcept { return hzPub_.load(std::memory_order_relaxed); }
    [[nodiscard]] float correctionCents() const noexcept { return corrPub_.load(std::memory_order_relaxed); }
    [[nodiscard]] int detectedKey() const noexcept { return keyPub_.load(std::memory_order_relaxed); } // 0..11 major, 12..23 minor, -1 none
    [[nodiscard]] float noteConfidence(int pc) const noexcept { return conf_[static_cast<std::size_t>(pc)].load(std::memory_order_relaxed); }
    [[nodiscard]] std::uint32_t activeScaleMask() const noexcept { return maskPub_.load(std::memory_order_relaxed); }

    void prepareCore(float sr);
    void resetCore() noexcept;
    void updateCore(const float* v, const float* old, bool force) noexcept;
    void processCore(float& l, float& r) noexcept;
    [[nodiscard]] int coreLatency() const noexcept { return hq_ ? latHq_ : latRt_; }
    [[nodiscard]] int maxCoreLatency() const noexcept { return latHq_; }
    // Latency the current REALTIME / HQ setting gives, read straight from the parameter (a plug-in host
    // can ask before the next process() call has applied the change).
    [[nodiscard]] int latencyFromParams() const noexcept {
        return latencySamples() - coreLatency() + (params().get(Quality) >= 0.5f ? latHq_ : latRt_);
    }
    [[nodiscard]] bool coreActive() const noexcept { return active_; }
    void afterChunk(int n) noexcept;

private:
    void analyse() noexcept;
    void placeGrain(double ts, double ta, double t0, double f) noexcept;
    void updateKeyDetect() noexcept;
    void publish(std::uint32_t m) noexcept;
    [[nodiscard]] float targetCorrection(float midi) const noexcept;
    [[nodiscard]] float inAt(const std::vector<float>& ring, double t) const noexcept;

    static constexpr int kRing = 32768;
    std::vector<float> inL_, inR_, outL_, outR_, wsum_, dec_, diff_;
    std::int64_t tIn_ = 0;
    int decW_ = 0;
    std::uint32_t decWrite_ = 0;
    OnePole decLp_{};
    float decPrev_ = 0;
    int latRt_ = 0, latHq_ = 0, pmaxRt_ = 0, pmaxHq_ = 0;
    bool hq_ = false, active_ = true, groups_ = false, quantum_ = false, link_ = false;
    std::uint32_t ownMask_ = 0, mask_ = 0, busVersion_ = 0, groupMask_ = 0, lastOwn_ = 0;
    bool haveGroup_ = false;
    float strength_ = 0.8f, transitionCoef_ = 0, sustainCoef_ = 0, rangeSemi_ = 3, shift_ = 0, formant_ = 0, mix_ = 1;
    double t0_ = 480, nextTs_ = 0, epoch_ = 0;
    bool voiced_ = false;
    float centerMidi_ = 60, corr_ = 0, curCorr_ = 0;
    Smoother engage_{};
    int hopCount_ = 0;
    std::array<double, 12> hist_{};
    double histDecay_ = 1.0;
    int keyCountdown_ = 0;
    int detectedKey_ = -1;
    std::atomic<float> hzPub_{0}, corrPub_{0};
    std::atomic<int> keyPub_{-1};
    std::atomic<std::uint32_t> maskPub_{0};
    std::array<std::atomic<float>, 12> conf_{};
};

} // namespace dm::il
