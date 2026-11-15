// HEM SYNDICATE RACK - module DSP.
#include "dm/syndicate/Modules.hpp"

#include "dm/core/Saturation.hpp"

namespace dm::sr {

namespace {
int pick(float v, int count) noexcept { return std::min(std::max(static_cast<int>(std::lround(v)), 0), count - 1); }
bool diff(const float* v, const float* old, std::initializer_list<std::size_t> ids) noexcept {
    if (!old) return true;
    for (auto i : ids) if (v[i] != old[i]) return true;
    return false;
}
} // namespace

// ===================================== SATURATE ========================================================
void SatModule::prepare(float sr) {
    sr_ = sr;
    for (auto& d : wow_) d.prepare(static_cast<int>(0.004f * sr_) + 16);
    wowLfo_.setRate(0.55f, sr_);
    flutLfo_.setRate(6.5f, sr_);
    reset();
}
void SatModule::reset() noexcept {
    for (StereoBiquad* b : {&pre_, &de_, &bump_, &tiltLo_, &tiltHi_}) b->reset();
    for (auto& f : hf_) f.reset();
    for (auto& d : wow_) d.reset();
    satAcc_ = sigAcc_ = 0;
}
void SatModule::update(const float* v, const float* old) noexcept {
    if (!diff(v, old, {Mode, Amount, Character})) return;
    m_ = &a::kSatModeTable[pick(v[Mode], a::kSatModes)];
    const float amt = v[Amount] * 0.01f;
    drive_ = 1.0f + (m_->driveMax - 1.0f) * amt;
    const float emph = m_->preEmphDb * std::min(1.0f, amt * 2.0f);
    pre_.set(designBiquad(BiquadType::HighShelf, 3000.0f, 0.7f, emph, sr_));
    de_.set(designBiquad(BiquadType::HighShelf, 3000.0f, 0.7f, -emph, sr_));
    bump_.set(designBiquad(BiquadType::LowShelf, m_->lowBumpHz, 0.8f, m_->lowBumpDb * amt, sr_));
    for (auto& f : hf_) f.setCutoff(expMap(1.0f - amt, m_->hfLossHz, 22000.0f), sr_);
    const float c = v[Character] * 0.01f;
    tiltLo_.set(designBiquad(BiquadType::LowShelf, a::kCharacterPivotHz, 0.5f, a::kCharacterTiltDb * c, sr_));
    tiltHi_.set(designBiquad(BiquadType::HighShelf, a::kCharacterPivotHz, 0.5f, -a::kCharacterTiltDb * c, sr_));
}
void SatModule::process(float& l, float& r) noexcept {
    const float mix = std::min(1.0f, drive_ > 1.0f ? (drive_ - 1.0f) * 2.0f : 0.0f);
    const float w = (m_->wowMs * (1.0f + wowLfo_.next()) + m_->flutterMs * (1.0f + flutLfo_.next())) * 0.001f * sr_;
    for (int ch = 0; ch < 2; ++ch) {
        float x = ch ? r : l;
        if (m_->wowMs > 0.0f) {
            auto& d = wow_[static_cast<std::size_t>(ch)];
            d.push(x);
            x = d.readFrac(2.0f + w);
        }
        const float p = pre_.process(ch, x);
        const float s = saturate(m_->sat, p, drive_);
        float y = lerp(p, s, mix);
        satAcc_ += (y - p) * (y - p);
        sigAcc_ += p * p;
        y = de_.process(ch, y);
        y = hf_[static_cast<std::size_t>(ch)].lp(y);
        y = bump_.process(ch, y);
        y = tiltHi_.process(ch, tiltLo_.process(ch, y));
        (ch ? r : l) = y;
    }
}
void SatModule::endChunk() noexcept {
    satDb_ = sigAcc_ > 1.0e-12f ? powToDb(satAcc_ / sigAcc_) : -120.0f;
    satAcc_ = sigAcc_ = 0;
}

// ===================================== COMP ============================================================
void CompModule::prepare(float sr) { sr_ = sr; reset(); }
void CompModule::reset() noexcept { env_.reset(); scHp_.reset(); lastGr_ = 0; grMax_ = 0; }
void CompModule::update(const float* v, const float* old) noexcept {
    if (!diff(v, old, {Mode, Amount, Color, Sidechain})) return;
    m_ = &a::kCompModeTable[pick(v[Mode], a::kCompModes)];
    const float amt = v[Amount] * 0.01f;
    thr_ = -a::kCompThreshRangeDb * amt;
    ratio_ = 1.0f + (m_->ratio - 1.0f) * amt;
    makeup_ = amt > 0 ? -0.5f * compressorGainDb(-18.0f, thr_, ratio_, m_->kneeDb) : 0.0f;
    env_.prepare(m_->attackMs, m_->releaseMs, sr_);
    optoFast_ = onePoleCoef(60.0f, sr_);
    optoSlow_ = onePoleCoef(m_->releaseMs, sr_);
    const float c = v[Color] * 0.01f;
    colorMix_ = c;
    colorDrive_ = 1.0f + (m_->satDrive - 1.0f) * 2.0f * c;
    scHp_.set(designBiquad(BiquadType::HighPass, expMap(v[Sidechain] * 0.01f, 20.0f, a::kSidechainMaxHz), 0.707f, 0, sr_));
}
void CompModule::process(float& l, float& r) noexcept {
    const float sl = scHp_.process(0, l), sr = scHp_.process(1, r);
    float level;
    if (m_->detector == 0) level = gainToDb(env_.process(std::max(std::fabs(sl), std::fabs(sr))));
    else {
        if (m_->detector == 2) env_.rel = lerp(optoFast_, optoSlow_, clampf(lastGr_ / 10.0f, 0.0f, 1.0f));
        level = powToDb(env_.process(0.5f * (sl * sl + sr * sr)));
    }
    const float gr = compressorGainDb(level, thr_, ratio_, m_->kneeDb);
    lastGr_ = -gr;
    grMax_ = std::max(grMax_, -gr);
    const float g = dbToGain(gr + makeup_);
    l *= g; r *= g;
    if (colorMix_ > 0.0f) {
        l = lerp(l, saturate(m_->sat, l, colorDrive_), colorMix_);
        r = lerp(r, saturate(m_->sat, r, colorDrive_), colorMix_);
    }
}

// ===================================== TRANSIENT =======================================================
void TransientModule::prepare(float sr) { sr_ = sr; gain_.prepare(1.0f, sr_); fast_.prepare(a::kTransFastMs, 60.0f, sr_); reset(); }
void TransientModule::reset() noexcept { fast_.reset(); slow_.reset(); gain_.snap(1.0f); envDb_ = 0; }
void TransientModule::update(const float* v, const float* old) noexcept {
    if (!diff(v, old, {Amount, Shape})) return;
    amount_ = v[Amount] * 0.01f;
    slow_.prepare(expMap(v[Shape] * 0.01f, a::kTransSlowMinMs, a::kTransSlowMaxMs), 60.0f, sr_);
}
void TransientModule::process(float& l, float& r) noexcept {
    const float x = std::max(std::fabs(l), std::fabs(r)) + 1.0e-9f;
    const float f = fast_.process(x), s = slow_.process(x);
    const float d = clampf(gainToDb(f) - gainToDb(s), 0.0f, 24.0f); // > 0 during attacks
    envDb_ = d;
    gain_.setTarget(dbToGain(clampf(amount_ * d * 1.5f, -a::kTransMaxDb, a::kTransMaxDb)));
    const float g = gain_.next();
    l *= g; r *= g;
}

// ===================================== TONE ============================================================
BiquadCoefs ToneModule::bandCoefs(const a::RackToneModeTune& m, int b, float gain, float shift, float sr) noexcept {
    const auto& t = m.band[b];
    const float q = m.propQ ? t.q * (1.0f + std::fabs(gain) / 12.0f) : t.q;
    const BiquadType type = t.type == 1 ? BiquadType::LowShelf : t.type == 2 ? BiquadType::HighShelf : BiquadType::Peak;
    const float hz = clampf(t.hz * std::pow(2.0f, a::kHandleOctaves * shift * 0.01f), 15.0f, 0.45f * sr);
    // AIR+ (band 5): positive boosts are split between this static shelf and the dynamic exciter.
    const float g = (b == 5 && gain > 0.0f) ? gain * (1.0f - a::kAirDynSplit) : gain;
    return designBiquad(type, hz, q, g, sr);
}
// Butterworth section Qs for 12 / 24 / 48 dB per octave; x1 (6 dB) uses a one-pole.
int ToneModule::slopeSections(int slope, float* qs) noexcept {
    switch (slope) {
    case 1: qs[0] = 0.7071f; return 1;
    case 2: qs[0] = 0.5412f; qs[1] = 1.3066f; return 2;
    case 3: qs[0] = 0.5098f; qs[1] = 0.6013f; qs[2] = 0.9000f; qs[3] = 2.5629f; return 4;
    default: return 0;
    }
}
void ToneModule::prepare(float sr) {
    sr_ = sr;
    analyzer_.prepare(sr_);
    airHp_.set(designBiquad(BiquadType::HighPass, a::kAirExciterHz, 0.707f, 0, sr_));
    airEnv_.prepare(5.0f, 120.0f, sr_);
    reset();
}
void ToneModule::reset() noexcept {
    for (auto& b : band_) b.reset();
    for (auto& f : hpf_) f.reset();
    for (auto& f : lpf_) f.reset();
    for (auto& f : hp1_) f.reset();
    for (auto& f : lp1_) f.reset();
    airHp_.reset(); airEnv_.reset();
}
void ToneModule::update(const float* v, const float* old) noexcept {
    if (!diff(v, old, {Mode, Sub, Low, Mid, HiMid, High, Air, Hpf, Lpf, Slope, SubF, LowF, MidF, HiMidF, HighF, AirF})) return;
    m_ = &a::kToneModeTable[pick(v[Mode], a::kToneModes)];
    for (int b = 0; b < 6; ++b) {
        const float g = v[Sub + static_cast<std::size_t>(b)];
        band_[static_cast<std::size_t>(b)].set(bandCoefs(*m_, b, g, v[SubF + static_cast<std::size_t>(b)], sr_));
        bandOn_[static_cast<std::size_t>(b)] = g != 0.0f;
    }
    airDyn_ = v[Air] > 0.0f ? v[Air] * a::kAirDynSplit : 0.0f;
    slope_ = pick(v[Slope], 4);
    float qs[4];
    sections_ = slopeSections(slope_, qs);
    hpfOn_ = v[Hpf] > 10.5f;
    lpfOn_ = v[Lpf] < 21900.0f;
    for (int k = 0; k < sections_; ++k) {
        hpf_[static_cast<std::size_t>(k)].set(designBiquad(BiquadType::HighPass, v[Hpf], qs[k], 0, sr_));
        lpf_[static_cast<std::size_t>(k)].set(designBiquad(BiquadType::LowPass, v[Lpf], qs[k], 0, sr_));
    }
    for (auto& f : hp1_) f.setCutoff(v[Hpf], sr_);
    for (auto& f : lp1_) f.setCutoff(v[Lpf], sr_);
}
void ToneModule::process(float& l, float& r) noexcept {
    // AIR+ exciter: quiet top end is lifted more than loud top end (smooth air, no harsh peaks).
    float airL = 0, airR = 0;
    if (airDyn_ > 0.0f) {
        const float hl = airHp_.process(0, l), hr = airHp_.process(1, r);
        const float envDb = gainToDb(airEnv_.process(std::max(std::fabs(hl), std::fabs(hr))));
        const float liftDb = airDyn_ * clampf((-20.0f - envDb) / 30.0f, 0.0f, 1.0f) + airDyn_ * 0.25f;
        const float k = dbToGain(liftDb) - 1.0f;
        airL = hl * k; airR = hr * k;
    }
    for (int ch = 0; ch < 2; ++ch) {
        float x = (ch ? r : l) + (ch ? airR : airL);
        if (hpfOn_) {
            if (sections_ == 0) x = hp1_[static_cast<std::size_t>(ch)].hp(x);
            for (int k = 0; k < sections_; ++k) x = hpf_[static_cast<std::size_t>(k)].process(ch, x);
        }
        for (std::size_t b = 0; b < 6; ++b) if (bandOn_[b]) x = band_[b].process(ch, x);
        if (lpfOn_) {
            if (sections_ == 0) x = lp1_[static_cast<std::size_t>(ch)].lp(x);
            for (int k = 0; k < sections_; ++k) x = lpf_[static_cast<std::size_t>(k)].process(ch, x);
        }
        if (m_->satDrive > 1.0f) x = saturate(m_->sat, x, m_->satDrive);
        (ch ? r : l) = x;
    }
    analyzer_.push(l, r);
}
float ToneModule::bandHz(const float* v, int b) noexcept {
    const auto& t = a::kToneModeTable[pick(v[Mode], a::kToneModes)].band[std::min(std::max(b, 0), 5)];
    return clampf(t.hz * std::pow(2.0f, a::kHandleOctaves * v[SubF + static_cast<std::size_t>(std::min(std::max(b, 0), 5))] * 0.01f), 15.0f, 21000.0f);
}
float ToneModule::curveDb(const float* v, float hz, float sr) noexcept {
    const auto& m = a::kToneModeTable[pick(v[Mode], a::kToneModes)];
    float db = 0;
    for (int b = 0; b < 6; ++b)
        db += bandCoefs(m, b, v[Sub + static_cast<std::size_t>(b)], v[SubF + static_cast<std::size_t>(b)], sr).magnitudeDb(hz, sr);
    // Display shows AIR+ at full lift (the exciter half reaches it on quiet top end).
    if (v[Air] > 0.0f) {
        const auto& t = m.band[5];
        const float hzA = clampf(t.hz * std::pow(2.0f, a::kHandleOctaves * v[AirF] * 0.01f), 15.0f, 0.45f * sr);
        db += designBiquad(BiquadType::HighShelf, hzA, t.q, v[Air], sr).magnitudeDb(hz, sr)
            - bandCoefs(m, 5, v[Air], v[AirF], sr).magnitudeDb(hz, sr);
    }
    float qs[4];
    const int sec = slopeSections(pick(v[Slope], 4), qs);
    auto onePoleDb = [&](float fc, bool high) {
        const float x = hz / fc;
        return high ? 10.0f * std::log10(x * x / (1.0f + x * x)) : 10.0f * std::log10(1.0f / (1.0f + x * x));
    };
    if (v[Hpf] > 10.5f) {
        if (sec == 0) db += onePoleDb(v[Hpf], true);
        for (int k = 0; k < sec; ++k) db += designBiquad(BiquadType::HighPass, v[Hpf], qs[k], 0, sr).magnitudeDb(hz, sr);
    }
    if (v[Lpf] < 21900.0f) {
        if (sec == 0) db += onePoleDb(v[Lpf], false);
        for (int k = 0; k < sec; ++k) db += designBiquad(BiquadType::LowPass, v[Lpf], qs[k], 0, sr).magnitudeDb(hz, sr);
    }
    return db;
}

// ===================================== MODULATE ========================================================
void ModModule::prepare(float sr) {
    sr_ = sr;
    for (auto& d : d_) d.prepare(static_cast<int>(0.06f * sr_) + 16);
    for (auto& d : wow_) d.prepare(static_cast<int>(0.004f * sr_) + 16);
    for (auto& s : dbl_) s.prepare(static_cast<int>(0.04f * sr_));
    driftLfo_.setRate(a::kDriftRateHz, sr_);
    wowLfo_.setRate(a::kAnalogWowHz, sr_);
    flutLfo_.setRate(a::kAnalogFlutterHz, sr_);
    sideHp_.set(designBiquad(BiquadType::HighPass, 250.0f, 0.707f, 0, sr_));
    reset();
}
void ModModule::reset() noexcept {
    for (auto& d : d_) d.reset();
    for (auto& d : wow_) d.reset();
    for (auto& s : dbl_) s.reset();
    for (auto& c : ph_) for (auto& ap : c) ap.reset();
    fb_ = {0, 0};
    for (std::size_t i = 0; i < lfo_.size(); ++i) lfo_[i].reset(0.25f * static_cast<float>(i));
    sideHp_.reset();
    xl_ = xr_ = xlr_ = 0;
}
void ModModule::update(const float* v, const float* old) noexcept {
    if (!diff(v, old, {Mode, Amount, Depth, Drift, Analog})) return;
    const auto* prev = m_;
    m_ = &a::kModModeTable[pick(v[Mode], a::kModModes)];
    if (prev != m_ && old) reset();
    mix_ = v[Amount] * 0.01f;
    depth_ = v[Depth] * 0.01f;
    drift_ = v[Drift] * 0.01f;
    analog_ = v[Analog] * 0.01f;
    constexpr float rateMul[4] = {1.0f, 1.37f, 0.71f, 1.83f};
    for (std::size_t i = 0; i < lfo_.size(); ++i) lfo_[i].setRate(m_->rateHz * rateMul[i], sr_);
}
void ModModule::process(float& l, float& r) noexcept {
    // Analog transport wow/flutter on the whole signal.
    if (analog_ > 0.0f) {
        const float w = analog_ * a::kAnalogWowMs * (1.0f + 0.8f * wowLfo_.next() + 0.2f * flutLfo_.next()) * 0.001f * sr_;
        wow_[0].push(l); wow_[1].push(r);
        l = wow_[0].readFrac(2.0f + w);
        r = wow_[1].readFrac(2.0f + w);
    }
    // Slow random-feeling drift (two incommensurate sines) expressed as extra delay modulation in samples.
    const float dLfo = 0.6f * driftLfo_.next() + 0.4f * driftLfo_.at(0.37f) * std::sin(0.31f * driftLfo_.phase * kTwoPi);
    const float driftSamples = drift_ * (std::pow(2.0f, a::kDriftMaxCents / 1200.0f) - 1.0f) * sr_ / (kTwoPi * a::kDriftRateHz) * dLfo;
    const float dl = l, dr = r;
    float wl = l, wr = r;
    const float ms = 0.001f * sr_;
    switch (m_->algo) {
    case 0: { // chorus (2 or 4 voices)
        d_[0].push(l); d_[1].push(r);
        float al = 0, ar = 0;
        for (int vI = 0; vI < m_->voices; ++vI) {
            const auto k = static_cast<std::size_t>(vI);
            const float mod = lfo_[k].next();
            const float dd = (m_->baseMs * ms) * (1.0f + 0.15f * static_cast<float>(vI)) + m_->depthMs * ms * depth_ * (1.0f + mod) + std::fabs(driftSamples);
            if (vI & 1) ar += d_[1].readFrac(dd); else al += d_[0].readFrac(dd);
        }
        const float n = static_cast<float>(std::max(1, m_->voices / 2));
        wl = al / n; wr = ar / n;
        break;
    }
    case 1: { // stereo enhance: high-passed, slightly delayed mid injected into the side channel
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r);
        d_[0].push(mid);
        const float dd = (m_->baseMs + m_->depthMs * (1.0f + 0.3f * lfo_[0].next())) * ms + std::fabs(driftSamples);
        const float inj = sideHp_.process(0, d_[0].readFrac(std::max(1.0f, dd))) * (0.3f + 0.7f * depth_);
        wl = mid + side + inj; wr = mid - side - inj;
        break;
    }
    case 2: { // doubler: opposite micro-detuned copies
        const float cents = (4.0f + 10.0f * depth_);
        const float up = std::pow(2.0f, cents / 1200.0f), dn = 1.0f / up;
        d_[0].push(l); d_[1].push(r);
        const float dd = m_->baseMs * ms + std::fabs(driftSamples);
        wl = dbl_[0].process(d_[0].readFrac(dd), up, 0.03f * sr_);
        wr = dbl_[1].process(d_[1].readFrac(dd * 1.11f), dn, 0.03f * sr_);
        break;
    }
    case 3: { // phaser, 4 stages per side, quadrature sweep
        const float s0 = 0.5f + 0.5f * lfo_[0].next();
        const float fl = expMap(clampf(s0 + 0.05f * dLfo * drift_, 0.0f, 1.0f), 200.0f, 200.0f + 3800.0f * (0.3f + 0.7f * depth_));
        const float fr = expMap(clampf(1.0f - s0, 0.0f, 1.0f), 200.0f, 200.0f + 3800.0f * (0.3f + 0.7f * depth_));
        float pl = l + fb_[0] * m_->feedback * depth_, pr = r + fb_[1] * m_->feedback * depth_;
        for (std::size_t s = 0; s < 4; ++s) {
            ph_[0][s].setFreq(fl, sr_); ph_[1][s].setFreq(fr, sr_);
            pl = ph_[0][s].process(pl); pr = ph_[1][s].process(pr);
        }
        fb_ = {flushDenormal(pl), flushDenormal(pr)};
        wl = pl; wr = pr;
        break;
    }
    default: { // retro flanger with feedback
        const float mod = lfo_[0].next();
        const float dd = m_->baseMs * ms + m_->depthMs * ms * depth_ * (1.0f + mod) + std::fabs(driftSamples);
        const float yl = d_[0].readFrac(std::max(1.0f, dd)), yr = d_[1].readFrac(std::max(1.0f, dd * 1.05f));
        d_[0].push(flushDenormal(l + yl * m_->feedback * depth_));
        d_[1].push(flushDenormal(r + yr * m_->feedback * depth_));
        wl = std::tanh(yl); wr = std::tanh(yr);
        break;
    }
    }
    const float m = (m_->algo == 1) ? mix_ : 0.5f * mix_;
    l = lerp(dl, wl, m);
    r = lerp(dr, wr, m);
    xl_ += double(l) * l; xr_ += double(r) * r; xlr_ += double(l) * r;
}
void ModModule::endChunk() noexcept {
    const double den = std::sqrt(xl_ * xr_);
    corr_ = den > 1e-12 ? static_cast<float>(xlr_ / den) : 1.0f;
    xl_ = xr_ = xlr_ = 0;
}

// ===================================== MAXIMIZE ========================================================
void MaxModule::prepare(float sr) { sr_ = sr; reset(); }
void MaxModule::reset() noexcept {
    split_.reset();
    limG_ = 1; limGr_ = 0;
    for (auto& e : env_) e.reset();
    actAcc_ = {0, 0, 0};
}
void MaxModule::update(const float* v, const float* old) noexcept {
    if (!diff(v, old, {Mode, Multi, Max, XoverLow, XoverHigh, Clip, BandLow, BandMid, BandHigh})) return;
    m_ = &a::kMaxModeTable[pick(v[Mode], a::kMaxModes)];
    multi_ = v[Multi] * 0.01f;
    downThr_ = -6.0f - 24.0f * multi_;
    upThr_ = -50.0f + 20.0f * multi_;
    downRatio_ = 1.0f + (m_->downRatio - 1.0f) * multi_;
    upRatio_ = 1.0f + (m_->upRatio - 1.0f) * multi_;
    upRange_ = m_->upRangeDb * multi_;
    push_ = dbToGain(a::kMaxPushDb * v[Max] * 0.01f);
    bandAmt_ = {v[BandLow] * 0.01f, v[BandMid] * 0.01f, v[BandHigh] * 0.01f};
    limThr_ = dbToGain(a::kMaxLimitDb);
    limRel_ = onePoleCoef(a::kMaxLimitRelMs, sr_);
    clip_ = pick(v[Clip], 4);
    split_.set(v[XoverLow], v[XoverHigh], sr_);
    for (auto& e : env_) e.prepare(m_->attackMs, m_->releaseMs, sr_);
    const float gains[3] = {m_->lowGainDb, m_->midGainDb, m_->highGainDb};
    for (std::size_t b = 0; b < 3; ++b) bandGain_[b] = gains[b] * multi_;
}
void MaxModule::process(float& l, float& r) noexcept {
    float band[2][3];
    split_.split(0, l, band[0][0], band[0][1], band[0][2]);
    split_.split(1, r, band[1][0], band[1][1], band[1][2]);
    float outL = 0, outR = 0;
    for (std::size_t b = 0; b < 3; ++b) {
        const float bl = band[0][b], br = band[1][b];
        const float level = powToDb(env_[b].process(0.5f * (bl * bl + br * br)));
        float g = bandGain_[b];
        if (multi_ > 0.0f) {
            if (level > downThr_) g -= (level - downThr_) * (1.0f - 1.0f / downRatio_);
            else if (level < upThr_ && level > -70.0f) g += std::min(upRange_, (upThr_ - level) * (1.0f - 1.0f / upRatio_));
        }
        g *= bandAmt_[b]; // AMOUNT PER BAND (LOW / MID / HIGH)
        actAcc_[b] = std::max(actAcc_[b], std::fabs(g));
        const float lin = lerp(1.0f, dbToGain(g), m_->parallel);
        outL += bl * lin; outR += br * lin;
    }
    if (multi_ > 0.0f && m_->satDrive > 1.0f) {
        const float d = 1.0f + (m_->satDrive - 1.0f) * multi_;
        outL = saturate(m_->sat, outL, d); outR = saturate(m_->sat, outR, d);
    }
    outL *= push_; outR *= push_;
    auto clipOne = [&](float x) {
        switch (clip_) {
        case 1: { // SOFT: hard knee into tanh above -1 dBFS
            const float k = dbToGain(a::kSoftClipKneeDb), ax = std::fabs(x);
            if (ax <= k) return x;
            return std::copysign(k + (1.0f - k) * std::tanh((ax - k) / (1.0f - k)), x);
        }
        case 2: return clampf(x, -1.0f, 1.0f); // HARD
        case 3: { // MST: wide smooth cubic knee from -6 dBFS
            const float k = 0.5f, ax = std::fabs(x);
            if (ax <= k) return x;
            const float t = std::min((ax - k) / (1.5f - k), 1.0f);
            return std::copysign(k + (1.0f - k) * (1.5f * t - 0.5f * t * t * t), x);
        }
        default: return x; // OFF
        }
    };
    l = clipOne(outL);
    r = clipOne(outR);
    // MAX: wideband zero-latency peak limiter after the clipper (instant attack, smooth release).
    if (push_ > 1.0f) {
        const float pk = std::max(std::fabs(l), std::fabs(r));
        const float need = pk > limThr_ ? limThr_ / pk : 1.0f;
        limG_ = need < limG_ ? need : need + limRel_ * (limG_ - need);
        l *= limG_; r *= limG_;
        limGrMax_ = std::max(limGrMax_, -gainToDb(limG_));
    }
}
void MaxModule::endChunk() noexcept {
    limGr_ = limGrMax_;
    limGrMax_ = 0;
    for (std::size_t b = 0; b < 3; ++b) {
        act_[b] = clampf(actAcc_[b] / 12.0f * 100.0f, 0.0f, 100.0f);
        actAcc_[b] = 0;
    }
}

} // namespace dm::sr
