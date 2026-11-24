// HEM PHANTOM VOX - SFX bay (11 modes) + ENHANCER.
#include "dm/phantom_vox/PhantomVox.hpp"

namespace dm::pv {

using namespace dm::assumed::pv;

namespace {
enum Mode { TapeSat, PitchWidener, Chorus, Phaser, Amp, Telephone, Unison, MegaMod, AnalogXL, TubeXL, TapeXL };
constexpr float kMaxModMs = 60.0f;
} // namespace

float SfxUnit::Shifter::process(float x, float ratio, float window) noexcept {
    d.push(x);
    // Delay sweeps through the window at (1 - ratio) samples per sample; two taps half a window apart
    // are crossfaded with complementary sin^2 windows so the wrap is inaudible.
    phase += (1.0f - ratio) / window;
    phase -= std::floor(phase);
    float p2 = phase + 0.5f;
    p2 -= std::floor(p2);
    const float d1 = 2.0f + phase * window;
    const float d2 = 2.0f + p2 * window;
    const float g1 = std::sin(kPi * phase);
    const float g2 = std::sin(kPi * p2);
    return d.readFrac(d1) * g1 * g1 + d.readFrac(d2) * g2 * g2;
}

void SfxUnit::prepare(float sampleRate) {
    sr_ = sampleRate;
    const int maxMod = static_cast<int>(kMaxModMs * 0.001f * sr_) + 8;
    for (auto& d : mod_) d.prepare(maxMod);
    for (auto& u : uni_) u[0].prepare(maxMod);
    for (auto& s : shift_) s.prepare(static_cast<int>(kWidenerWindowMs * 0.001f * sr_) + 8);
    for (auto& d : dc_) d.prepare(sr_);
    lfoA_.setRate(kChorusRateHz, sr_);
    lfoB_.setRate(kPhaserRateHz, sr_);
    lfoC_.setRate(kMegaTremHz, sr_);
    lfoWow_.setRate(kTapeWowHz, sr_);
    lfoFlut_.setRate(kTapeFlutterHz, sr_);
    constexpr float uniRates[4] = {0.61f, 0.87f, 1.29f, 1.73f};
    for (std::size_t i = 0; i < 4; ++i) lfoUni_[i].setRate(uniRates[i], sr_);
    reset();
    updateFilters();
}

void SfxUnit::reset() noexcept {
    // every LFO back to its start phase (unison voices keep their 0.23 offsets), so renders repeat
    for (Lfo* l : {&lfoA_, &lfoB_, &lfoC_, &lfoWow_, &lfoFlut_}) l->reset();
    for (std::size_t i = 0; i < 4; ++i) lfoUni_[i].reset(0.23f * static_cast<float>(i));
    for (auto& d : mod_) d.reset();
    for (auto& u : uni_) u[0].reset();
    for (auto& s : shift_) s.reset();
    for (auto& ch : ph_) for (auto& a : ch) a.reset();
    phFb_ = {0, 0};
    for (auto& f : hfLoss_) f.reset();
    for (StereoBiquad* b : {&bandHp_, &bandLp_, &bandHp2_, &bandLp2_, &ampMid_, &ampCab_, &ampCab2_, &lowShelf_,
                            &presence_, &enhHp_, &enhHp2_, &enhBody_})
        b->reset();
    for (auto& d : dc_) d.reset();
}

void SfxUnit::setMode(int mode) noexcept {
    mode = std::min(std::max(mode, 0), kSfxModes - 1);
    if (mode != mode_) { mode_ = mode; reset(); updateFilters(); }
}
void SfxUnit::setAmount(float a) noexcept {
    a = clampf(a, 0.0f, 1.0f);
    if (a != amt_) { amt_ = a; updateFilters(); }
}
void SfxUnit::setEnhancer(float a) noexcept {
    a = clampf(a, 0.0f, 1.0f);
    if (a != enh_) { enh_ = a; updateFilters(); }
}

void SfxUnit::updateFilters() noexcept {
    const float a = amt_;
    switch (mode_) {
    case TapeSat: case TapeXL: {
        const float loss = mode_ == TapeXL ? 7000.0f : kTapeHfLossHz;
        for (auto& f : hfLoss_) f.setCutoff(expMap(1.0f - a, loss, 20000.0f), sr_);
        lowShelf_.set(designBiquad(BiquadType::LowShelf, 80.0f, 0.7f, mode_ == TapeXL ? 2.5f * a : 1.0f * a, sr_));
        break;
    }
    case Amp:
        bandHp_.set(designBiquad(BiquadType::HighPass, kAmpLowCutHz, 0.707f, 0, sr_));
        ampMid_.set(designBiquad(BiquadType::Peak, kAmpMidHz, 0.9f, 5.0f * a, sr_));
        ampCab_.set(designBiquad(BiquadType::LowPass, kAmpCabHz, 0.9f, 0, sr_));
        ampCab2_.set(designBiquad(BiquadType::LowPass, kAmpCabHz * 1.3f, 0.6f, 0, sr_));
        break;
    case Telephone:
        bandHp_.set(designBiquad(BiquadType::HighPass, kPhoneLowHz, 0.707f, 0, sr_));
        bandHp2_.set(designBiquad(BiquadType::HighPass, kPhoneLowHz, 0.707f, 0, sr_));
        bandLp_.set(designBiquad(BiquadType::LowPass, kPhoneHighHz, 0.707f, 0, sr_));
        bandLp2_.set(designBiquad(BiquadType::LowPass, kPhoneHighHz, 0.707f, 0, sr_));
        ampMid_.set(designBiquad(BiquadType::Peak, 1800.0f, 1.2f, 6.0f, sr_));
        break;
    case AnalogXL:
        presence_.set(designBiquad(BiquadType::Peak, 3000.0f, 0.8f, 1.5f * a, sr_));
        break;
    case TubeXL:
        lowShelf_.set(designBiquad(BiquadType::LowShelf, 120.0f, 0.7f, 2.0f * a, sr_));
        presence_.set(designBiquad(BiquadType::HighShelf, 9000.0f, 0.7f, -1.5f * a, sr_));
        break;
    default: break;
    }
    enhHp_.set(designBiquad(BiquadType::HighPass, kEnhancerHpfHz, 0.707f, 0, sr_));
    enhHp2_.set(designBiquad(BiquadType::HighPass, kEnhancerHpfHz, 0.707f, 0, sr_));
    enhBody_.set(designBiquad(BiquadType::LowShelf, kEnhancerBodyHz, 0.7f, kEnhancerBodyDb * enh_, sr_));
}

float SfxUnit::tape(int ch, float x, float drive, float satMix, float wowMs, float flutterMs) noexcept {
    // The whole signal goes through the transport (wow/flutter), so no dry/delayed blend -> no comb filter.
    // Only the saturation is blended, by satMix.
    auto& d = mod_[static_cast<std::size_t>(ch)];
    d.push(x);
    const float w = (wowMs * (1.0f + lfoWow_.at(ch ? 0.1f : 0.0f)) + flutterMs * (1.0f + lfoFlut_.at(ch ? 0.3f : 0.0f)))
                    * 0.001f * sr_;
    float y = d.readFrac(2.0f + w);
    y = lerp(y, saturate(SatType::Tape, y, drive), satMix);
    y = hfLoss_[static_cast<std::size_t>(ch)].lp(y);
    y = lowShelf_.process(ch, y);
    return y;
}

void SfxUnit::process(float& l, float& r) noexcept {
    const float a = amt_;
    const float dl = l, dr = r;
    float wl = l, wr = r;
    float mix = a; // most modes crossfade dry -> effect with the macro

    switch (mode_) {
    case TapeSat: {
        const float sm = std::min(1.0f, a * 3.0f);
        wl = tape(0, l, 1.0f + 5.0f * a, sm, 0.15f * a, 0.03f * a);
        wr = tape(1, r, 1.0f + 5.0f * a, sm, 0.15f * a, 0.03f * a);
        (void)lfoWow_.next(); (void)lfoFlut_.next();
        mix = 1.0f;
        break;
    }
    case TapeXL: {
        const float sm = std::min(1.0f, a * 3.0f);
        wl = tape(0, l, 1.0f + 8.0f * a, sm, 0.45f * a, 0.08f * a);
        wr = tape(1, r, 1.0f + 8.0f * a, sm, 0.45f * a, 0.08f * a);
        (void)lfoWow_.next(); (void)lfoFlut_.next();
        mix = 1.0f;
        break;
    }
    case PitchWidener: {
        const float c = kWidenerCents * a;
        const float up = std::pow(2.0f, c / 1200.0f), dn = std::pow(2.0f, -c / 1200.0f);
        const float win = kWidenerWindowMs * 0.001f * sr_;
        const float mono = 0.5f * (l + r);
        const float sL = shift_[0].process(mono, up, win);
        const float sR = shift_[1].process(mono, dn, win);
        wl = l + sL; wr = r + sR; // add detuned copies, opposite sides
        mix = 0.5f * a;
        break;
    }
    case Chorus: case MegaMod: {
        const float depth = kChorusDepthMs * (0.3f + 0.7f * a) * 0.001f * sr_;
        const float base = kChorusBaseMs * 0.001f * sr_;
        mod_[0].push(l); mod_[1].push(r);
        const float m1 = lfoA_.at(0.0f), m2 = lfoA_.at(0.25f), m3 = lfoA_.at(0.5f), m4 = lfoA_.at(0.75f);
        (void)lfoA_.next();
        wl = 0.5f * (mod_[0].readFrac(base + depth * (1 + m1)) + mod_[1].readFrac(base * 1.3f + depth * (1 + m3)));
        wr = 0.5f * (mod_[1].readFrac(base + depth * (1 + m2)) + mod_[0].readFrac(base * 1.3f + depth * (1 + m4)));
        mix = 0.5f * a;
        if (mode_ == MegaMod) {
            // + phaser on the chorus voices + tremolo on the whole
            const float sweep = expMap(0.5f + 0.5f * lfoB_.next(), kPhaserMinHz, kPhaserMaxHz);
            float pl = wl + phFb_[0] * kPhaserFeedback * a, pr = wr + phFb_[1] * kPhaserFeedback * a;
            for (std::size_t s = 0; s < 6; ++s) {
                ph_[0][s].setFreq(sweep, sr_); ph_[1][s].setFreq(sweep * 1.1f, sr_);
                pl = ph_[0][s].process(pl); pr = ph_[1][s].process(pr);
            }
            phFb_ = {flushDenormal(pl), flushDenormal(pr)};
            const float trem = 1.0f - 0.3f * a * (0.5f + 0.5f * lfoC_.next());
            wl = (wl + pl) * trem; wr = (wr + pr) * trem;
            mix = 0.6f * a;
        }
        break;
    }
    case Phaser: {
        const float sweep = expMap(0.5f + 0.5f * lfoB_.next(), kPhaserMinHz, kPhaserMaxHz);
        float pl = l + phFb_[0] * kPhaserFeedback * a, pr = r + phFb_[1] * kPhaserFeedback * a;
        for (std::size_t s = 0; s < 6; ++s) {
            ph_[0][s].setFreq(sweep, sr_); ph_[1][s].setFreq(sweep * 1.15f, sr_);
            pl = ph_[0][s].process(pl); pr = ph_[1][s].process(pr);
        }
        phFb_ = {flushDenormal(pl), flushDenormal(pr)};
        wl = pl; wr = pr;
        mix = 0.5f * a;
        break;
    }
    case Amp: {
        const float drive = 1.0f + 19.0f * a;
        for (int ch = 0; ch < 2; ++ch) {
            float x = ch ? r : l;
            x = bandHp_.process(ch, x);
            x = saturate(SatType::Tube, x, drive);
            x = ampMid_.process(ch, x);
            x = ampCab2_.process(ch, ampCab_.process(ch, x));
            x = dc_[static_cast<std::size_t>(ch)].process(x);
            (ch ? wr : wl) = x;
        }
        break;
    }
    case Telephone: {
        for (int ch = 0; ch < 2; ++ch) {
            float x = ch ? r : l;
            x = bandHp2_.process(ch, bandHp_.process(ch, x));
            x = bandLp2_.process(ch, bandLp_.process(ch, x));
            x = ampMid_.process(ch, x);
            x = saturate(SatType::Odd, x, 1.0f + 4.0f * a);
            (ch ? wr : wl) = x;
        }
        break;
    }
    case Unison: {
        // Four voices on modulated delays; each LFO depth gives about +/- kUnisonDetuneCents of drift.
        uni_[0][0].push(l); uni_[1][0].push(r);
        const float base = 8.0f * 0.001f * sr_;
        float sumL = 0, sumR = 0;
        for (std::size_t v = 0; v < 4; ++v) {
            // peak pitch deviation (cents) ~ 1200*log2(1 + 2*pi*f*depth/sr); solve depth for the target.
            const float f = lfoUni_[v].inc * sr_;
            const float ratio = std::pow(2.0f, kUnisonDetuneCents * a / 1200.0f) - 1.0f;
            const float depth = ratio * sr_ / (kTwoPi * std::max(f, 0.05f));
            const float m = lfoUni_[v].next();
            const float d = base * (1.0f + 0.25f * static_cast<float>(v)) + depth * (1.0f + m);
            const float vl = uni_[0][0].readFrac(d), vr = uni_[1][0].readFrac(d);
            const float pan = (v & 1) ? 0.8f : 0.2f; // spread voices
            sumL += (vl * (1.0f - pan) + vr * pan * 0.3f);
            sumR += (vr * pan + vl * (1.0f - pan) * 0.3f);
        }
        wl = 0.5f * sumL; wr = 0.5f * sumR;
        mix = 0.5f * a;
        break;
    }
    case AnalogXL: {
        wl = presence_.process(0, saturate(SatType::Odd, l, 1.0f + 3.0f * a));
        wr = presence_.process(1, saturate(SatType::Odd, r, 1.0f + 3.0f * a));
        mix = std::min(1.0f, a * 2.0f);
        break;
    }
    case TubeXL: {
        wl = presence_.process(0, lowShelf_.process(0, saturate(SatType::Tube, l, 1.0f + 5.0f * a)));
        wr = presence_.process(1, lowShelf_.process(1, saturate(SatType::Tube, r, 1.0f + 5.0f * a)));
        wl = dc_[0].process(wl); wr = dc_[1].process(wr);
        mix = std::min(1.0f, a * 2.0f);
        break;
    }
    default: break;
    }

    l = lerp(dl, wl, mix);
    r = lerp(dr, wr, mix);

    // ENHANCER: high-band exciter (saturated top end added back) + gentle low-mid body shelf.
    if (enh_ > 0.0f) {
        for (int ch = 0; ch < 2; ++ch) {
            float& x = ch ? r : l;
            const float hb = enhHp2_.process(ch, enhHp_.process(ch, x));
            const float ex = saturate(SatType::Soft, hb, kEnhancerDrive) * kEnhancerDrive * 0.5f;
            x = enhBody_.process(ch, x + kEnhancerMaxMix * enh_ * ex);
        }
    }
}

} // namespace dm::pv
