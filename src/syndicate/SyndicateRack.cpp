// HEM SYNDICATE RACK - rack engine + factory presets (all values original HEM settings).
#include "dm/syndicate/SyndicateRack.hpp"

namespace dm::sr {

void SyndicateRack::prepareCore(float sr) {
    sat_.prepare(sr); comp_.prepare(sr); trans_.prepare(sr); tone_.prepare(sr); mod_.prepare(sr); max_.prepare(sr);
}
void SyndicateRack::resetCore() noexcept {
    sat_.reset(); comp_.reset(); trans_.reset(); tone_.reset(); mod_.reset(); max_.reset();
}
void SyndicateRack::updateCore(const float* v, const float* old, bool) noexcept {
    auto on = [&](std::size_t i) { return v[i] >= 0.5f; };
    const bool anySolo = on(kSatOn + 1) || on(kDynOn + 1) || on(kToneOn + 1) || on(kModOn + 1) || on(kMaxOn + 1);
    auto active = [&](std::size_t i) { return on(i) && (!anySolo || on(i + 1)); };
    satOn_ = active(kSatOn); dynOn_ = active(kDynOn); toneOn_ = active(kToneOn); modOn_ = active(kModOn); maxOn_ = active(kMaxOn);
    route_ = std::min(std::max(static_cast<int>(std::lround(v[kRoute])), 0), 2);
    auto o = [&](std::size_t first) { return old ? old + first : nullptr; };
    sat_.update(v + kSat, o(kSat));
    comp_.update(v + kComp, o(kComp));
    trans_.update(v + kTrans, o(kTrans));
    tone_.update(v + kTone, o(kTone));
    mod_.update(v + kMod, o(kMod));
    max_.update(v + kMax, o(kMax));
}
void SyndicateRack::processCore(float& l, float& r) noexcept {
    if (toneOn_ && route_ == 0) tone_.process(l, r);
    if (satOn_) sat_.process(l, r);
    if (dynOn_) {
        comp_.process(l, r, keyFrame());
        if (trans_.active()) trans_.process(l, r);
    }
    if (toneOn_ && route_ == 1) tone_.process(l, r);
    if (modOn_) mod_.process(l, r);
    if (toneOn_ && route_ == 2) tone_.process(l, r);
    if (maxOn_) max_.process(l, r);
}

namespace {
constexpr PresetValue kMixGlue[] = {{"sat_mode", 2}, {"saturate", 25}, {"comp_mode", 4}, {"comp", 30}, {"color", 30},
    {"max_mode", 5}, {"multi", 15}, {"mod", 10}};
constexpr PresetValue kMixLoud[] = {{"sat_mode", 4}, {"saturate", 30}, {"comp_mode", 4}, {"comp", 35}, {"max_mode", 1},
    {"multi", 35}, {"max", 20}, {"clip", 3}, {"air", 1.5f}, {"mod_on", 0}};
constexpr PresetValue kDrums[] = {{"sat_mode", 5}, {"saturate", 30}, {"comp_mode", 2}, {"comp", 50}, {"transient", 40},
    {"max_mode", 4}, {"multi", 30}, {"mod_on", 0}};
constexpr PresetValue kKick[] = {{"sat_mode", 0}, {"saturate", 20}, {"transient", 50}, {"shape", 30}, {"sub", 3},
    {"low", -2}, {"mid", -3}, {"mod_on", 0}, {"max_mode", 2}};
constexpr PresetValue kSnare[] = {{"sat_mode", 1}, {"saturate", 35}, {"transient", 40}, {"shape", 40}, {"hi_mid", 3},
    {"low", -1}, {"mod_on", 0}};
constexpr PresetValue kPerc[] = {{"transient", 20}, {"air", 3}, {"high", 1}, {"mod_mode", 1}, {"mod", 40}, {"tone_mode", 5}};
constexpr PresetValue kBass[] = {{"sat_mode", 7}, {"saturate", 40}, {"comp_mode", 0}, {"comp", 40}, {"sub", 2},
    {"mid", -1}, {"mod_on", 0}, {"max_mode", 2}};
constexpr PresetValue k808[] = {{"sat_mode", 5}, {"saturate", 25}, {"comp_mode", 1}, {"comp", 40}, {"hpf", 25},
    {"slope", 1}, {"max_mode", 2}, {"multi", 25}, {"mod_on", 0}};
constexpr PresetValue kAcEl[] = {{"sat_mode", 0}, {"saturate", 20}, {"comp_mode", 0}, {"comp", 30}, {"tone_mode", 1},
    {"low", -2}, {"hi_mid", 2}, {"mod_mode", 0}, {"mod", 15}};
constexpr PresetValue kAiryCrush[] = {{"sat_mode", 4}, {"comp_mode", 2}, {"comp", 55}, {"air", 4}, {"high", 2},
    {"max_mode", 0}, {"multi", 35}, {"mod_mode", 1}, {"mod", 35}};
constexpr PresetValue kCleanLift[] = {{"sat_on", 0}, {"comp_mode", 5}, {"comp", 25}, {"tone_mode", 0}, {"high", 2},
    {"air", 2}, {"low", -1}, {"mod_on", 0}};
constexpr PresetValue kDark[] = {{"sat_mode", 3}, {"saturate", 35}, {"comp_mode", 7}, {"comp", 30}, {"tone_mode", 2},
    {"high", -3}, {"air", -4}, {"low", 1}, {"mod_mode", 5}, {"mod", 15}};
constexpr PresetValue kOpen[] = {{"sat_mode", 2}, {"saturate", 15}, {"comp_mode", 4}, {"comp", 20}, {"tone_mode", 3},
    {"air", 3}, {"sub", -1}, {"mod_mode", 2}, {"mod", 30}};

const Preset kPresets[] = {
    {"Mixbus Glue",       "01 Mixbus",           kMixGlue},
    {"Mixbus Loud",       "01 Mixbus",           kMixLoud},
    {"Drum Bus Smash",    "02 Drums",            kDrums},
    {"Kick Punch",        "03 Kick",             kKick},
    {"Snare Crack",       "04 Snares and Claps", kSnare},
    {"Perc Sparkle",      "05 Percussion",       kPerc},
    {"Bass Weight",       "06 Bass",             kBass},
    {"808 Control",       "07 Subs and 808s",    k808},
    {"Acoustic Electric", "08 Acoustic Guitar",  kAcEl},
    {"Airy Crush",        "08 Acoustic Guitar",  kAiryCrush},
    {"Clean Lift",        "08 Acoustic Guitar",  kCleanLift},
    {"Dark",              "08 Acoustic Guitar",  kDark},
    {"Open",              "08 Acoustic Guitar",  kOpen},
};
} // namespace

std::span<const Preset> rackPresets() noexcept { return kPresets; }

} // namespace dm::sr
