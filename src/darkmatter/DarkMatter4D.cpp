// HEM DARK MATTER 4D - engine + presets.
#include "dm/darkmatter/DarkMatter4D.hpp"

namespace dm::d4 {

void DarkMatter4D::prepareCore(float sr) {
    sr_ = sr;
    rev_.prepare(sr_);
    dly_.prepare(sr_);
    const int win = static_cast<int>(a::kGrainWindowMs * 0.001f * sr_);
    cloudL_.prepare(win, 0x13579BDu);
    cloudR_.prepare(win, 0x2468ACEu);
    revMix_.prepare(20.0f, sr_);
    dlyMix_.prepare(20.0f, sr_);
}
void DarkMatter4D::resetCore() noexcept {
    rev_.reset(); dly_.reset(); cloudL_.reset(); cloudR_.reset();
    lowSh_.reset(); highSh_.reset();
    for (auto& e : duckEnv_) e.reset();
    revFbL_ = revFbR_ = dlyFbL_ = dlyFbR_ = 0;
}
void DarkMatter4D::updateCore(const float* v, const float* old, bool) noexcept {
    revMix_.setTarget(v[Reverb] * 0.01f);
    dlyMix_.setTarget(v[Delay] * 0.01f);
    if (!old) { revMix_.snap(revMix_.target); dlyMix_.snap(dlyMix_.target); } // reset / load: no fade-in
    extKey_ = v[ExtKey] >= 0.5f;
    if (!changed(v, old, {Decay, Depth, Predelay, LowColor, HighColor, Routing, Ducking, Shape, Time, Feedback, Multiplier, Tempo}))
        return;
    const float depth = v[Depth] * 0.01f;
    ReverbSettings rs;
    rs.size = 0.6f + 1.8f * depth;
    rs.decaySec = v[Decay];
    rs.dampHz = expMap(1.0f - 0.6f * depth, 3000.0f, 16000.0f) * dbToGain(v[HighColor] * 0.01f * 6.0f) ;
    rs.diffusion = 0.75f;
    rs.modDepthMs = 0.35f;
    rs.modRateHz = 0.45f;
    rs.earlyLevel = 0.6f - 0.5f * depth;
    rs.predelayMs = v[Predelay];
    rs.width = 1.0f + 0.3f * depth;
    rev_.apply(rs);
    DelaySettings ds;
    ds.timeMs = syncToMs(static_cast<int>(std::lround(v[Time])), v[Tempo]);
    ds.feedback = v[Feedback] * 0.01f;
    ds.toneHz = 7000.0f;
    ds.lowCutHz = 150.0f;
    ds.pingPong = true;
    dly_.apply(ds);
    lowSh_.set(designBiquad(BiquadType::LowShelf, a::kColorLowHz, 0.7f, a::kColorMaxDb * v[LowColor] * 0.01f, sr_));
    highSh_.set(designBiquad(BiquadType::HighShelf, a::kColorHighHz, 0.7f, a::kColorMaxDb * v[HighColor] * 0.01f, sr_));
    route_ = std::min(std::max(static_cast<int>(std::lround(v[Routing])), 0), 2);
    multiplier_ = v[Multiplier] >= 0.5f;
    ducking_ = v[Ducking] * 0.01f;
    const float shape = v[Shape] * 0.01f;
    const float rel = expMap(shape, a::kDuckReleaseMinMs, a::kDuckReleaseMaxMs);
    for (auto& e : duckEnv_) e.prepare(5.0f, rel, sr_);
    shapeExp_ = lerp(0.5f, 2.0f, shape); // curve of the duck depth vs level
}
void DarkMatter4D::processCore(float& l, float& r) noexcept {
    const float rm = revMix_.next(), dm = dlyMix_.next();
    // Routing: feed the other engine's previous output in series modes.
    float rinL = l, rinR = r, dinL = l, dinR = r;
    if (route_ == 0) { dinL += revFbL_; dinR += revFbR_; }
    if (route_ == 2) { rinL += dlyFbL_; rinR += dlyFbR_; }
    float rl, rr, dl, dr;
    rev_.process(rinL, rinR, rl, rr);
    dly_.process(dinL, dinR, dl, dr);
    if (multiplier_) {
        float gl0, gr0, gl1, gr1;
        cloudL_.process(dl, 1.0f, 0.6f, 0.8f, false, gl0, gr0);
        cloudR_.process(dr, 1.0f, 0.6f, 0.8f, false, gl1, gr1);
        dl = 0.5f * dl + (gl0 + gl1) * 0.75f;
        dr = 0.5f * dr + (gr0 + gr1) * 0.75f;
    }
    revFbL_ = rl * 0.5f; revFbR_ = rr * 0.5f;
    dlyFbL_ = dl * 0.5f; dlyFbR_ = dr * 0.5f;

    float duckG[2] = {1.0f, 1.0f};
    if (ducking_ > 0.0f) {
        const float* key = extKey_ ? keyFrame() : nullptr;
        const float in[2] = {key ? key[0] : l, key ? key[1] : r};
        for (std::size_t ch = 0; ch < 2; ++ch) {
            const float envDb = gainToDb(duckEnv_[ch].process(std::fabs(in[ch])));
            const float lvl = std::pow(clampf((envDb + 40.0f) / 34.0f, 0.0f, 1.0f), shapeExp_);
            const float dDb = a::kDuckMaxDb * ducking_ * lvl;
            duckMax_[ch] = std::max(duckMax_[ch], dDb);
            duckG[ch] = dbToGain(-dDb);
        }
    }
    float wl = (rl * rm + dl * dm) * duckG[0];
    float wr = (rr * rm + dr * dm) * duckG[1];
    wl = highSh_.process(0, lowSh_.process(0, wl));
    wr = highSh_.process(1, lowSh_.process(1, wr));
    revPk_ = std::max(revPk_, std::max(std::fabs(rl), std::fabs(rr)) * rm);
    dlyPk_ = std::max(dlyPk_, std::max(std::fabs(dl), std::fabs(dr)) * dm);
    l += wl;
    r += wr;
}
void DarkMatter4D::afterChunk(int) noexcept {
    for (std::size_t c = 0; c < 2; ++c) { duckPub_[c].store(duckMax_[c], std::memory_order_relaxed); duckMax_[c] = 0; }
    revPub_.store(gainToDb(revPk_), std::memory_order_relaxed);
    dlyPub_.store(gainToDb(dlyPk_), std::memory_order_relaxed);
    revPk_ = dlyPk_ = 0;
}
float DarkMatter4D::shapeCurve(float tMs) const noexcept {
    const float shape = params_.get(Shape) * 0.01f;
    const float rel = expMap(shape, a::kDuckReleaseMinMs, a::kDuckReleaseMaxMs);
    const float lvl = std::pow(std::exp(-tMs / rel), lerp(0.5f, 2.0f, shape));
    return dbToGain(-a::kDuckMaxDb * lvl);
}

namespace {
constexpr PresetValue kVocalSpace[] = {{"reverb", 25}};
constexpr PresetValue kInfinite[] = {{"reverb", 35}, {"decay", 40}, {"depth", 90}, {"ducking", 50}, {"delay", 0}};
constexpr PresetValue kSlapRoom[] = {{"reverb", 20}, {"decay", 0.8f}, {"depth", 20}, {"delay", 30}, {"time", 10}, {"feedback", 10}};
constexpr PresetValue kGrainWash[] = {{"reverb", 30}, {"decay", 8}, {"delay", 30}, {"multiplier", 1}, {"routing", 2}, {"feedback", 55}};
constexpr PresetValue kDarkThrow[] = {{"reverb", 30}, {"decay", 5}, {"high_color", -60}, {"low_color", -30}, {"delay", 30},
    {"time", 5}, {"routing", 0}, {"ducking", 60}, {"shape", 70}};
const Preset kPresets[] = {{"Vocal Space", "Vocal", kVocalSpace}, {"Infinite Dark Matter", "Ambient", kInfinite},
    {"Slap Room", "Vocal", kSlapRoom}, {"Grain Wash", "Ambient", kGrainWash}, {"Dark Throw", "FX", kDarkThrow}};
} // namespace
std::span<const Preset> darkMatterPresets() noexcept { return kPresets; }

} // namespace dm::d4
