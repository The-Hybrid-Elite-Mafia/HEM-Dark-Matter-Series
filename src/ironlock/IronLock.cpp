// HEM IRON LOCK - engine + factory presets.
#include "dm/ironlock/IronLock.hpp"

namespace dm::il {

namespace {
constexpr int kMask = 32768 - 1;
constexpr int kDecRing = 8192;
constexpr int kMajor[7] = {0, 2, 4, 5, 7, 9, 11};
constexpr int kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
std::uint32_t scaleMask(int key) noexcept {
    std::uint32_t m = 0;
    const int root = key % 12;
    const int* iv = key < 12 ? kMajor : kMinor;
    for (int i = 0; i < 7; ++i) m |= 1u << ((root + iv[i]) % 12);
    return m;
}
} // namespace

void IronLock::prepareCore(float sr) {
    sr_ = sr;
    pmaxRt_ = static_cast<int>(std::ceil(sr_ / a::kRealtimeMinF0));
    pmaxHq_ = static_cast<int>(std::ceil(sr_ / a::kHqMinF0));
    latRt_ = 4 * pmaxRt_ + a::kHop;
    latHq_ = 4 * pmaxHq_ + a::kHop;
    for (auto* v : {&inL_, &inR_, &outL_, &outR_, &wsum_}) v->assign(kRing, 0.0f);
    dec_.assign(kDecRing, 0.0f);
    diff_.assign(static_cast<std::size_t>(pmaxHq_ / 2 + 4), 0.0f);
    decLp_.setCutoff(0.2f * sr_, sr_);
    engage_.prepare(10.0f / 4.6f, sr_);
    histDecay_ = std::exp(-static_cast<double>(a::kHop) / (sr_ * a::kKeyMemorySec));
    for (auto& c : conf_) c.store(0.0f);
}

void IronLock::resetCore() noexcept {
    for (auto* v : {&inL_, &inR_, &outL_, &outR_, &wsum_}) std::fill(v->begin(), v->end(), 0.0f);
    std::fill(dec_.begin(), dec_.end(), 0.0f);
    tIn_ = 0; decWrite_ = 0; decPrev_ = 0; decLp_.reset();
    t0_ = a::kUnvoicedGrainMs * 0.001 * sr_;
    nextTs_ = 0; epoch_ = 0; voiced_ = false;
    centerMidi_ = 60; corr_ = curCorr_ = 0;
    engage_.snap(0.0f);
    hopCount_ = 0;
    hist_.fill(0.0);
    keyCountdown_ = 0;
    detectedKey_ = -1;
}

void IronLock::updateCore(const float* v, const float* old, bool) noexcept {
    if (!changed(v, old, {Groups, Quality, QuantumKey, NoteC, NoteCs, NoteD, NoteDs, NoteE, NoteF, NoteFs, NoteG, NoteGs,
                          NoteA, NoteAs, NoteB, Strength, Transition, Sustain, Range, PitchShift, Formant, Link, Mix}))
        return;
    groups_ = v[Groups] >= 0.5f;
    hq_ = v[Quality] >= 0.5f;
    quantum_ = v[QuantumKey] >= 0.5f;
    std::uint32_t m = 0;
    for (int n = 0; n < 12; ++n) if (v[NoteC + static_cast<std::size_t>(n)] >= 0.5f) m |= 1u << n;
    const bool scaleChanged = !old || m != ownMask_ || (old && v[QuantumKey] != old[QuantumKey]);
    ownMask_ = m;
    strength_ = v[Strength] * 0.01f;
    transitionCoef_ = onePoleCoef(v[Transition], sr_);
    sustainCoef_ = std::exp(-static_cast<float>(a::kHop) /
                            (sr_ * 0.001f * expMap(v[Sustain] * 0.01f, a::kSustainMinMs, a::kSustainMaxMs)));
    rangeSemi_ = v[Range] * 0.01f;
    shift_ = v[PitchShift];
    formant_ = v[Formant];
    link_ = v[Link] >= 0.5f;
    mix_ = v[Mix] * 0.01f;
    active_ = mix_ > 0.0f && (strength_ > 0.0f || shift_ != 0.0f || formant_ != 0.0f);
    const std::uint32_t own = (quantum_ && detectedKey_ >= 0) ? scaleMask(detectedKey_) : ownMask_;
    if (groups_ && scaleChanged && old) publish(own); // this instance changed the scale: share it
    if (!groups_) haveGroup_ = false;
    mask_ = (groups_ && haveGroup_) ? groupMask_ : own;
    lastOwn_ = own;
}

float IronLock::inAt(const std::vector<float>& ring, double t) const noexcept {
    const double fl = std::floor(t);
    const auto i = static_cast<std::int64_t>(fl);
    const float frac = static_cast<float>(t - fl);
    const float x0 = ring[static_cast<std::size_t>(i & kMask)], x1 = ring[static_cast<std::size_t>((i + 1) & kMask)];
    return x0 + (x1 - x0) * frac;
}

float IronLock::targetCorrection(float midi) const noexcept {
    const int base = static_cast<int>(std::lround(midi));
    int best = base;
    if (mask_ != 0) {
        float bestD = 1e9f;
        for (int k = -12; k <= 12; ++k) {
            const int n = base + k;
            if (!(mask_ & (1u << (((n % 12) + 12) % 12)))) continue;
            const float d = std::fabs(static_cast<float>(n) - midi);
            if (d < bestD) { bestD = d; best = n; }
        }
    }
    const float diff = static_cast<float>(best) - midi;
    return std::fabs(diff) <= rangeSemi_ ? diff : 0.0f;
}

void IronLock::analyse() noexcept {
    const int pmax = hq_ ? pmaxHq_ : pmaxRt_;
    const int tauMax = pmax / 2;                       // decimated domain
    const int tauMin = std::max(2, static_cast<int>(sr_ * 0.5f / a::kMaxF0));
    const int W = tauMax;
    const std::uint32_t start = decWrite_ - static_cast<std::uint32_t>(W + tauMax);
    auto x = [&](int i) { return dec_[(start + static_cast<std::uint32_t>(i)) & (kDecRing - 1)]; };

    double energy = 0;
    for (int j = 0; j < W; ++j) energy += double(x(j)) * x(j);
    const bool loud = energy / W > 1.0e-7; // about -70 dBFS
    int found = -1;
    if (loud) {
        float running = 0;
        diff_[0] = 1.0f;
        for (int tau = 1; tau <= tauMax; ++tau) {
            float d = 0;
            for (int j = 0; j < W; ++j) { const float e = x(j) - x(j + tau); d += e * e; }
            running += d;
            diff_[static_cast<std::size_t>(tau)] = running > 0 ? d * static_cast<float>(tau) / running : 1.0f;
        }
        for (int tau = tauMin; tau < tauMax; ++tau) {
            if (diff_[static_cast<std::size_t>(tau)] < a::kYinThreshold) {
                while (tau + 1 < tauMax && diff_[static_cast<std::size_t>(tau + 1)] < diff_[static_cast<std::size_t>(tau)]) ++tau;
                found = tau;
                break;
            }
        }
    }
    if (found > 0) {
        const float s0 = diff_[static_cast<std::size_t>(found - 1)], s1 = diff_[static_cast<std::size_t>(found)],
                    s2 = diff_[static_cast<std::size_t>(found + 1)];
        const float den = s0 - 2 * s1 + s2;
        const float tau = static_cast<float>(found) + (std::fabs(den) > 1e-9f ? 0.5f * (s0 - s2) / den : 0.0f);
        t0_ = 2.0 * tau;
        voiced_ = true;
        const float hz = sr_ / static_cast<float>(t0_);
        const float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
        centerMidi_ = midi + sustainCoef_ * (centerMidi_ - midi);
        if (std::fabs(centerMidi_ - midi) > 2.0f) centerMidi_ = midi; // new note: re-centre at once
        corr_ = targetCorrection(centerMidi_);
        const int pc = ((static_cast<int>(std::lround(midi)) % 12) + 12) % 12;
        hist_[static_cast<std::size_t>(pc)] += 1.0;
        hzPub_.store(hz, std::memory_order_relaxed);
    } else {
        voiced_ = false;
        corr_ = 0;
        t0_ = a::kUnvoicedGrainMs * 0.001 * sr_;
        hzPub_.store(0.0f, std::memory_order_relaxed);
    }
    for (auto& h : hist_) h *= histDecay_;
    if (--keyCountdown_ <= 0) {
        keyCountdown_ = static_cast<int>(a::kKeyUpdateSec * sr_ / a::kHop);
        updateKeyDetect();
    }
}

void IronLock::updateKeyDetect() noexcept {
    double total = 0, mx = 0;
    for (double h : hist_) { total += h; mx = std::max(mx, h); }
    for (std::size_t i = 0; i < 12; ++i) conf_[i].store(mx > 0 ? static_cast<float>(hist_[i] / mx) : 0.0f, std::memory_order_relaxed);
    if (total < 20.0) return; // not enough voiced material yet
    const double meanH = total / 12.0;
    double best = -2.0;
    int bestKey = -1;
    for (int k = 0; k < 24; ++k) {
        const float* prof = k < 12 ? a::kMajorProfile : a::kMinorProfile;
        double meanP = 0;
        for (int i = 0; i < 12; ++i) meanP += prof[i];
        meanP /= 12.0;
        double num = 0, dh = 0, dp = 0;
        for (int i = 0; i < 12; ++i) {
            const double h = hist_[static_cast<std::size_t>((i + k) % 12)] - meanH, p = prof[i] - meanP;
            num += h * p; dh += h * h; dp += p * p;
        }
        const double r = num / std::sqrt(std::max(dh * dp, 1e-12));
        if (r > best) { best = r; bestKey = k; }
    }
    detectedKey_ = bestKey;
    keyPub_.store(bestKey, std::memory_order_relaxed);
}

void IronLock::placeGrain(double ts, double ta, double t0, double f) noexcept {
    const int H = std::max(2, static_cast<int>(t0 / f));
    const double invH = 1.0 / H;
    const auto center = static_cast<std::int64_t>(std::llround(ts));
    for (int j = -H; j <= H; ++j) {
        const float w = 0.5f - 0.5f * std::cos(kPi * static_cast<float>((j + H) * invH));
        const double src = ta + static_cast<double>(j) * f;
        const auto idx = static_cast<std::size_t>((center + j) & kMask);
        outL_[idx] += w * inAt(inL_, src);
        outR_[idx] += w * inAt(inR_, src);
        wsum_[idx] += w;
    }
}

void IronLock::processCore(float& l, float& r) noexcept {
    const auto wi = static_cast<std::size_t>(tIn_ & kMask);
    inL_[wi] = l;
    inR_[wi] = r;
    // 2x decimation for detection.
    const float m = decLp_.lp(0.5f * (l + r));
    if (tIn_ & 1) { dec_[decWrite_ & (kDecRing - 1)] = 0.5f * (m + decPrev_); ++decWrite_; }
    decPrev_ = m;
    if (++hopCount_ >= a::kHop) { hopCount_ = 0; analyse(); }

    curCorr_ = corr_ + transitionCoef_ * (curCorr_ - corr_);
    const float semis = curCorr_ * strength_ + shift_;
    const double p = std::pow(2.0, semis / 12.0);
    const double f = std::pow(2.0, (formant_ + (link_ ? shift_ : 0.0f)) / 12.0);
    const double T0 = t0_;
    const double T1 = T0 / p;
    const double now = static_cast<double>(tIn_);
    if (nextTs_ < now - 4.0 * T0 - 8.0) nextTs_ = now - 1.5 * T0; // re-sync after silence or a period jump
    while (now >= nextTs_ + 1.5 * T0) {
        if (epoch_ < nextTs_ - 4.0 * T0) epoch_ = nextTs_;
        while (epoch_ + 0.5 * T0 < nextTs_) epoch_ += T0;
        if (epoch_ > nextTs_ + T0) epoch_ = nextTs_;
        placeGrain(nextTs_, epoch_, T0, f);
        nextTs_ += T1;
    }

    const int lat = coreLatency();
    const std::int64_t tOut = tIn_ - lat;
    const auto oi = static_cast<std::size_t>(tOut & kMask);
    const float dl = inL_[oi], dr = inR_[oi];
    const float ws = std::max(wsum_[oi], 1.0f);
    const float el = outL_[oi] / ws, er = outR_[oi] / ws;
    outL_[oi] = outR_[oi] = wsum_[oi] = 0.0f;
    engage_.setTarget((std::fabs(semis) > 1e-3f || std::fabs(f - 1.0) > 1e-4) ? 1.0f : 0.0f);
    const float g = engage_.next();
    const float yl = lerp(dl, el, g), yr = lerp(dr, er, g);
    l = lerp(dl, yl, mix_);
    r = lerp(dr, yr, mix_);
    ++tIn_;
}

void IronLock::publish(std::uint32_t m) noexcept {
    auto& bus = GroupBus::instance();
    bus.mask.store(m, std::memory_order_relaxed);
    busVersion_ = bus.version.fetch_add(1, std::memory_order_acq_rel) + 1;
    groupMask_ = m;
    haveGroup_ = true;
}

void IronLock::afterChunk(int) noexcept {
    corrPub_.store(curCorr_ * strength_ * 100.0f, std::memory_order_relaxed);
    // QUANTUM KEY follows the detector. GROUPS: adopt the shared scale whenever another instance changed it;
    // publish when this instance's own scale (or detected key) changes.
    std::uint32_t want = (quantum_ && detectedKey_ >= 0) ? scaleMask(detectedKey_) : ownMask_;
    if (groups_) {
        if (want != lastOwn_) publish(want);
        const std::uint32_t ver = GroupBus::instance().version.load(std::memory_order_acquire);
        if (ver != busVersion_) {
            busVersion_ = ver;
            groupMask_ = GroupBus::instance().mask.load(std::memory_order_relaxed);
            haveGroup_ = true;
        }
        if (haveGroup_) want = groupMask_;
    }
    lastOwn_ = (quantum_ && detectedKey_ >= 0) ? scaleMask(detectedKey_) : ownMask_;
    mask_ = want;
    maskPub_.store(mask_, std::memory_order_relaxed);
}

namespace {
constexpr PresetValue kNatural[] = {{"strength", 55}, {"transition", 90}, {"sustain", 70}};
constexpr PresetValue kHard[] = {{"strength", 100}, {"transition", 0}, {"sustain", 0}};
constexpr PresetValue kQuantum[] = {{"quantum_key", 1}, {"strength", 75}};
constexpr PresetValue kDeep[] = {{"strength", 60}, {"pitch_shift", -5}, {"formant", -2}};
constexpr PresetValue kHigh[] = {{"strength", 60}, {"pitch_shift", 7}, {"link", 1}};
constexpr PresetValue kOctaveDouble[] = {{"strength", 70}, {"pitch_shift", -12}, {"mix", 50}};
const Preset kPresets[] = {
    {"Natural Lead", "Tuning", kNatural}, {"Hard Lock", "Tuning", kHard}, {"Quantum Auto Key", "Tuning", kQuantum},
    {"Deep Voice", "Shift", kDeep}, {"High Chipmunk", "Shift", kHigh}, {"Octave Down Double", "Shift", kOctaveDouble},
};
} // namespace
std::span<const Preset> ironLockPresets() noexcept { return kPresets; }

} // namespace dm::il
