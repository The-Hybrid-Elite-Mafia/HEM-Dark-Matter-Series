// HEM PHANTOM VOX - parameter table and factory presets.
#include "dm/phantom_vox/PhantomVox.hpp"

namespace dm::pv {
namespace {

using namespace dm::assumed::pv;

constexpr const char* kDynNames[kDynModes] = {
    kDynModeTable[0].name, kDynModeTable[1].name, kDynModeTable[2].name, kDynModeTable[3].name,
    kDynModeTable[4].name, kDynModeTable[5].name, kDynModeTable[6].name};
constexpr const char* kToneNames[kToneModes] = {
    kToneModeTable[0].name, kToneModeTable[1].name, kToneModeTable[2].name, kToneModeTable[3].name,
    kToneModeTable[4].name, kToneModeTable[5].name, kToneModeTable[6].name};
constexpr const char* kSpaceNames[kSpaceModes] = {
    kSpaceModeTable[0].name,  kSpaceModeTable[1].name,  kSpaceModeTable[2].name,  kSpaceModeTable[3].name,
    kSpaceModeTable[4].name,  kSpaceModeTable[5].name,  kSpaceModeTable[6].name,  kSpaceModeTable[7].name,
    kSpaceModeTable[8].name,  kSpaceModeTable[9].name,  kSpaceModeTable[10].name, kSpaceModeTable[11].name,
    kSpaceModeTable[12].name, kSpaceModeTable[13].name};
constexpr const char* kVoiceNames[2] = {"M", "V"};

using C = std::span<const char* const>;

const std::array<ParamDesc, kParamCount> kTable = {{
    {"input",       "INPUT",        -24, 24, 0, "dB"},
    {"output",      "OUTPUT",       -24, 24, 0, "dB"},
    {"bypass",      "BYPASS",         0,  1, 0, "toggle"},
    {"autogain",    "AUTO GAIN",      0,  1, 1, "toggle"},

    {"dyn_on",      "DYNAMICS",       0,  1, 1, "toggle"},
    {"dyn_solo",    "S",              0,  1, 0, "toggle"},
    {"dyn_mode",    "DYNAMICS MODE",  0, kDynModes - 1, 0, "", C(kDynNames)},
    {"dyn_trim",    "TRIM",         -12, 12, 0, "dB"},
    {"gate",        "GATE",         -96,  0, kDefGateDb, "dB"},
    {"comp",        "COMP",           0, 100, kDefComp, "%"},
    {"color",       "COLOR",          0, 200, kDefColor, "%"},
    {"deess",       "DE-ESS",         0, 100, kDefDeEss, "%"},
    {"focus",       "FOCUS",       2000, 12000, kDefFocusHz, "Hz"},

    {"tone_on",     "TONE",           0,  1, 1, "toggle"},
    {"tone_solo",   "S",              0,  1, 0, "toggle"},
    {"tone_mode",   "TONE MODE",      0, kToneModes - 1, 0, "", C(kToneNames)},
    {"tone_trim",   "TRIM",         -12, 12, 0, "dB"},
    {"low",         "LOW",          -12, 12, 0, "dB"},
    {"mid",         "MID",          -12, 12, 0, "dB"},
    {"high",        "HIGH",         -12, 12, 0, "dB"},
    {"air",         "AIR+",         -12, 12, 0, "dB"},
    {"voice",       "M / V",          0,  1, 0, "", C(kVoiceNames)},
    {"hpf",         "HPF",           20, 1000, kDefHpfHz, "Hz"},
    {"lpf",         "LPF",         2000, 20000, kDefLpfHz, "Hz"},

    {"space_on",    "SPACE",          0,  1, 1, "toggle"},
    {"space_solo",  "S",              0,  1, 0, "toggle"},
    {"space_mode",  "SPACE MODE",     0, kSpaceModes - 1, 0, "", C(kSpaceNames)},
    {"space_trim",  "TRIM",         -12, 12, 0, "dB"},
    {"reverb",      "REVERB",         0, 100, kDefReverbMix, "%"},
    {"reverb_time", "TIME",         0.2f, 20, kDefReverbTime, "s"},
    {"delay",       "DELAY",          0, 100, kDefDelayMix, "%"},
    {"delay_rate",  "RATE",           0, kSyncCount - 1, kDefDelaySync, "", C(kSyncNames)},
    {"vrb_duck",    "VRB DUCK",       0, 100, kDefVrbDuck, "%"},
    {"dly_duck",    "DLY DUCK",       0, 100, kDefDlyDuck, "%"},
    {"space_tone",  "SPACE TONE",  -100, 100, 0, "%"},
    {"tempo",       "TEMPO",         40, 240, dm::assumed::kHostTempoDefault, "BPM"},

    {"sfx_on",      "SFX",            0,  1, 1, "toggle"},
    {"sfx_solo",    "S",              0,  1, 0, "toggle"},
    {"sfx_mode",    "SFX MODE",       0, kSfxModes - 1, 0, "", C(kSfxNames)},
    {"sfx_trim",    "TRIM",         -12, 12, 0, "dB"},
    {"sfx",         "SFX",            0, 100, kDefSfx, "%"},
    {"enhancer",    "ENHANCER",       0, 100, kDefEnhancer, "%"},

    {"spc_post",    "SPC POST",       0,  1, 0, "toggle"},
    {"flt_post",    "FLT POST",       0,  1, 0, "toggle"},
}};

// ---- Factory presets (all values are original HEM settings) -----------------------------------------
constexpr PresetValue kClearLead[] = {{"dyn_mode", 0}};
constexpr PresetValue kRapUpfront[] = {
    {"dyn_mode", 2}, {"comp", 55}, {"color", 140}, {"deess", 40}, {"tone_mode", 1}, {"mid", 2}, {"high", 3},
    {"air", 2}, {"hpf", 90}, {"space_mode", 4}, {"reverb", 20}, {"reverb_time", 0.9f}, {"delay", 22},
    {"delay_rate", 7}, {"sfx_mode", 8}, {"sfx", 30}, {"enhancer", 25}};
constexpr PresetValue kAiryPop[] = {
    {"dyn_mode", 1}, {"comp", 45}, {"deess", 45}, {"focus", 6500}, {"tone_mode", 2}, {"voice", 1},
    {"low", -2}, {"high", 2}, {"air", 5}, {"hpf", 110}, {"space_mode", 7}, {"reverb", 30},
    {"reverb_time", 2.8f}, {"delay", 20}, {"delay_rate", 5}, {"sfx_mode", 1}, {"sfx", 35}, {"enhancer", 30}};
constexpr PresetValue kDarkBallad[] = {
    {"dyn_mode", 3}, {"comp", 40}, {"color", 160}, {"tone_mode", 6}, {"low", 2}, {"high", -2},
    {"space_mode", 1}, {"reverb", 35}, {"reverb_time", 4.5f}, {"vrb_duck", 45}, {"delay", 15},
    {"delay_rate", 4}, {"space_tone", -35}, {"sfx_mode", 9}, {"sfx", 25}, {"enhancer", 10}};
constexpr PresetValue kPhoneVerse[] = {
    {"dyn_mode", 5}, {"comp", 60}, {"tone_mode", 4}, {"mid", 4}, {"hpf", 300}, {"lpf", 4000},
    {"space_mode", 11}, {"reverb", 20}, {"reverb_time", 0.6f}, {"delay", 20}, {"delay_rate", 10},
    {"sfx_mode", 5}, {"sfx", 80}, {"enhancer", 0}};
constexpr PresetValue kGospelStack[] = {
    {"dyn_mode", 4}, {"comp", 40}, {"color", 110}, {"tone_mode", 5}, {"voice", 1}, {"air", 3},
    {"space_mode", 8}, {"reverb", 30}, {"reverb_time", 2.0f}, {"delay", 0}, {"sfx_mode", 6}, {"sfx", 40},
    {"enhancer", 20}};
constexpr PresetValue kOutlawTape[] = {
    {"dyn_mode", 0}, {"comp", 40}, {"color", 150}, {"tone_mode", 3}, {"low", 1}, {"high", -1},
    {"space_mode", 12}, {"reverb", 22}, {"reverb_time", 1.6f}, {"delay", 25}, {"delay_rate", 4},
    {"sfx_mode", 10}, {"sfx", 45}, {"enhancer", 5}};
constexpr PresetValue kAdlibThrow[] = {
    {"dyn_mode", 6}, {"comp", 50}, {"tone_mode", 1}, {"hpf", 200}, {"space_mode", 6}, {"reverb", 35},
    {"reverb_time", 6.0f}, {"delay", 35}, {"delay_rate", 8}, {"dly_duck", 40}, {"spc_post", 1},
    {"sfx_mode", 7}, {"sfx", 50}, {"enhancer", 20}};

const Preset kPresets[] = {
    {"Clear Lead Vocals",   "Lead",    kClearLead},
    {"Rap Lead Upfront",    "Lead",    kRapUpfront},
    {"Airy Pop Lead",       "Lead",    kAiryPop},
    {"Dark Hall Ballad",    "Lead",    kDarkBallad},
    {"Phone Verse",         "FX",      kPhoneVerse},
    {"Gospel Stack Glue",   "Backing", kGospelStack},
    {"Outlaw Tape Vocal",   "Lead",    kOutlawTape},
    {"Ad-Lib Throw",        "FX",      kAdlibThrow},
};

} // namespace

const std::array<ParamDesc, kParamCount>& paramTable() noexcept { return kTable; }
std::span<const Preset> factoryPresets() noexcept { return kPresets; }

} // namespace dm::pv
