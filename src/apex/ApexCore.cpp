// HEM APEX CORE - engine + factory presets.
#include "dm/apex/ApexCore.hpp"

namespace dm::ax {

void ApexCore::prepareCore(float sr) {
    sr_ = sr;
    mono_.set(a::kMonoSafeHz, sr_);
    lowTap_.set(designBiquad(BiquadType::LowPass, a::kColorLowHz, 0.707f, 0, sr_));
    highTap_.set(designBiquad(BiquadType::HighPass, a::kColorHighHz, 0.707f, 0, sr_));
    sideHp_.set(designBiquad(BiquadType::HighPass, 300.0f, 0.707f, 0, sr_));
    for (auto& d : dc_) d.prepare(sr_);
}
void ApexCore::resetCore() noexcept {
    split_.reset(); mono_.reset();
    for (auto& e : bandEnv_) e.reset();
    avgEnv_.reset(); peakEnv_.reset();
    for (StereoBiquad* b : {&lowTap_, &highTap_, &air_, &sideHp_}) b->reset();
    for (auto& d : dc_) d.reset();
    grMax_ = 0;
}

void ApexCore::updateCore(const float* v, const float* old, bool) noexcept {
    if (!changed(v, old, {Engine, Amount, Contain, Boost, Parallel, MonoSafe, Width, Peaks, Curve, Attack, Release,
                          ColorLow, ColorHigh})) return;
    e_ = &a::kEngineTable[std::min(std::max(static_cast<int>(std::lround(v[Engine])), 0), a::kEngines - 1)];
    amt_ = v[Amount] * 0.01f;
    const float s = std::min(amt_, 2.0f);
    contain_ = v[Contain] * 0.01f * s;
    boost_ = v[Boost] * 0.01f * s;
    parallel_ = v[Parallel] * 0.01f;
    monoSafe_ = v[MonoSafe] >= 0.5f;
    width_ = v[Width] * 0.01f;
    drive_ = dbToGain(a::kAmountDrivePerUnitDb * std::max(0.0f, amt_ - 1.0f));
    const float pk = v[Peaks] * 0.01f, strength = std::min(1.0f, amt_);
    thr_ = -2.0f - 10.0f * pk;
    ratio_ = 1.0f + (e_->ratioMax - 1.0f) * pk * strength;
    knee_ = 24.0f * v[Curve] * 0.01f;
    colorLow_ = v[ColorLow] * 0.01f * std::min(1.0f, amt_);
    colorHigh_ = v[ColorHigh] * 0.01f * std::min(1.0f, amt_);
    const float atk = e_->attackMs * v[Attack] * 0.01f, rel = e_->releaseMs * v[Release] * 0.01f;
    for (auto& e : bandEnv_) e.prepare(atk, rel, sr_);
    avgEnv_.prepare(300.0f, 300.0f, sr_);
    peakEnv_.prepare(atk * 0.2f, rel, sr_);
    split_.set(e_->xoverLow, e_->xoverHigh, sr_);
    air_.set(designBiquad(BiquadType::HighShelf, 12000.0f, 0.7f, e_->airDb * std::min(1.0f, amt_), sr_));
    active_ = parallel_ > 0.0f && (amt_ > 0.0f || width_ != 1.0f || monoSafe_);
}

void ApexCore::processCore(float& l, float& r) noexcept {
    const float dryL = l, dryR = r;
    { const float m = 0.5f * (l + r), sd = 0.5f * (l - r); inMid_ += double(m) * m; inSide_ += double(sd) * sd; }

    // MONO-SAFE: everything under 120 Hz summed to mono.
    if (monoSafe_) {
        float lo0, hi0, lo1, hi1;
        mono_.split(0, l, lo0, hi0);
        mono_.split(1, r, lo1, hi1);
        const float lm = 0.5f * (lo0 + lo1);
        l = lm + hi0; r = lm + hi1;
    }

    // HELIX engine works on mid/side; the others on left/right.
    float a0 = l, a1 = r;
    if (e_->midSide) { a0 = 0.5f * (l + r); a1 = 0.5f * (l - r); }

    // CONTAIN / BOOST across 3 bands.
    if (contain_ > 0.0f || boost_ > 0.0f) {
        float b[2][3];
        split_.split(0, a0, b[0][0], b[0][1], b[0][2]);
        split_.split(1, a1, b[1][0], b[1][1], b[1][2]);
        float lv[3], sum = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            lv[k] = powToDb(bandEnv_[k].process(0.5f * (b[0][k] * b[0][k] + b[1][k] * b[1][k])));
            sum += lv[k];
        }
        const float avg = avgEnv_.process(sum / 3.0f);
        a0 = a1 = 0;
        for (std::size_t k = 0; k < 3; ++k) {
            float g = 0;
            const float over = lv[k] - (avg + a::kContainOffsetDb);
            if (over > 0) g -= std::min(e_->containDb, over * contain_);
            if (lv[k] < a::kBoostThreshDb && lv[k] > -70.0f)
                g += std::min(e_->boostDb * boost_, (a::kBoostThreshDb - lv[k]) * 0.5f * boost_);
            grMax_ = std::max(grMax_, -std::min(g, 0.0f));
            const float lin = dbToGain(g);
            a0 += b[0][k] * lin; a1 += b[1][k] * lin;
        }
    }
    if (e_->midSide) { l = a0 + a1; r = a0 - a1; } else { l = a0; r = a1; }

    // COLOR PROFILE: low-band and high-band harmonics added back.
    for (int ch = 0; ch < 2; ++ch) {
        float& x = ch ? r : l;
        if (colorLow_ > 0.0f) {
            const float lo = lowTap_.process(ch, x);
            x += 0.5f * colorLow_ * (saturate(SatType::Tube, lo, 4.0f) * 4.0f - lo * 4.0f) * 0.25f;
        }
        if (colorHigh_ > 0.0f) {
            const float hi = highTap_.process(ch, x);
            x += 0.5f * colorHigh_ * std::tanh(hi * 3.0f);
        }
        x = air_.process(ch, x);
        x = dc_[static_cast<std::size_t>(ch)].process(x);
    }

    // AMOUNT drive into the engine's saturation, then the PEAKS / CURVE transfer stage.
    l *= drive_; r *= drive_;
    if (amt_ > 0.0f) {
        const float d = 1.0f + (e_->satDrive - 1.0f) * std::min(1.0f, amt_);
        l = saturate(e_->sat, l, d); r = saturate(e_->sat, r, d);
        const float lvl = gainToDb(peakEnv_.process(std::max(std::fabs(l), std::fabs(r))));
        const float gr = compressorGainDb(lvl, thr_, ratio_, knee_);
        grMax_ = std::max(grMax_, -gr);
        const float g = dbToGain(gr);
        l *= g; r *= g;
    }

    // WIDTH (HELIX widens only above 300 Hz).
    if (width_ != 1.0f) {
        const float m = 0.5f * (l + r);
        float sd = 0.5f * (l - r);
        if (e_->midSide) { const float hiS = sideHp_.process(0, sd); sd = (sd - hiS) + hiS * width_; }
        else sd *= width_;
        l = m + sd; r = m - sd;
    }
    { const float m = 0.5f * (l + r), sd = 0.5f * (l - r); outMid_ += double(m) * m; outSide_ += double(sd) * sd; }

    // PARALLEL
    if (parallel_ < 1.0f) { l = lerp(dryL, l, parallel_); r = lerp(dryR, r, parallel_); }
}

void ApexCore::afterChunk(int) noexcept {
    grPub_ = grMax_;
    grMax_ = 0;
    const double inW = inMid_ > 1e-12 ? inSide_ / inMid_ : 0.0, outW = outMid_ > 1e-12 ? outSide_ / outMid_ : 0.0;
    widthPub_ = inW > 1e-9 ? static_cast<float>(100.0 * std::sqrt(outW / inW)) : 100.0f;
    inMid_ = inSide_ = outMid_ = outSide_ = 0;
}

float ApexCore::transferDb(float inDb) const noexcept {
    const float pk = params_.get(Peaks) * 0.01f, amt = params_.get(Amount) * 0.01f;
    const auto& e = a::kEngineTable[std::min(std::max(params_.getInt(Engine), 0), a::kEngines - 1)];
    const float driven = inDb + a::kAmountDrivePerUnitDb * std::max(0.0f, amt - 1.0f);
    const float ratio = 1.0f + (e.ratioMax - 1.0f) * pk * std::min(1.0f, amt);
    return driven + compressorGainDb(driven, -2.0f - 10.0f * pk, ratio, 24.0f * params_.get(Curve) * 0.01f);
}
float ApexCore::attackMs() const noexcept {
    return a::kEngineTable[std::min(std::max(params_.getInt(Engine), 0), a::kEngines - 1)].attackMs * params_.get(Attack) * 0.01f;
}
float ApexCore::releaseMs() const noexcept {
    return a::kEngineTable[std::min(std::max(params_.getInt(Engine), 0), a::kEngines - 1)].releaseMs * params_.get(Release) * 0.01f;
}

namespace {
constexpr PresetValue kBalanced[] = {{"engine", 0}};
constexpr PresetValue kAiry[] = {{"engine", 1}, {"amount", 120}, {"color_high", 25}, {"width", 115}};
constexpr PresetValue kLoudTrap[] = {{"engine", 2}, {"amount", 160}, {"contain", 45}, {"boost", 30}, {"peaks", 70},
    {"mono_safe", 1}, {"color_low", 35}};
constexpr PresetValue kWide[] = {{"engine", 3}, {"amount", 110}, {"width", 140}, {"mono_safe", 1}};
constexpr PresetValue kTitan[] = {{"engine", 4}, {"amount", 150}, {"peaks", 75}, {"curve", 15}, {"color_low", 40}, {"mono_safe", 1}};
constexpr PresetValue kVelvet[] = {{"engine", 5}, {"amount", 100}, {"peaks", 35}, {"curve", 60}, {"release", 150}};
constexpr PresetValue kGentle[] = {{"engine", 0}, {"amount", 70}, {"parallel", 60}, {"contain", 20}, {"boost", 10}};
const Preset kPresets[] = {
    {"Balanced Master", "Master", kBalanced}, {"Airy Pop Master", "Master", kAiry}, {"Loud Trap Master", "Master", kLoudTrap},
    {"Wide Helix", "Master", kWide}, {"Titan Slam", "Master", kTitan}, {"Velvet Smooth", "Master", kVelvet},
    {"Gentle Parallel", "Master", kGentle},
};
} // namespace
std::span<const Preset> apexPresets() noexcept { return kPresets; }

} // namespace dm::ax
