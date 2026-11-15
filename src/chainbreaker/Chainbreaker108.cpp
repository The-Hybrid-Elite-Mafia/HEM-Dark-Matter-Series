// HEM CHAINBREAKER 108 - engine, modules, presets and the 8 Quick-Load chains.
#include "dm/chainbreaker/Chainbreaker108.hpp"

#include "dm/core/Saturation.hpp"

namespace dm::cb {

namespace {
float clipTo(int mode, float x) noexcept {
    switch (mode) {
    case 1: { const float k = 0.89f, ax = std::fabs(x); return ax <= k ? x : std::copysign(k + (1 - k) * std::tanh((ax - k) / (1 - k)), x); }
    case 2: return clampf(x, -1.0f, 1.0f);
    case 3: { const float k = 0.5f, ax = std::fabs(x); if (ax <= k) return x;
              const float t = std::min((ax - k) / (1.5f - k), 1.0f); return std::copysign(k + (1 - k) * (1.5f * t - 0.5f * t * t * t), x); }
    default: return x;
    }
}
} // namespace

// ===================================== STUTTER =========================================================
void StutterUnit::prepare(float sr, std::uint32_t seed) {
    sr_ = sr;
    int n = 1;
    while (n < static_cast<int>(2.0f * sr_)) n <<= 1;
    bufL_.assign(static_cast<std::size_t>(n), 0.0f);
    bufR_.assign(static_cast<std::size_t>(n), 0.0f);
    rng_ = Noise(seed);
    reset();
}
void StutterUnit::reset() noexcept {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    t_ = 0; beatStart_ = 0; sliceLen_ = 1; beatPos_ = 0; on_ = false; rateHz_ = 0;
}
void StutterUnit::process(float& l, float& r, const Settings& s) noexcept {
    const auto mask = static_cast<std::int64_t>(bufL_.size()) - 1;
    bufL_[static_cast<std::size_t>(t_ & mask)] = l;
    bufR_[static_cast<std::size_t>(t_ & mask)] = r;
    const double beat = 60.0 / std::max(s.bpm, 20.0f) * sr_;
    if (beatPos_ >= beat || t_ == 0) {
        if (t_ != 0) beatPos_ -= beat;
        beatStart_ = t_;
        const float u = 0.5f * (rng_.next() + 1.0f);
        on_ = u < s.flux;
        float frac = s.stutDivFrac;
        const float v = 0.5f * (rng_.next() + 1.0f);
        if (v < s.flux * 0.5f) frac *= (v < s.flux * 0.25f) ? 2.0f : 0.5f; // FLUX also scatters the rate
        sliceLen_ = std::max<std::int64_t>(32, static_cast<std::int64_t>(beat * std::min(frac, 1.0f)));
        rateHz_ = on_ ? sr_ / static_cast<float>(sliceLen_) : 0.0f;
    }
    if (on_) {
        const std::int64_t k = (t_ - beatStart_) % sliceLen_;
        const auto src = static_cast<std::size_t>((beatStart_ + k) & mask);
        const float fade = assumed::cb::kStutFadeMs * 0.001f * sr_;
        const float open = s.stutDuty * static_cast<float>(sliceLen_);
        const float kk = static_cast<float>(k);
        const float g = clampf(std::min(kk / fade, (open - kk) / fade), 0.0f, 1.0f);
        l = bufL_[src] * g;
        r = bufR_[src] * g;
    }
    beatPos_ += 1.0;
    ++t_;
}

// ===================================== MULTI-CLIP ======================================================
void ClipUnit::prepare(float sr) { sr_ = sr; reset(); }
void ClipUnit::reset() noexcept { split_.reset(); clipped_ = {0, 0, 0}; count_ = 0; lastLow_ = lastHigh_ = -1; }
void ClipUnit::process(float& l, float& r, const Settings& s) noexcept {
    if (s.clipLow != lastLow_ || s.clipHigh != lastHigh_) { split_.set(s.clipLow, s.clipHigh, sr_); lastLow_ = s.clipLow; lastHigh_ = s.clipHigh; }
    float b[2][3];
    split_.split(0, l, b[0][0], b[0][1], b[0][2]);
    split_.split(1, r, b[1][0], b[1][1], b[1][2]);
    const float g = s.drive, comp = 1.0f / std::sqrt(g);
    float out[2] = {0, 0};
    for (std::size_t k = 0; k < 3; ++k)
        for (std::size_t ch = 0; ch < 2; ++ch) {
            const float x = b[ch][k] * g;
            if (std::fabs(x) > 1.0f) ++clipped_[k];
            out[ch] += lerp(clampf(x, -1.0f, 1.0f), std::tanh(x), s.shape) * comp;
        }
    count_ += 2;
    l = out[0]; r = out[1];
}
void ClipUnit::endChunk() noexcept {
    for (std::size_t k = 0; k < 3; ++k) { pub_[k] = count_ ? 100.0f * static_cast<float>(clipped_[k]) / static_cast<float>(count_) : 0.0f; clipped_[k] = 0; }
    count_ = 0;
}

// ===================================== GRAIN ENGINE ====================================================
void GrainUnit::prepare(float sr) {
    const int win = static_cast<int>(assumed::cb::kGrainWindowMs * 0.001f * sr);
    cl_.prepare(win, 0xA5A5A5u);
    cr_.prepare(win, 0x5A5A5Au);
}
void GrainUnit::reset() noexcept { cl_.reset(); cr_.reset(); }
void GrainUnit::process(float& l, float& r, const Settings& s) noexcept {
    float a0, b0, a1, b1;
    cl_.process(l, s.grainRatio, s.flux, s.wide, s.reverse, a0, b0);
    cr_.process(r, s.grainRatio, s.flux, s.wide, s.reverse, a1, b1);
    const float wl = a0 + 0.5f * a1, wr = b1 + 0.5f * b0;
    energyDb_ = gainToDb(std::max(std::fabs(wl), std::fabs(wr)));
    l = lerp(l, wl, s.grainMix);
    r = lerp(r, wr, s.grainMix);
}

// ===================================== SPECTRO EQ ======================================================
void EqUnit::process(float& l, float& r, const Settings& s) noexcept {
    for (std::size_t k = 0; k < 4; ++k) {
        if (!s.eqOn[k]) continue;
        b_[k].set(s.eq[k]);
        l = b_[k].process(0, l);
        r = b_[k].process(1, r);
    }
}

// ===================================== ENGINE ==========================================================
void Chainbreaker108::prepareCore(float sr) {
    sr_ = sr;
    for (std::size_t i = 0; i < 4; ++i) {
        stut_[i].prepare(sr_, 0x9E3779B9u + static_cast<std::uint32_t>(i) * 7919u);
        clip_[i].prepare(sr_);
        grain_[i].prepare(sr_);
    }
    for (auto& f : xf_) f.setCutoff(a::kCrossfeedHz, sr_);
}
void Chainbreaker108::resetCore() noexcept {
    for (std::size_t i = 0; i < 4; ++i) { stut_[i].reset(); clip_[i].reset(); grain_[i].reset(); eq_[i].reset(); }
    for (auto& b : bc_) b.reset();
    for (auto& b : gr_) b.reset();
    tiltLo_.reset(); tiltHi_.reset();
    for (auto& f : xf_) f.reset();
}
void Chainbreaker108::updateCore(const float* v, const float* old, bool) noexcept {
    (void)old;
    auto pick = [&](std::size_t i, int n) { return std::min(std::max(static_cast<int>(std::lround(v[i])), 0), n - 1); };
    for (std::size_t i = 0; i < 4; ++i) {
        type_[i] = pick(S1Type + i, 4);
        on_[i] = v[S1On + i] >= 0.5f;
    }
    constexpr float divs[a::kStutDivs] = {0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f};
    s_.stutDivFrac = divs[std::min(a::kStutDivs - 1, static_cast<int>(v[StutX] * 0.01f * (a::kStutDivs - 1) + 0.5f))];
    s_.stutDuty = 0.1f + 0.9f * v[StutY] * 0.01f;
    s_.flux = clampf((v[Flux] + v[MFlux]) * 0.01f, 0.0f, 1.0f);
    s_.bpm = v[Tempo];
    const float d = clampf((v[Drive] + v[MDrive]) * 0.01f, 0.0f, 1.0f);
    s_.drive = 1.0f + (a::kDriveMaxGain - 1.0f) * d * d;
    s_.shape = v[ClipShape] * 0.01f;
    s_.clipLow = v[ClipLow];
    s_.clipHigh = v[ClipHigh];
    s_.grainRatio = std::pow(2.0f, v[GrainPitch] / 12.0f);
    s_.reverse = v[GrainReverse] >= 0.5f;
    s_.wide = v[Wide] * 0.01f;
    s_.grainMix = v[GrainMix] * 0.01f;
    for (std::size_t k = 0; k < 4; ++k) {
        const float f = v[Eq1F + 3 * k], g = v[Eq1G + 3 * k], q = v[Eq1Q + 3 * k];
        const BiquadType t = k == 0 ? BiquadType::LowShelf : k == 3 ? BiquadType::HighShelf : BiquadType::Peak;
        s_.eq[k] = designBiquad(t, f, q, g, sr_);
        s_.eqOn[k] = g != 0.0f;
    }
    mix_ = v[MMix] * 0.01f;
    const float t = v[MTone] * 0.01f;
    tiltOn_ = t != 0.0f;
    tiltLo_.set(designBiquad(BiquadType::LowShelf, 1000.0f, 0.5f, -a::kToneTiltDb * 0.5f * t, sr_));
    tiltHi_.set(designBiquad(BiquadType::HighShelf, 1000.0f, 0.5f, a::kToneTiltDb * 0.5f * t, sr_));
    bc_[0].set(designBiquad(BiquadType::LowShelf, a::kBroadcastHz[0], 0.7f, v[BcLow], sr_));
    bc_[1].set(designBiquad(BiquadType::Peak, a::kBroadcastHz[1], 0.7f, v[BcMid], sr_));
    bc_[2].set(designBiquad(BiquadType::HighShelf, a::kBroadcastHz[2], 0.7f, v[BcHigh], sr_));
    masterEqOn_ = v[BcLow] != 0 || v[BcMid] != 0 || v[BcHigh] != 0;
    for (std::size_t k = 0; k < 5; ++k) {
        const BiquadType tp = k == 0 ? BiquadType::LowShelf : k == 4 ? BiquadType::HighShelf : BiquadType::Peak;
        gr_[k].set(designBiquad(tp, a::kGraphicHz[k], 1.0f, v[GSub + k], sr_));
        masterEqOn_ |= v[GSub + k] != 0;
    }
    crossfeed_ = v[Crossfeed] * 0.01f;
    width_ = v[Width] * 0.01f;
    outLevel_ = dbToGain(v[OutLevel]);
    route_ = pick(UtilRoute, 4);
}
void Chainbreaker108::masterEq(float& l, float& r) noexcept {
    if (!masterEqOn_) return;
    for (auto& b : bc_) { l = b.process(0, l); r = b.process(1, r); }
    for (auto& b : gr_) { l = b.process(0, l); r = b.process(1, r); }
}
void Chainbreaker108::processCore(float& l, float& r) noexcept {
    const float dl = l, dr = r;
    if (route_ == 0) masterEq(l, r);
    for (std::size_t i = 0; i < 4; ++i) {
        if (!on_[i]) continue;
        switch (type_[i]) {
        case 0: stut_[i].process(l, r, s_); break;
        case 1: clip_[i].process(l, r, s_); break;
        case 2: grain_[i].process(l, r, s_); break;
        default: eq_[i].process(l, r, s_); break;
        }
    }
    if (route_ != 0) masterEq(l, r);
    if (tiltOn_) { l = tiltHi_.process(0, tiltLo_.process(0, l)); r = tiltHi_.process(1, tiltLo_.process(1, r)); }
    if (crossfeed_ > 0.0f) {
        const float xl = xf_[0].lp(r), xr = xf_[1].lp(l), n = 1.0f / (1.0f + 0.4f * crossfeed_);
        l = (l + 0.4f * crossfeed_ * xl) * n;
        r = (r + 0.4f * crossfeed_ * xr) * n;
    }
    if (width_ != 1.0f) { const float m = 0.5f * (l + r), s = 0.5f * (l - r) * width_; l = m + s; r = m - s; }
    l *= outLevel_; r *= outLevel_;
    if (route_ != 0) { l = clipTo(route_, l); r = clipTo(route_, r); }
    l = lerp(dl, l, mix_);
    r = lerp(dr, r, mix_);
}
void Chainbreaker108::afterChunk(int) noexcept {
    float hz = 0;
    std::array<float, 3> cp{0, 0, 0};
    for (std::size_t i = 0; i < 4; ++i) {
        if (on_[i] && type_[i] == 0) hz = std::max(hz, stut_[i].rateHz());
        if (on_[i] && type_[i] == 1) { for (int b = 0; b < 3; ++b) cp[static_cast<std::size_t>(b)] = std::max(cp[static_cast<std::size_t>(b)], clip_[i].clipPercent(b)); }
        clip_[i].endChunk();
    }
    stutHz_.store(hz, std::memory_order_relaxed);
    for (std::size_t b = 0; b < 3; ++b) clipPub_[b].store(cp[b], std::memory_order_relaxed);
}
float Chainbreaker108::spectroCurveDb(float hz) const noexcept {
    float db = 0;
    for (std::size_t k = 0; k < 4; ++k) {
        const BiquadType t = k == 0 ? BiquadType::LowShelf : k == 3 ? BiquadType::HighShelf : BiquadType::Peak;
        db += designBiquad(t, params_.get(Eq1F + 3 * k), params_.get(Eq1Q + 3 * k), params_.get(Eq1G + 3 * k), sr_).magnitudeDb(hz, sr_);
    }
    return db;
}
bool Chainbreaker108::loadChain(std::size_t i) noexcept {
    const auto c = quickLoadChains();
    if (i >= c.size()) return false;
    for (const auto& v : c[i].values) {
        const int k = params_.indexOf(v.id);
        if (k >= 0) params_.set(static_cast<std::size_t>(k), v.value);
    }
    return true;
}

namespace {
constexpr PresetValue kInit[] = {{"s1_type", 0}};
constexpr PresetValue kBeatChopper[] = {{"flux", 60}, {"stut_x", 75}, {"stut_y", 45}, {"drive", 30}, {"grain_mix", 15}};
constexpr PresetValue kDirtyBus[] = {{"s1_on", 0}, {"drive", 55}, {"clip_shape", 70}, {"grain_mix", 0}, {"eq2_gain", 3}, {"util_route", 3}};
constexpr PresetValue kShimmer[] = {{"s1_on", 0}, {"grain_pitch", 12}, {"grain_mix", 35}, {"wide", 90}, {"eq4_gain", 4}, {"drive", 5}};
constexpr PresetValue kLoFiRadio[] = {{"flux", 15}, {"drive", 50}, {"bc_low", -8}, {"bc_high", -8}, {"bc_mid", 5}, {"width", 40}};
const Preset kPresets[] = {{"Init Chain", "Init", kInit}, {"Beat Chopper", "Rhythm", kBeatChopper},
    {"Dirty Bus", "Drive", kDirtyBus}, {"Shimmer Up", "Texture", kShimmer}, {"Lo-Fi Radio", "Character", kLoFiRadio}};

constexpr PresetValue kQ1[] = {{"s1_type", 0}, {"s2_type", 1}, {"s3_type", 2}, {"s4_type", 3}, {"s1_on", 1}, {"s2_on", 1}, {"s3_on", 1}, {"s4_on", 1}};
constexpr PresetValue kQ2[] = {{"s1_type", 1}, {"s2_type", 3}, {"s3_type", 2}, {"s4_type", 0}, {"drive", 45}};
constexpr PresetValue kQ3[] = {{"s1_type", 2}, {"s2_type", 2}, {"s3_type", 3}, {"s4_type", 1}, {"grain_pitch", -12}, {"grain_mix", 30}};
constexpr PresetValue kQ4[] = {{"s1_type", 3}, {"s2_type", 0}, {"s3_type", 1}, {"s4_type", 2}, {"flux", 50}, {"stut_x", 100}};
constexpr PresetValue kQ5[] = {{"s1_type", 0}, {"s2_type", 0}, {"s3_type", 3}, {"s4_type", 3}, {"flux", 40}, {"eq3_gain", 4}};
constexpr PresetValue kQ6[] = {{"s1_type", 1}, {"s2_type", 1}, {"s3_on", 0}, {"s4_on", 0}, {"drive", 35}, {"clip_shape", 20}};
constexpr PresetValue kQ7[] = {{"s1_type", 2}, {"s2_type", 3}, {"s3_on", 0}, {"s4_on", 0}, {"grain_reverse", 1}, {"grain_mix", 40}};
constexpr PresetValue kQ8[] = {{"s1_type", 3}, {"s2_on", 0}, {"s3_on", 0}, {"s4_on", 0}, {"eq1_gain", 3}, {"eq4_gain", 3}};
const Preset kChains[] = {{"Chain 1", "Quick", kQ1}, {"Chain 2", "Quick", kQ2}, {"Chain 3", "Quick", kQ3}, {"Chain 4", "Quick", kQ4},
    {"Chain 5", "Quick", kQ5}, {"Chain 6", "Quick", kQ6}, {"Chain 7", "Quick", kQ7}, {"Chain 8", "Quick", kQ8}};
} // namespace
std::span<const Preset> chainbreakerPresets() noexcept { return kPresets; }
std::span<const Preset> quickLoadChains() noexcept { return kChains; }

} // namespace dm::cb
