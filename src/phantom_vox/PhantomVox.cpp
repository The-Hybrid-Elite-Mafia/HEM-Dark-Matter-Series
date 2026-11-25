// HEM PHANTOM VOX - engine.
#include "dm/phantom_vox/PhantomVox.hpp"

namespace dm::pv {

using namespace dm::assumed::pv;
namespace ag = dm::assumed;

PhantomVox::PhantomVox() : params_(paramTable()) { cached_.fill(-1.0e30f); }

void PhantomVox::prepare(float sampleRate, int maxBlock) {
    sr_ = sampleRate;
    maxBlock_ = std::max(1, maxBlock);

    for (Smoother* s : {&inTrim_, &outTrim_, &dynTrim_, &toneTrim_, &spaceTrim_, &sfxTrim_, &revMix_, &dlyMix_})
        s->prepare(20.0f, sr_);
    bypassMix_.prepare(ag::kBypassFadeMs / 4.6f, sr_);
    cleanMix_.prepare(ag::kBypassFadeMs / 4.6f, sr_);

    autoGain_.prepare(sr_, ag::kAutoGainWindowMs, ag::kAutoGainMaxDb);
    limiter_.prepare(sr_, ag::kLimiterLookaheadMs, ag::kLimiterReleaseMs);
    for (auto& d : dryDelay_) d.prepare(limiter_.latency() + 8);

    gateEnv_.prepare(0.5f, 50.0f, sr_);
    gateAtt_ = onePoleCoef(kGateAttackMs, sr_);
    gateRel_ = onePoleCoef(kGateReleaseMs, sr_);
    deEssBand_.prepare(0.5f, 60.0f, sr_);
    deEssBroad_.prepare(0.5f, 60.0f, sr_);
    duckEnv_.prepare(kDuckAttackMs, kDuckReleaseMs, sr_);

    reverb_.prepare(sr_);
    delay_.prepare(sr_);
    sfx_.prepare(sr_);
    analyzer_.prepare(sr_);

    for (PeakMeter* m : {&meters_.inL, &meters_.inR, &meters_.outL, &meters_.outR, &meters_.revIn, &meters_.revOut,
                         &meters_.dlyIn, &meters_.dlyOut})
        m->prepare(sr_, ag::kChunk);

    reset();
}

void PhantomVox::reset() noexcept {
    autoGain_.reset();
    limiter_.reset();
    for (auto& d : dryDelay_) d.reset();
    gateEnv_.reset(); gateGain_ = 1.0f; gateHold_ = 0; gateOpen_ = true;
    envA_.reset(); envFet_.reset(); lastGrA_ = 0;
    deEssBand_.reset(); deEssBroad_.reset(); deEssDetect_.reset(); deEssSplit_.reset();
    for (StereoBiquad* b : {&eqLow_, &eqMid_, &eqHigh_, &eqAir_, &hpf_, &lpf_, &sendHp_, &retHp_, &retLp_}) b->reset();
    reverb_.reset();
    delay_.reset();
    duckEnv_.reset();
    sfx_.reset();
    cached_.fill(-1.0e30f);
    updateFromParams(true);
    inTrim_.snap(inTrim_.target); outTrim_.snap(outTrim_.target);
    for (Smoother* s : {&dynTrim_, &toneTrim_, &spaceTrim_, &sfxTrim_, &revMix_, &dlyMix_, &bypassMix_, &cleanMix_}) s->snap(s->target);
}

std::span<const PresetValue> PhantomVox::presetBase() noexcept {
    static constexpr PresetValue kBase[] = {{"comp", 35}, {"color", 127}, {"deess", 30}, {"hpf", 70},
                                            {"reverb", 25}, {"delay", 20}, {"sfx", 25}, {"enhancer", 15}};
    return kBase;
}

bool PhantomVox::loadPreset(std::size_t index) noexcept {
    const auto presets = factoryPresets();
    if (index >= presets.size()) return false;
    params_.resetToDefaults();
    // The factory presets were written over the old (coloured) defaults: put those back first, so every preset
    // still sounds as it did (the plug-in itself now loads with its colour at zero).
    for (const auto& v : presetBase()) {
        const int i = params_.indexOf(v.id);
        if (i >= 0) params_.set(static_cast<std::size_t>(i), v.value);
    }
    for (const auto& v : presets[index].values) {
        const int i = params_.indexOf(v.id);
        if (i >= 0) params_.set(static_cast<std::size_t>(i), v.value);
    }
    return true;
}

// -----------------------------------------------------------------------------------------------------
void PhantomVox::updateFromParams(bool force) noexcept {
    std::array<float, kParamCount> v{};
    bool changed = force;
    for (std::size_t i = 0; i < kParamCount; ++i) {
        v[i] = params_.get(i);
        if (v[i] != cached_[i]) changed = true;
    }
    if (!changed) return;
    auto diff = [&](std::initializer_list<std::size_t> ids) {
        if (force) return true;
        for (auto i : ids) if (v[i] != cached_[i]) return true;
        return false;
    };
    auto on = [&](std::size_t i) { return v[i] >= 0.5f; };
    auto idx = [&](std::size_t i) { return static_cast<int>(std::lround(v[i])); };

    // ---- Global + solo logic ----
    inTrim_.setTarget(dbToGain(v[InputTrim]));
    outTrim_.setTarget(dbToGain(v[OutputTrim]));
    bypassMix_.setTarget(on(Bypass) ? 1.0f : 0.0f);
    {   // every colour control at zero (or its bay off), trims at 0 dB: the voice passes untouched
        const bool dynClean = !on(DynOn) || (v[Comp] <= 0.0f && v[Color] <= 0.0f && v[DeEss] <= 0.0f && v[Gate] <= -96.0f && v[DynTrim] == 0.0f);
        const bool toneClean = !on(ToneOn) || (v[Low] == 0.0f && v[Mid] == 0.0f && v[High] == 0.0f && v[Air] == 0.0f && v[Hpf] <= 20.0f &&
                                               v[Lpf] >= 20000.0f && v[ToneTrim] == 0.0f);
        const bool spaceClean = !on(SpaceOn) || (v[ReverbMix] <= 0.0f && v[DelayMix] <= 0.0f && !on(SpaceSolo));
        const bool sfxClean = !on(SfxOn) || (v[Sfx] <= 0.0f && v[Enhancer] <= 0.0f && v[SfxTrim] == 0.0f);
        const bool clean = dynClean && toneClean && spaceClean && sfxClean && v[InputTrim] == 0.0f && v[OutputTrim] == 0.0f;
        cleanMix_.setTarget(clean ? 1.0f : 0.0f);
    }
    autoGainOn_ = on(AutoGainOn);
    const bool anySolo = on(DynSolo) || on(ToneSolo) || on(SpaceSolo) || on(SfxSolo);
    auto active = [&](std::size_t onId, std::size_t soloId) { return on(onId) && (!anySolo || on(soloId)); };
    dynOn_ = active(DynOn, DynSolo);
    toneOn_ = active(ToneOn, ToneSolo);
    spaceOn_ = active(SpaceOn, SpaceSolo);
    sfxOn_ = active(SfxOn, SfxSolo);
    spaceSoloOnly_ = on(SpaceSolo) && on(SpaceOn) && !on(DynSolo) && !on(ToneSolo) && !on(SfxSolo);
    fltPost_ = on(FltPost);
    spcPost_ = on(SpcPost);
    dynTrim_.setTarget(dbToGain(v[DynTrim]));
    toneTrim_.setTarget(dbToGain(v[ToneTrim]));
    spaceTrim_.setTarget(dbToGain(v[SpaceTrim]));
    sfxTrim_.setTarget(dbToGain(v[SfxTrim]));

    // ---- Dynamics ----
    if (diff({DynMode, Gate, Comp, Color, DeEss, Focus})) {
        dyn_ = &kDynModeTable[std::min(std::max(idx(DynMode), 0), kDynModes - 1)];
        const float amount = v[Comp] * 0.01f;
        compThrDb_ = -kCompThreshRangeDb * amount;
        compRatio_ = 1.0f + (dyn_->ratio - 1.0f) * amount;
        fetRatio_ = 1.0f + (dyn_->fetRatio - 1.0f) * amount;
        upRange_ = dyn_->upward ? dyn_->upRangeDb * amount : 0.0f;
        // Static make-up for half the reduction an -18 dBFS signal would see (auto-gain trims the rest).
        makeupDb_ = amount > 0.0f ? -0.5f * compressorGainDb(-18.0f, compThrDb_, compRatio_, dyn_->kneeDb) : 0.0f;
        const float attA = dyn_->attackMs, relA = dyn_->releaseMs;
        envA_.prepare(attA, relA, sr_); // peak mode follows |x|; RMS/opto modes follow power
        optoFast_ = onePoleCoef(kOptoFastReleaseMs, sr_);
        optoSlow_ = onePoleCoef(relA, sr_);
        envFet_.prepare(dyn_->fetAttackMs, dyn_->fetReleaseMs, sr_);
        color_ = v[Color] * 0.01f;
        satDrive_ = 1.0f + (dyn_->satDrive - 1.0f) * color_;
        satMix_ = std::min(1.0f, color_);
        gateThrDb_ = v[Gate];
        deEssAmt_ = v[DeEss] * 0.01f;
        deEssRelThrDb_ = lerp(-4.0f, -18.0f, deEssAmt_);
        deEssMaxGr_ = kDeEssMaxGrDb * deEssAmt_;
        deEssDetect_.set(designBiquad(BiquadType::BandPass, v[Focus], 1.5f, 0, sr_));
        deEssSplit_.set(designBiquad(BiquadType::HighPass, v[Focus] * kDeEssSplitRatio, 0.707f, 0, sr_));
    }

    // ---- Tone ----
    if (diff({ToneMode, Low, Mid, High, Air, VoiceMV, Hpf, Lpf})) {
        tone_ = &kToneModeTable[std::min(std::max(idx(ToneMode), 0), kToneModes - 1)];
        const float vs = idx(VoiceMV) == 0 ? kVoiceMaleScale : kVoiceHigherScale;
        auto q = [&](float base, float g) { return tone_->propQ ? base * (1.0f + std::fabs(g) / 12.0f) : base; };
        eqLow_.set(designBiquad(tone_->lowShelf ? BiquadType::LowShelf : BiquadType::Peak, tone_->lowHz * vs,
                                q(tone_->lowQ, v[Low]), v[Low], sr_));
        eqMid_.set(designBiquad(BiquadType::Peak, tone_->midHz * vs, q(tone_->midQ, v[Mid]), v[Mid], sr_));
        eqHigh_.set(designBiquad(tone_->highShelf ? BiquadType::HighShelf : BiquadType::Peak, tone_->highHz,
                                 q(tone_->highQ, v[High]), v[High], sr_));
        eqAir_.set(designBiquad(BiquadType::HighShelf, tone_->airHz, 0.7f, v[Air], sr_));
        eqLowActive_ = v[Low] != 0.0f; eqMidActive_ = v[Mid] != 0.0f;
        eqHighActive_ = v[High] != 0.0f; eqAirActive_ = v[Air] != 0.0f;
        hpf_.set(designBiquad(BiquadType::HighPass, v[Hpf], kFilterQ, 0, sr_));
        lpf_.set(designBiquad(BiquadType::LowPass, std::max(v[Lpf], v[Hpf] * 1.5f), kFilterQ, 0, sr_));
    }

    // ---- Space ----
    if (diff({SpaceMode, ReverbTime, DelaySync, Tempo, SpaceTone})) {
        const auto& m = kSpaceModeTable[std::min(std::max(idx(SpaceMode), 0), kSpaceModes - 1)];
        ReverbSettings rs;
        rs.size = m.size; rs.decaySec = v[ReverbTime] * m.decayMul; rs.dampHz = m.dampHz; rs.diffusion = m.diffusion;
        rs.modDepthMs = m.modDepthMs; rs.modRateHz = m.modRateHz; rs.earlyLevel = m.early;
        rs.predelayMs = m.predelayMs; rs.width = m.width;
        reverb_.apply(rs);
        DelaySettings ds;
        ds.timeMs = syncToMs(idx(DelaySync), v[Tempo]);
        ds.feedback = m.dlyFeedback; ds.toneHz = m.dlyToneHz; ds.lowCutHz = m.dlyLowCutHz;
        ds.pingPong = m.dlyPingPong; ds.wobbleMs = m.dlyWobbleMs; ds.drive = m.dlyDrive;
        delay_.apply(ds);
        sendHp_.set(designBiquad(BiquadType::HighPass, kSpaceSendHpfHz, 0.707f, 0, sr_));
        const float t = v[SpaceTone] * 0.01f;
        const float lp = t < 0 ? expMap(1.0f + t, kSpaceToneLpfMin, 20000.0f) : 20000.0f;
        const float hp = t > 0 ? expMap(t, 20.0f, kSpaceToneHpfMax) : 20.0f;
        retLp_.set(designBiquad(BiquadType::LowPass, lp, 0.707f, 0, sr_));
        retHp_.set(designBiquad(BiquadType::HighPass, hp, 0.707f, 0, sr_));
    }
    revMix_.setTarget(v[ReverbMix] * 0.01f);
    dlyMix_.setTarget(v[DelayMix] * 0.01f);
    vrbDuck_ = v[VrbDuck] * 0.01f;
    dlyDuck_ = v[DlyDuck] * 0.01f;

    // ---- SFX ----
    sfx_.setMode(idx(SfxMode));
    sfx_.setAmount(v[Sfx] * 0.01f);
    sfx_.setEnhancer(v[Enhancer] * 0.01f);

    cached_ = v;
}

// -----------------------------------------------------------------------------------------------------
void PhantomVox::processFilters(float& l, float& r) noexcept {
    l = lpf_.process(0, hpf_.process(0, l));
    r = lpf_.process(1, hpf_.process(1, r));
}

void PhantomVox::processDynamics(float& l, float& r) noexcept {
    // GATE (stereo-linked, hysteresis + hold). At -96 dB it only closes on digital silence.
    const float pk = std::max(std::fabs(l), std::fabs(r));
    const float gEnvDb = gainToDb(gateEnv_.process(pk));
    if (gEnvDb > gateThrDb_) { gateOpen_ = true; gateHold_ = kGateHoldMs * 0.001f * sr_; }
    else if (gEnvDb < gateThrDb_ - kGateHysteresisDb) {
        if (gateHold_ > 0) gateHold_ -= 1.0f; else gateOpen_ = false;
    }
    const float gTarget = gateOpen_ ? 1.0f : dbToGain(kGateFloorDb);
    gateGain_ = gTarget + (gTarget > gateGain_ ? gateAtt_ : gateRel_) * (gateGain_ - gTarget);
    l *= gateGain_; r *= gateGain_;

    // COMPRESSOR stage A (mode detector) + optional FET peak stage + MOBIUS upward stage.
    float levelDb;
    if (dyn_->det == assumed::pv::Detector::Peak) {
        levelDb = gainToDb(envA_.process(std::max(std::fabs(l), std::fabs(r))));
    } else {
        if (dyn_->det == assumed::pv::Detector::Opto) // release slows as reduction deepens
            envA_.rel = lerp(optoFast_, optoSlow_, clampf(lastGrA_ / 12.0f, 0.0f, 1.0f));
        levelDb = powToDb(envA_.process(0.5f * (l * l + r * r)));
    }
    float grA = compressorGainDb(levelDb, compThrDb_, compRatio_, dyn_->kneeDb);
    lastGrA_ = -grA;
    float gainDb = grA;
    if (dyn_->fet) {
        const float fl = gainToDb(envFet_.process(std::max(std::fabs(l), std::fabs(r)) * dbToGain(grA)));
        gainDb += compressorGainDb(fl, compThrDb_ + dyn_->fetOffsetDb, fetRatio_, 1.0f);
    }
    if (upRange_ > 0.0f && levelDb > -70.0f) {
        const float upThr = compThrDb_ - 10.0f;
        if (levelDb < upThr) gainDb += std::min(upRange_, (upThr - levelDb) * (1.0f - 1.0f / dyn_->upRatio));
    }
    grBlockMax_ = std::max(grBlockMax_, -std::min(gainDb, 0.0f));
    const float g = dbToGain(gainDb + makeupDb_);
    l *= g; r *= g;

    // DE-ESSER: band-vs-broadband sidechain at FOCUS, reduction applied to the band above FOCUS*0.75.
    if (deEssAmt_ > 0.0f) {
        const float bl = deEssDetect_.process(0, l), br = deEssDetect_.process(1, r);
        const float bandDb = powToDb(deEssBand_.process(0.5f * (bl * bl + br * br)));
        const float broadDb = powToDb(deEssBroad_.process(0.5f * (l * l + r * r)));
        float red = 0.0f;
        if (broadDb > kDeEssFloorDb) red = clampf((bandDb - broadDb - deEssRelThrDb_) * 0.75f, 0.0f, deEssMaxGr_);
        const float hl = deEssSplit_.process(0, l), hr = deEssSplit_.process(1, r);
        const float k = 1.0f - dbToGain(-red);
        l -= k * hl; r -= k * hr;
        dsBlockMax_ = std::max(dsBlockMax_, red);
    }

    // COLOR: mode saturation, blended by COLOR (0% = clean, >100% drives harder).
    if (satMix_ > 0.0f) {
        l = lerp(l, saturate(dyn_->sat, l, satDrive_), satMix_);
        r = lerp(r, saturate(dyn_->sat, r, satDrive_), satMix_);
    }
}

void PhantomVox::processTone(float& l, float& r, bool filtersHere) noexcept {
    if (filtersHere) processFilters(l, r);
    for (int ch = 0; ch < 2; ++ch) {
        float x = ch ? r : l;
        if (eqLowActive_)  x = eqLow_.process(ch, x);
        if (eqMidActive_)  x = eqMid_.process(ch, x);
        if (eqHighActive_) x = eqHigh_.process(ch, x);
        if (eqAirActive_)  x = eqAir_.process(ch, x);
        if (tone_->satDrive > 1.0f) x = saturate(tone_->sat, x, tone_->satDrive);
        (ch ? r : l) = x;
    }
}

void PhantomVox::processSpace(float sendL, float sendR, float duckRef, float& retL, float& retR) noexcept {
    const float sl = sendHp_.process(0, sendL), sr = sendHp_.process(1, sendR);
    const float rm = revMix_.next(), dm = dlyMix_.next();

    // Ducking from the dry vocal level.
    const float envDb = gainToDb(duckEnv_.process(duckRef));
    const float depth = clampf((envDb - kDuckFloorDb) / kDuckSpanDb, 0.0f, 1.0f);
    const float rDuckDb = -kDuckMaxDb * vrbDuck_ * depth;
    const float dDuckDb = -kDuckMaxDb * dlyDuck_ * depth;
    revDuckDb_ = std::min(revDuckDb_, rDuckDb);
    dlyDuckDb_ = std::min(dlyDuckDb_, dDuckDb);

    float rl = 0, rr = 0, dl = 0, dr = 0;
    reverb_.process(sl, sr, rl, rr);
    delay_.process(sl, sr, dl, dr);
    const float rg = rm * dbToGain(rDuckDb), dg = dm * dbToGain(dDuckDb);
    rl *= rg; rr *= rg; dl *= dg; dr *= dg;

    revInPk_ = std::max(revInPk_, std::max(std::fabs(sl), std::fabs(sr)) * (rm > 0 ? 1.0f : 0.0f));
    dlyInPk_ = std::max(dlyInPk_, std::max(std::fabs(sl), std::fabs(sr)) * (dm > 0 ? 1.0f : 0.0f));
    revOutPk_ = std::max(revOutPk_, std::max(std::fabs(rl), std::fabs(rr)));
    dlyOutPk_ = std::max(dlyOutPk_, std::max(std::fabs(dl), std::fabs(dr)));

    retL = retLp_.process(0, retHp_.process(0, rl + dl));
    retR = retLp_.process(1, retHp_.process(1, rr + dr));
}

// -----------------------------------------------------------------------------------------------------
void PhantomVox::process(float* left, float* right, int numSamples) noexcept {
    ScopedNoDenormals noDenormals;
    activity_ = {};
    int pos = 0;
    while (pos < numSamples) {
        const int n = std::min(ag::kChunk, numSamples - pos);
        updateFromParams(false);

        const bool fullyBypassed = bypassMix_.value >= 1.0f && bypassMix_.target >= 1.0f;
        float inPk[2] = {0, 0}, outPk[2] = {0, 0};
        grBlockMax_ = dsBlockMax_ = 0;
        revInPk_ = revOutPk_ = dlyInPk_ = dlyOutPk_ = 0;
        revDuckDb_ = dlyDuckDb_ = 0;
        float agDb = 0;

        for (int i = pos; i < pos + n; ++i) {
            const float dryL = left[i], dryR = right[i];
            dryDelay_[0].push(dryL);
            dryDelay_[1].push(dryR);
            const float alignedL = dryDelay_[0].read(limiter_.latency());
            const float alignedR = dryDelay_[1].read(limiter_.latency());
            const float bp = bypassMix_.next();
            if (fullyBypassed) { left[i] = alignedL; right[i] = alignedR; continue; }

            const float it = inTrim_.next();
            float l = dryL * it, r = dryR * it;
            const float refL = l, refR = r;
            inPk[0] = std::max(inPk[0], std::fabs(l));
            inPk[1] = std::max(inPk[1], std::fabs(r));

            if (toneOn_ && !fltPost_) processFilters(l, r);
            if (dynOn_) {
                processDynamics(l, r);
                const float t = dynTrim_.next(); l *= t; r *= t;
            }
            if (toneOn_) {
                processTone(l, r, fltPost_);
                const float t = toneTrim_.next(); l *= t; r *= t;
            }
            analyzer_.push(l, r);
            const float preSfxL = l, preSfxR = r;
            if (sfxOn_) {
                sfx_.process(l, r);
                const float t = sfxTrim_.next(); l *= t; r *= t;
            }
            if (spaceOn_) {
                float retL = 0, retR = 0;
                const float sendL = spcPost_ ? l : preSfxL, sendR = spcPost_ ? r : preSfxR;
                processSpace(sendL, sendR, std::max(std::fabs(preSfxL), std::fabs(preSfxR)), retL, retR);
                const float t = spaceTrim_.next();
                if (spaceSoloOnly_) { l = retL * t; r = retR * t; }
                else { l += retL * t; r += retR * t; }
            }

            if (autoGainOn_) {
                const float g = autoGain_.process(refL, refR, l, r);
                l *= g; r *= g;
                agDb = gainToDb(g);
            }
            const float ot = outTrim_.next();
            l *= ot; r *= ot;
            limiter_.process(l, r);
            const float cm = cleanMix_.next();   // colour controls all at zero: the voice passes untouched
            if (cm >= 1.0f) { l = alignedL; r = alignedR; }
            else if (cm > 0.0f) { l = lerp(l, alignedL, cm); r = lerp(r, alignedR, cm); }
            outPk[0] = std::max(outPk[0], std::fabs(l));
            outPk[1] = std::max(outPk[1], std::fabs(r));

            if (bp > 0.0f) { l = lerp(l, alignedL, bp); r = lerp(r, alignedR, bp); }
            left[i] = l;
            right[i] = r;
        }

        if (!fullyBypassed) {
            activity_.dyn |= dynOn_; activity_.tone |= toneOn_; activity_.space |= spaceOn_; activity_.sfx |= sfxOn_;
        }
        meters_.inL.push(inPk[0]); meters_.inR.push(inPk[1]);
        meters_.outL.push(outPk[0]); meters_.outR.push(outPk[1]);
        meters_.gr.set(grBlockMax_);
        meters_.ds.set(dsBlockMax_);
        meters_.gateOpen.set(gateOpen_ ? 1.0f : 0.0f);
        meters_.autoGainDb.set(agDb);
        meters_.limiterDb.set(limiter_.gainReductionDb());
        meters_.revIn.push(revInPk_); meters_.revOut.push(revOutPk_);
        meters_.dlyIn.push(dlyInPk_); meters_.dlyOut.push(dlyOutPk_);
        meters_.revDuck.set(-revDuckDb_);
        meters_.dlyDuck.set(-dlyDuckDb_);
        pos += n;
    }
}

// -----------------------------------------------------------------------------------------------------
float PhantomVox::toneCurveDb(float hz) const noexcept {
    const auto& t = kToneModeTable[std::min(std::max(params_.getInt(ToneMode), 0), kToneModes - 1)];
    const float vs = params_.getInt(VoiceMV) == 0 ? kVoiceMaleScale : kVoiceHigherScale;
    auto q = [&](float base, float g) { return t.propQ ? base * (1.0f + std::fabs(g) / 12.0f) : base; };
    const float lo = params_.get(Low), mi = params_.get(Mid), hi = params_.get(High), ai = params_.get(Air);
    float db = 0;
    db += designBiquad(t.lowShelf ? BiquadType::LowShelf : BiquadType::Peak, t.lowHz * vs, q(t.lowQ, lo), lo, sr_).magnitudeDb(hz, sr_);
    db += designBiquad(BiquadType::Peak, t.midHz * vs, q(t.midQ, mi), mi, sr_).magnitudeDb(hz, sr_);
    db += designBiquad(t.highShelf ? BiquadType::HighShelf : BiquadType::Peak, t.highHz, q(t.highQ, hi), hi, sr_).magnitudeDb(hz, sr_);
    db += designBiquad(BiquadType::HighShelf, t.airHz, 0.7f, ai, sr_).magnitudeDb(hz, sr_);
    db += designBiquad(BiquadType::HighPass, params_.get(Hpf), kFilterQ, 0, sr_).magnitudeDb(hz, sr_);
    db += designBiquad(BiquadType::LowPass, std::max(params_.get(Lpf), params_.get(Hpf) * 1.5f), kFilterQ, 0, sr_).magnitudeDb(hz, sr_);
    return db;
}

} // namespace dm::pv
