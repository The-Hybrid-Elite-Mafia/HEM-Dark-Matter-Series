#pragma once
// =====================================================================================================
//  HEM DARK MATTER SERIES - CENTRAL ASSUMPTIONS FILE
//  Every working guess (tuning numbers, mode voicings, defaults not given in the brief) lives here and
//  ONLY here. Each line is marked // ASSUMED. Values given directly by the brief are marked // BRIEF.
//  Nothing here claims to match any other product's internals - the brief gives layouts and labels,
//  not DSP curves, so all curves below are original HEM voicings, tuned by measurement in tests/.
// =====================================================================================================
#include "dm/core/Saturation.hpp"

namespace dm::assumed {

// ---- Global safety (every plugin) -------------------------------------------------------------------
inline constexpr float kCeilingDb          = -0.1f;   // BRIEF
inline constexpr float kLimiterLookaheadMs = 1.5f;    // ASSUMED
inline constexpr float kLimiterReleaseMs   = 80.0f;   // ASSUMED
inline constexpr float kAutoGainWindowMs   = 400.0f;  // ASSUMED (K-weighted RMS, LUFS-momentary-like)
inline constexpr float kAutoGainMaxDb      = 12.0f;   // ASSUMED
inline constexpr float kBypassFadeMs       = 10.0f;   // ASSUMED (click-free bypass)
inline constexpr float kHostTempoDefault   = 120.0f;  // ASSUMED until the host transport supplies BPM
inline constexpr int   kChunk              = 32;      // ASSUMED parameter update granularity (samples)

namespace pv { // =================================== HEM PHANTOM VOX ===================================

// ---- Defaults ---------------------------------------------------------------------------------------
inline constexpr float kDefComp       = 35.0f;  // ASSUMED
inline constexpr float kDefColor      = 127.0f; // BRIEF (COLOR 127%)
inline constexpr float kDefDeEss      = 30.0f;  // ASSUMED
inline constexpr float kDefFocusHz    = 5200.0f;// BRIEF (FOCUS 5.2 kHz)
inline constexpr float kDefGateDb     = -96.0f; // BRIEF range floor = gate effectively open
inline constexpr float kDefHpfHz      = 70.0f;  // ASSUMED (vocal rumble cut)
inline constexpr float kDefLpfHz      = 20000.0f; // ASSUMED (open)
inline constexpr float kDefReverbMix  = 25.0f;  // ASSUMED (protocol: 20-35%)
inline constexpr float kDefReverbTime = 2.3f;   // BRIEF (TIME 2.3s)
inline constexpr float kDefDelayMix   = 20.0f;  // ASSUMED (protocol: 20-35%)
inline constexpr int   kDefDelaySync  = 8;      // BRIEF (RATE 1/8d) - index into kSyncNames
inline constexpr float kDefVrbDuck    = 32.0f;  // BRIEF (VRB DUCK 32%)
inline constexpr float kDefDlyDuck    = 17.0f;  // BRIEF (DLY DUCK 17%)
inline constexpr float kDefSfx        = 25.0f;  // ASSUMED
inline constexpr float kDefEnhancer   = 15.0f;  // ASSUMED

// ---- Dynamics ---------------------------------------------------------------------------------------
inline constexpr float kCompThreshRangeDb = 36.0f;  // ASSUMED: COMP 100% -> threshold -36 dBFS
inline constexpr float kGateHysteresisDb  = 6.0f;   // ASSUMED
inline constexpr float kGateAttackMs      = 1.0f;   // ASSUMED
inline constexpr float kGateHoldMs        = 40.0f;  // ASSUMED
inline constexpr float kGateReleaseMs     = 150.0f; // ASSUMED
inline constexpr float kGateFloorDb       = -80.0f; // ASSUMED
inline constexpr float kDeEssSplitRatio   = 0.75f;  // ASSUMED: high band starts at FOCUS * 0.75
inline constexpr float kDeEssMaxGrDb      = 14.0f;  // ASSUMED at DE-ESS 100%
inline constexpr float kDeEssFloorDb      = -50.0f; // ASSUMED: no de-essing below this broadband level

enum class Detector { Peak, Rms, Opto };
struct DynModeTune {
    const char* name;
    Detector det; float ratio, kneeDb, attackMs, releaseMs; SatType sat; float satDrive;
    bool  fet; float fetRatio, fetAttackMs, fetReleaseMs, fetOffsetDb;   // second (peak) stage
    bool  upward; float upRatio, upRangeDb;                              // MOBIUS upward stage
};
inline constexpr int kDynModes = 7;
inline constexpr DynModeTune kDynModeTable[kDynModes] = { // ASSUMED (all voicings)
    {"SMOOTH",     Detector::Rms,  2.5f, 10.0f, 15.0f, 180.0f, SatType::Soft, 1.6f, false, 1, 1, 1, 0, false, 1, 0},
    {"OPTO + FET", Detector::Opto, 3.0f,  8.0f, 10.0f, 600.0f, SatType::Tube, 1.4f, true, 6.0f, 0.3f, 60.0f, 6.0f, false, 1, 0},
    {"VCA + FET",  Detector::Rms,  4.0f,  2.0f,  5.0f, 100.0f, SatType::Soft, 1.3f, true, 6.0f, 0.3f, 60.0f, 6.0f, false, 1, 0},
    {"TUBE + FET", Detector::Rms,  2.0f, 12.0f, 20.0f, 300.0f, SatType::Tube, 2.2f, true, 5.0f, 0.5f, 80.0f, 6.0f, false, 1, 0},
    {"VOX BUS",    Detector::Rms,  1.8f,  8.0f, 30.0f, 250.0f, SatType::Tape, 1.5f, false, 1, 1, 1, 0, false, 1, 0},
    {"FET",        Detector::Peak, 8.0f,  1.0f,  0.2f,  50.0f, SatType::Odd,  2.0f, false, 1, 1, 1, 0, false, 1, 0},
    {"MOBIUS",     Detector::Rms,  3.0f,  8.0f, 10.0f, 150.0f, SatType::Soft, 1.4f, false, 1, 1, 1, 0, true, 1.6f, 10.0f},
};
inline constexpr float kOptoFastReleaseMs = 60.0f; // ASSUMED: opto release slides fast->slow with depth

// ---- Tone -------------------------------------------------------------------------------------------
struct ToneModeTune {
    const char* name;
    float lowHz;  bool lowShelf;  float lowQ;
    float midHz;  float midQ;
    float highHz; bool highShelf; float highQ;
    float airHz;
    bool  propQ;              // Q rises with boost/cut amount
    SatType sat; float satDrive; // satDrive 1 = no saturation
};
inline constexpr int kToneModes = 7;
inline constexpr ToneModeTune kToneModeTable[kToneModes] = { // ASSUMED (all voicings)
    {"NYC",          110, true,  0.7f, 1200, 0.8f, 4500, true,  0.7f, 12000, false, SatType::Soft, 1.0f},
    {"MODERN",        90, true,  0.7f, 2500, 1.0f, 6000, false, 0.9f, 14000, true,  SatType::Soft, 1.0f},
    {"CLEAR",        200, false, 0.9f,  900, 1.2f, 3500, true,  0.7f, 10000, true,  SatType::Soft, 1.0f},
    {"VINTAGE",      100, true,  0.6f,  700, 0.6f, 5000, true,  0.6f, 10000, false, SatType::Tape, 1.6f},
    {"SHAPER",       150, false, 1.6f, 1800, 2.0f, 5500, false, 1.6f, 16000, true,  SatType::Soft, 1.0f},
    {"BRIT CLEAN",    80, true,  0.7f, 1500, 0.7f, 8000, true,  0.7f, 15000, false, SatType::Soft, 1.0f},
    {"VINTAGE TUBE", 100, true,  0.5f, 1000, 0.5f, 3000, true,  0.5f, 12000, false, SatType::Tube, 2.0f},
};
inline constexpr float kVoiceMaleScale   = 0.8f;  // ASSUMED: M switch moves LOW/MID down
inline constexpr float kVoiceHigherScale = 1.3f;  // ASSUMED: V switch moves LOW/MID up
inline constexpr float kFilterQ          = 0.707f;// ASSUMED (12 dB/oct Butterworth)

// ---- Space ------------------------------------------------------------------------------------------
struct SpaceModeTune {
    const char* name;
    float size, dampHz, diffusion, modDepthMs, modRateHz, early, predelayMs, width, decayMul;
    float dlyFeedback, dlyToneHz, dlyLowCutHz; bool dlyPingPong; float dlyWobbleMs, dlyDrive;
};
inline constexpr int kSpaceModes = 14;
inline constexpr SpaceModeTune kSpaceModeTable[kSpaceModes] = { // ASSUMED (all voicings)
    {"MASSIVE",     2.2f,  7000, 0.75f, 0.45f, 0.35f, 0.15f, 30, 1.3f, 1.0f, 0.40f,  5500, 200, true,  0.0f, 1.0f},
    {"DARK HALL",   1.8f,  3200, 0.70f, 0.30f, 0.30f, 0.20f, 25, 1.1f, 1.0f, 0.38f,  3000, 180, true,  0.0f, 1.2f},
    {"PLATE",       1.0f,  9000, 0.82f, 0.20f, 0.90f, 0.00f,  0, 1.0f, 1.0f, 0.30f,  7000, 250, true,  0.0f, 1.0f},
    {"DIGITAL",     1.2f, 14000, 0.60f, 0.00f, 0.10f, 0.10f, 10, 1.0f, 1.0f, 0.35f, 12000, 120, false, 0.0f, 1.0f},
    {"ROOM",        0.5f,  6000, 0.65f, 0.15f, 0.60f, 0.60f,  4, 0.8f, 0.5f, 0.25f,  6000, 200, false, 0.0f, 1.0f},
    {"SHORT PLATE", 0.7f,  8500, 0.80f, 0.20f, 0.90f, 0.00f,  0, 1.0f, 0.5f, 0.28f,  7000, 250, true,  0.0f, 1.0f},
    {"INFINIVERB",  2.5f,  6000, 0.80f, 0.60f, 0.20f, 0.00f, 40, 1.4f, 8.0f, 0.55f,  5000, 200, true,  0.8f, 1.0f},
    {"BRIGHT HALL", 1.9f, 12000, 0.75f, 0.35f, 0.40f, 0.20f, 20, 1.2f, 1.0f, 0.38f,  9000, 200, true,  0.0f, 1.0f},
    {"CHAMBER",     1.1f,  7500, 0.70f, 0.20f, 0.50f, 0.45f,  8, 0.9f, 0.8f, 0.30f,  6500, 200, false, 0.0f, 1.0f},
    {"CRYSTAL",     1.6f, 16000, 0.85f, 0.80f, 1.20f, 0.10f, 15, 1.4f, 1.2f, 0.42f, 14000, 300, true,  0.3f, 1.0f},
    {"VIBRANT",     1.4f, 10000, 0.70f, 1.00f, 2.00f, 0.25f, 12, 1.3f, 1.0f, 0.36f,  8000, 200, true,  0.5f, 1.0f},
    {"AMBIANCE",    0.6f,  9000, 0.60f, 0.10f, 0.50f, 0.70f,  2, 1.1f, 0.4f, 0.20f,  8000, 200, false, 0.0f, 1.0f},
    {"RETRO",       1.0f,  4500, 0.55f, 0.40f, 0.70f, 0.30f, 15, 0.9f, 0.9f, 0.45f,  3500, 300, false, 1.5f, 1.8f},
    {"LARGE ROOM",  0.9f,  7000, 0.70f, 0.20f, 0.50f, 0.55f,  8, 1.0f, 0.7f, 0.30f,  6500, 200, false, 0.0f, 1.0f},
};
inline constexpr float kSpaceSendHpfHz   = 120.0f;  // ASSUMED: keeps low end out of the returns
inline constexpr float kDuckAttackMs     = 5.0f;    // ASSUMED
inline constexpr float kDuckReleaseMs    = 250.0f;  // ASSUMED
inline constexpr float kDuckMaxDb        = 24.0f;   // ASSUMED duck depth at 100%
inline constexpr float kDuckFloorDb      = -40.0f;  // ASSUMED: vocal level where ducking starts
inline constexpr float kDuckSpanDb       = 30.0f;   // ASSUMED: dB above floor for full duck
inline constexpr float kSpaceToneLpfMin  = 1500.0f; // ASSUMED: SPACE TONE fully dark
inline constexpr float kSpaceToneHpfMax  = 1500.0f; // ASSUMED: SPACE TONE fully bright

// ---- SFX + Enhancer ---------------------------------------------------------------------------------
inline constexpr int kSfxModes = 11;
inline constexpr const char* kSfxNames[kSfxModes] = { // BRIEF (names)
    "TAPE SAT", "PITCH WIDENER", "CHORUS", "PHASER", "AMP", "TELEPHONE", "UNISON", "MEGAMOD",
    "ANALOG XL", "TUBE XL", "TAPE XL"};
inline constexpr float kTapeHfLossHz      = 9000.0f; // ASSUMED
inline constexpr float kTapeFlutterHz     = 5.5f;    // ASSUMED
inline constexpr float kTapeWowHz         = 0.7f;    // ASSUMED
inline constexpr float kWidenerCents      = 9.0f;    // ASSUMED at SFX 100%
inline constexpr float kWidenerWindowMs   = 40.0f;   // ASSUMED pitch-shifter grain
inline constexpr float kChorusRateHz      = 0.8f;    // ASSUMED
inline constexpr float kChorusDepthMs     = 3.0f;    // ASSUMED
inline constexpr float kChorusBaseMs      = 12.0f;   // ASSUMED
inline constexpr float kPhaserRateHz      = 0.35f;   // ASSUMED
inline constexpr float kPhaserMinHz       = 250.0f;  // ASSUMED
inline constexpr float kPhaserMaxHz       = 3500.0f; // ASSUMED
inline constexpr float kPhaserFeedback    = 0.55f;   // ASSUMED
inline constexpr float kAmpLowCutHz       = 110.0f;  // ASSUMED
inline constexpr float kAmpCabHz          = 4200.0f; // ASSUMED
inline constexpr float kAmpMidHz          = 900.0f;  // ASSUMED
inline constexpr float kPhoneLowHz        = 300.0f;  // ASSUMED (telephone band)
inline constexpr float kPhoneHighHz       = 3400.0f; // ASSUMED
inline constexpr float kUnisonDetuneCents = 14.0f;   // ASSUMED at SFX 100%
inline constexpr float kMegaTremHz        = 4.0f;    // ASSUMED
inline constexpr float kEnhancerHpfHz     = 3000.0f; // ASSUMED
inline constexpr float kEnhancerDrive     = 4.0f;    // ASSUMED
inline constexpr float kEnhancerMaxMix    = 0.35f;   // ASSUMED
inline constexpr float kEnhancerBodyHz    = 180.0f;  // ASSUMED low-mid warmth shelf
inline constexpr float kEnhancerBodyDb    = 2.0f;    // ASSUMED at ENHANCER 100%

} // namespace pv
} // namespace dm::assumed
