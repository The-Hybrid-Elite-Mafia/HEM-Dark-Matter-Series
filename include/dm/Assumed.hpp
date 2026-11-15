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

namespace sr { // ================================ HEM SYNDICATE RACK =====================================

// ---- SATURATE (8 modes) -----------------------------------------------------------------------------
struct SatModeTune {
    const char* name; SatType sat; float driveMax; float preEmphDb; float hfLossHz;
    float wowMs; float flutterMs; float lowBumpDb; float lowBumpHz;
};
inline constexpr int kSatModes = 8;
inline constexpr SatModeTune kSatModeTable[kSatModes] = { // ASSUMED (all voicings; names BRIEF)
    {"N STRIP",  SatType::Tube, 4.0f,  0.0f, 20000.0f, 0.00f, 0.00f, 1.0f,  60.0f},
    {"A STRIP",  SatType::Odd,  4.0f,  2.0f, 20000.0f, 0.00f, 0.00f, 0.0f, 100.0f},
    {"2\" TAPE", SatType::Tape, 5.0f,  3.0f, 16000.0f, 0.05f, 0.01f, 1.5f,  70.0f},
    {"CASSETTE", SatType::Tape, 6.0f,  4.0f,  9000.0f, 0.25f, 0.04f, 0.5f,  90.0f},
    {"BUS SAT",  SatType::Soft, 3.0f,  0.0f, 20000.0f, 0.00f, 0.00f, 0.0f, 100.0f},
    {"DRIVE",    SatType::Odd, 10.0f,  1.0f, 18000.0f, 0.00f, 0.00f, 0.0f, 100.0f},
    {"S STRIP",  SatType::Odd,  3.0f,  1.5f, 20000.0f, 0.00f, 0.00f, 0.0f, 100.0f},
    {"M TUBE",   SatType::Tube, 6.0f,  0.0f, 17000.0f, 0.00f, 0.00f, 1.0f,  80.0f},
};
inline constexpr float kSatDefault       = 30.0f;  // ASSUMED
inline constexpr float kCharacterTiltDb  = 3.0f;   // ASSUMED: CRISP/WARM full travel
inline constexpr float kCharacterPivotHz = 1000.0f;// ASSUMED

// ---- DYNAMICS: COMP (8 modes) + TRANSIENT -----------------------------------------------------------
struct CompModeTune {
    const char* name; int detector; /*0 peak 1 rms 2 opto*/ float ratio, kneeDb, attackMs, releaseMs;
    SatType sat; float satDrive;
};
inline constexpr int kCompModes = 8;
inline constexpr CompModeTune kCompModeTable[kCompModes] = { // ASSUMED (all voicings; names BRIEF)
    {"OPTO X",  2, 3.0f, 10.0f, 10.0f, 500.0f, SatType::Tube, 1.4f},
    {"VINTAGE", 1, 4.0f,  6.0f,  8.0f, 250.0f, SatType::Tube, 1.8f},
    {"FET",     0, 8.0f,  1.0f,  0.3f,  60.0f, SatType::Odd,  2.0f},
    {"SOLID",   1, 4.0f,  2.0f,  3.0f, 120.0f, SatType::Soft, 1.2f},
    {"GLUE",    1, 2.0f,  8.0f, 30.0f, 300.0f, SatType::Soft, 1.3f},
    {"SATIN",   1, 1.6f, 14.0f, 25.0f, 400.0f, SatType::Tape, 1.4f},
    {"S COMP",  1, 4.0f,  3.0f, 10.0f, 100.0f, SatType::Odd,  1.3f},
    {"VACUUM",  2, 2.5f, 12.0f, 15.0f, 700.0f, SatType::Tube, 2.4f},
};
inline constexpr float kCompDefault      = 30.0f;  // ASSUMED
inline constexpr float kCompColorDefault = 40.0f;  // ASSUMED
inline constexpr float kSidechainDefault = 35.0f;  // BRIEF (35% AMOUNT)
inline constexpr float kSidechainMaxHz   = 300.0f; // ASSUMED: SIDECHAIN 100% -> 300 Hz detector high-pass
inline constexpr float kCompThreshRangeDb= 30.0f;  // ASSUMED
inline constexpr float kTransMaxDb       = 12.0f;  // ASSUMED: TRANSIENT +/-100% -> +/-12 dB on attacks
inline constexpr float kTransFastMs      = 1.0f;   // ASSUMED
inline constexpr float kTransSlowMinMs   = 10.0f;  // ASSUMED: SHAPE 0%
inline constexpr float kTransSlowMaxMs   = 80.0f;  // ASSUMED: SHAPE 100%

// ---- TONE (8 modes, 6 bands) ------------------------------------------------------------------------
struct ToneBandTune { float hz; float q; int type; /*0 peak 1 low shelf 2 high shelf*/ };
struct RackToneModeTune { const char* name; ToneBandTune band[6]; bool propQ; SatType sat; float satDrive; };
inline constexpr int kToneModes = 8;
inline constexpr RackToneModeTune kToneModeTable[kToneModes] = { // ASSUMED (all voicings; names BRIEF)
    {"CLEAN",     {{40,0.7f,1},{120,0.9f,0},{800,0.9f,0},{2500,0.9f,0},{7000,0.9f,0},{14000,0.7f,2}}, false, SatType::Soft, 1.0f},
    {"PRESENT",   {{45,0.7f,1},{150,1.0f,0},{1200,1.1f,0},{3500,1.2f,0},{8000,1.0f,0},{15000,0.7f,2}}, true,  SatType::Soft, 1.0f},
    {"WARM",      {{35,0.6f,1},{100,0.7f,0},{600,0.7f,0},{2000,0.8f,0},{6000,0.7f,2},{12000,0.6f,2}}, false, SatType::Tape, 1.4f},
    {"BLUE",      {{50,0.5f,1},{100,0.5f,1},{700,0.5f,0},{3000,0.5f,0},{10000,0.5f,2},{16000,0.5f,2}}, false, SatType::Soft, 1.0f},
    {"TUBE",      {{40,0.6f,1},{110,0.8f,0},{900,0.8f,0},{2800,0.8f,0},{7500,0.8f,0},{13000,0.6f,2}}, false, SatType::Tube, 1.8f},
    {"CLARITY",   {{45,0.8f,1},{180,1.4f,0},{1500,1.6f,0},{4000,1.6f,0},{9000,1.4f,0},{17000,0.8f,2}}, true,  SatType::Soft, 1.0f},
    {"PASSIVE",   {{30,0.4f,1},{90,0.4f,0},{500,0.4f,0},{2200,0.4f,0},{6500,0.4f,0},{12000,0.4f,2}}, false, SatType::Soft, 1.0f},
    {"S CHANNEL", {{50,0.7f,1},{140,0.8f,0},{1000,1.0f,0},{3200,1.0f,0},{8500,0.9f,2},{15000,0.7f,2}}, true,  SatType::Odd,  1.2f},
};
inline constexpr float kToneHpfDefault = 10.0f;    // ASSUMED (open)
inline constexpr float kToneLpfDefault = 22000.0f; // ASSUMED (open)

// ---- MODULATE (6 modes) -----------------------------------------------------------------------------
struct ModModeTune { const char* name; int algo; float rateHz; float baseMs; float depthMs; int voices; float feedback; };
inline constexpr int kModModes = 6;
inline constexpr ModModeTune kModModeTable[kModModes] = { // ASSUMED (all voicings; names BRIEF)
    {"J CHORUS",   0, 0.55f,  7.0f, 2.5f, 2, 0.0f},
    {"ST ENHANCE", 1, 0.10f,  0.8f, 0.6f, 1, 0.0f},
    {"DOUBLER",    2, 0.25f, 22.0f, 1.0f, 2, 0.0f},
    {"THICK",      0, 0.90f, 10.0f, 3.0f, 4, 0.0f},
    {"M PHASER",   3, 0.30f,  0.0f, 0.0f, 4, 0.5f},
    {"RETRO",      4, 0.20f,  3.0f, 2.0f, 1, 0.6f},
};
inline constexpr float kModDefault     = 20.0f; // ASSUMED
inline constexpr float kModDepthDefault= 50.0f; // ASSUMED
inline constexpr float kDriftMaxCents  = 12.0f; // ASSUMED at DRIFT 100%
inline constexpr float kDriftRateHz    = 0.13f; // ASSUMED
inline constexpr float kAnalogWowMs    = 0.6f;  // ASSUMED at ANALOG 100%
inline constexpr float kAnalogWowHz    = 0.5f;  // ASSUMED
inline constexpr float kAnalogFlutterHz= 6.0f;  // ASSUMED

// ---- MAXIMIZE (7 modes) -----------------------------------------------------------------------------
struct MaxModeTune {
    const char* name; float downRatio; float upRatio; float upRangeDb; float attackMs; float releaseMs;
    float lowGainDb, midGainDb, highGainDb; float parallel; /*1 = fully processed*/ SatType sat; float satDrive;
};
inline constexpr int kMaxModes = 7;
inline constexpr MaxModeTune kMaxModeTable[kMaxModes] = { // ASSUMED (all voicings). Mode 0 renamed from the brief's
    // third-party name to the HEM name "TRIPLE CRUSH" at Steven's request.
    {"TRIPLE CRUSH", 6.0f, 3.0f, 18.0f,  2.0f,  60.0f, 0.0f, 0.0f, 0.0f, 1.0f, SatType::Soft, 1.0f},
    {"MASSIVE",      4.0f, 2.0f, 12.0f,  5.0f, 120.0f, 1.5f, 0.0f, 1.0f, 1.0f, SatType::Soft, 1.3f},
    {"TIGHT",        5.0f, 1.3f,  4.0f,  1.0f,  40.0f, 0.0f, 0.0f, 0.0f, 1.0f, SatType::Soft, 1.0f},
    {"OPEN",         2.5f, 2.0f, 10.0f, 10.0f, 200.0f, 0.0f, 0.0f, 1.5f, 1.0f, SatType::Soft, 1.0f},
    {"CRUSH",       10.0f, 4.0f, 24.0f,  0.5f,  30.0f, 0.0f, 1.0f, 0.0f, 1.0f, SatType::Odd,  2.0f},
    {"NY MIXBUS",    8.0f, 1.0f,  0.0f,  1.0f,  80.0f, 0.0f, 0.0f, 0.0f, 0.5f, SatType::Odd,  1.5f},
    {"LA MIXBUS",    3.0f, 1.5f,  6.0f, 20.0f, 500.0f, 0.5f, 0.0f, 0.5f, 1.0f, SatType::Tube, 1.3f},
};
inline constexpr float kMultiDefault   = 20.0f;  // ASSUMED
inline constexpr float kMaxDefault     = 0.0f;   // ASSUMED (no loudness push on load)
inline constexpr float kMaxPushDb      = 12.0f;  // ASSUMED: MAX 100% -> +12 dB into the clipper
inline constexpr float kXoverLowDef    = 250.0f; // BRIEF
inline constexpr float kXoverHighDef   = 5200.0f;// BRIEF
inline constexpr float kSoftClipKneeDb = -1.0f;  // ASSUMED

} // namespace sr

namespace ax { // ================================== HEM APEX CORE =========================================
struct ApexEngineTune {
    const char* name; float xoverLow, xoverHigh; float attackMs, releaseMs; float ratioMax;
    float containDb; float boostDb; SatType sat; float satDrive; bool midSide; float airDb;
};
inline constexpr int kEngines = 6;
inline constexpr ApexEngineTune kEngineTable[kEngines] = { // ASSUMED (all voicings; names BRIEF)
    {"PRIMA",    200, 4000, 10.0f, 150.0f, 10.0f,  6.0f,  6.0f, SatType::Soft, 1.3f, false, 0.0f},
    {"AIR-1B",   250, 6000,  5.0f, 120.0f,  8.0f,  6.0f,  5.0f, SatType::Soft, 1.2f, false, 1.5f},
    {"MULTIMAX", 150, 3000,  2.0f,  80.0f, 16.0f, 10.0f,  4.0f, SatType::Odd,  1.4f, false, 0.0f},
    {"HELIX",    200, 4500,  8.0f, 140.0f, 10.0f,  6.0f,  6.0f, SatType::Soft, 1.2f, true,  0.5f},
    {"TITAN",    120, 2500,  1.0f,  60.0f, 20.0f,  8.0f,  8.0f, SatType::Odd,  2.0f, false, 0.0f},
    {"VELVET",   300, 5000, 30.0f, 400.0f,  4.0f,  5.0f,  6.0f, SatType::Tube, 1.4f, false, 0.5f},
};
inline constexpr float kDefAmount    = 127.0f; // BRIEF
inline constexpr float kDefContain   = 30.0f;  // ASSUMED (brief shows the arc, not a value)
inline constexpr float kDefBoost     = 20.0f;  // ASSUMED
inline constexpr float kDefPeaks     = 50.0f;  // BRIEF
inline constexpr float kDefCurve     = 27.0f;  // BRIEF
inline constexpr float kDefColorLow  = 23.0f;  // BRIEF
inline constexpr float kDefColorHigh = 7.0f;   // BRIEF
inline constexpr float kMonoSafeHz   = 120.0f; // ASSUMED
inline constexpr float kAmountDrivePerUnitDb = 6.0f; // ASSUMED: each 100% above 100% adds 6 dB drive
inline constexpr float kContainOffsetDb = 3.0f;  // ASSUMED: band may sit this far above the band average freely
inline constexpr float kBoostThreshDb   = -30.0f;// ASSUMED: upward stage works below this band level
inline constexpr float kColorLowHz      = 180.0f;// ASSUMED
inline constexpr float kColorHighHz     = 5000.0f;// ASSUMED
} // namespace ax
} // namespace dm::assumed
