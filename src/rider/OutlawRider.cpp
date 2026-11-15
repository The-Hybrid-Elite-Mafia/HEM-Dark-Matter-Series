// HEM OUTLAW RIDER - engine + presets.
#include "dm/rider/OutlawRider.hpp"

namespace dm::rd {

void OutlawRider::prepareCore(float sr) {
    sr_ = sr;
    frames_.assign(kFrames, RideFrame{-120, -120, 0});
}
void OutlawRider::resetCore() noexcept {
    pow_ = 0; desiredDb_ = 0; hop_ = 0; inPk_ = outPk_ = 0;
    gain_.snap(1.0f);
    std::fill(frames_.begin(), frames_.end(), RideFrame{-120, -120, 0});
    frameWrite_.store(0);
}
void OutlawRider::updateCore(const float* v, const float* old, bool) noexcept {
    if (!changed(v, old, {Target, MaxBoost, MinCut, Range, Speed, Sensitivity, RiderOut})) return;
    target_ = v[Target];
    range_ = v[Range] * 0.01f;
    maxBoost_ = v[MaxBoost] * range_;
    minCut_ = v[MinCut] * range_;
    sens_ = v[Sensitivity];
    out_ = dbToGain(v[RiderOut]);
    const float ms = expMap(v[Speed] * 0.01f, a::kSlowestMs, a::kFastestMs);
    powCoef_ = onePoleCoef(ms, sr_);
    gain_.prepare(ms * 0.5f, sr_);
}
void OutlawRider::processCore(float& l, float& r) noexcept {
    const float p = 0.5f * (l * l + r * r);
    pow_ = flushDenormal(p + powCoef_ * (pow_ - p));
    const float levelDb = powToDb(pow_);
    if (levelDb > sens_) desiredDb_ = clampf(target_ - levelDb, -minCut_, maxBoost_); // below: hold last gain
    gain_.setTarget(dbToGain(desiredDb_));
    const float g = gain_.next() * out_;
    inPk_ = std::max(inPk_, std::max(std::fabs(l), std::fabs(r)));
    l *= g; r *= g;
    outPk_ = std::max(outPk_, std::max(std::fabs(l), std::fabs(r)));
    if (++hop_ >= a::kDisplayHop) {
        hop_ = 0;
        const std::size_t w = frameWrite_.load(std::memory_order_relaxed);
        frames_[w % kFrames] = {gainToDb(inPk_), gainToDb(outPk_), gainToDb(gain_.value)};
        frameWrite_.store(w + 1, std::memory_order_release);
        gainPub_.store(gainToDb(gain_.value), std::memory_order_relaxed);
        inPk_ = outPk_ = 0;
    }
}
std::size_t OutlawRider::readFrames(RideFrame* dst, std::size_t max) const noexcept {
    const std::size_t w = frameWrite_.load(std::memory_order_acquire);
    const std::size_t n = std::min({max, w, kFrames});
    for (std::size_t i = 0; i < n; ++i) dst[i] = frames_[(w - n + i) % kFrames];
    return n;
}

namespace {
constexpr PresetValue kLead[] = {{"target", -18}};
constexpr PresetValue kRap[] = {{"target", -16}, {"speed", 75}, {"max_boost", 8}, {"min_cut", 10}};
constexpr PresetValue kBallad[] = {{"target", -20}, {"speed", 25}, {"max_boost", 5}, {"min_cut", 8}};
constexpr PresetValue kPodcast[] = {{"target", -20}, {"speed", 40}, {"sensitivity", -45}, {"max_boost", 10}};
constexpr PresetValue kGentle[] = {{"range", 50}, {"speed", 30}};
const Preset kPresets[] = {{"Lead Vocal Ride", "Vocal", kLead}, {"Fast Rap Ride", "Vocal", kRap},
    {"Slow Ballad Ride", "Vocal", kBallad}, {"Spoken Word", "Voice", kPodcast}, {"Gentle Half Range", "Vocal", kGentle}};
} // namespace
std::span<const Preset> riderPresets() noexcept { return kPresets; }

} // namespace dm::rd
