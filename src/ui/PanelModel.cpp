// HEM Dark Matter Series - faceplate model: value mapping, layout checks, shared chassis, layouts.
#include "dm/ui/PanelModel.hpp"

#include "dm/phantom_vox/PhantomVox.hpp"
#include "dm/ui/LayoutKit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dm::ui {

// ---- value mapping ----------------------------------------------------------------------------------
bool isLogScale(const ParamDesc& d) noexcept {
    return !d.isChoice() && d.min > 0.0f && d.max / d.min >= 20.0f && (d.unit == "Hz" || d.unit == "s" || d.unit == "ms");
}
float toNorm(const ParamDesc& d, float v) noexcept {
    if (d.max <= d.min) return 0.0f;
    v = std::min(std::max(v, d.min), d.max);
    if (isLogScale(d)) return std::log(v / d.min) / std::log(d.max / d.min);
    return (v - d.min) / (d.max - d.min);
}
float fromNorm(const ParamDesc& d, float n) noexcept {
    n = std::min(std::max(n, 0.0f), 1.0f);
    float v = isLogScale(d) ? d.min * std::pow(d.max / d.min, n) : d.min + (d.max - d.min) * n;
    if (d.isChoice() || d.unit == "toggle") v = std::round(v);
    return v;
}
std::string formatValue(const ParamDesc& d, float v) {
    char buf[64];
    if (d.isChoice()) {
        const auto i = static_cast<std::size_t>(std::min(std::max(static_cast<int>(std::lround(v)), 0), static_cast<int>(d.choices.size()) - 1));
        return d.choices[i];
    }
    if (d.unit == "toggle") return v >= 0.5f ? "ON" : "OFF";
    if (d.unit == "Hz") {
        if (v >= 1000.0f) std::snprintf(buf, sizeof buf, "%.1f kHz", v / 1000.0f);
        else std::snprintf(buf, sizeof buf, "%.0f Hz", v);
        return buf;
    }
    if (d.unit == "dB") { std::snprintf(buf, sizeof buf, "%+.1f dB", v); return buf; }
    if (d.unit == "s") { std::snprintf(buf, sizeof buf, v < 10.0f ? "%.1fs" : "%.0fs", v); return buf; }
    if (d.unit == "ms") { std::snprintf(buf, sizeof buf, "%.0f ms", v); return buf; }
    if (d.unit == "%") { std::snprintf(buf, sizeof buf, "%.0f%%", v); return buf; }
    if (d.unit == "st") { std::snprintf(buf, sizeof buf, "%+.0f st", v); return buf; }
    if (d.unit == "cents") { std::snprintf(buf, sizeof buf, "%.0f c", v); return buf; }
    if (d.unit == "BPM") { std::snprintf(buf, sizeof buf, "%.0f BPM", v); return buf; }
    std::snprintf(buf, sizeof buf, "%.2f", v);
    return buf;
}

// ---- shared geometry -------------------------------------------------------------------------------
Track faderTrack(const Widget& w) noexcept {
    if (w.kind == Kind::Range || !w.vertical) return {w.box.x + 70, w.box.r() - 70};
    return {w.box.y + 40, w.box.b() - 40};
}
float rangeX(const Widget& w, float hz) noexcept {
    const Track t = faderTrack(w);
    return t.a + (t.b - t.a) * std::log(std::min(std::max(hz, 20.0f), 22000.0f) / 20.0f) / std::log(1100.0f);
}
float rangeHz(const Widget& w, float x) noexcept {
    const Track t = faderTrack(w);
    const float n = std::min(std::max((x - t.a) / std::max(t.b - t.a, 1.0f), 0.0f), 1.0f);
    return 20.0f * std::pow(1100.0f, n);
}
float curveX(const Box& d, float hz) noexcept {
    return d.x + d.w * std::log(std::min(std::max(hz, 20.0f), 22000.0f) / 20.0f) / std::log(1100.0f);
}
float curveY(const Box& d, float db, float rangeDb) noexcept {
    const float top = d.y + 30, bot = d.b() - 50;
    return (top + bot) * 0.5f - (bot - top) * 0.5f * std::min(std::max(db / rangeDb, -1.0f), 1.0f);
}
float curveRange(const Widget& w) noexcept { return w.hi > 0.5f ? w.hi : 12.0f; }
bool isShown(const Widget& w, const PanelBinding& b) {
    return w.showParam < 0 || static_cast<int>(std::lround(b.value(w.showParam))) == w.showValue;
}

// ---- layout checks ----------------------------------------------------------------------------------
// ---- the HEM frame ------------------------------------------------------------------------------------
namespace {
constexpr float kTopBarBottom = 222.0f;   // layouts' old top bar: anything above this moves into the frame's top strip
constexpr float kPad = 30.0f;             // breathing room round a layout's controls on the bed
}

void fitFrame(PanelDef& d) {
    Box bb{};
    bool any = false;
    std::vector<std::size_t> topKeys;
    for (std::size_t i = 0; i < d.widgets.size(); ++i) {
        const Box& b = d.widgets[i].box;
        if (b.b() <= kTopBarBottom) { topKeys.push_back(i); continue; }
        if (!any) { bb = b; any = true; continue; }
        const float x0 = std::min(bb.x, b.x), y0 = std::min(bb.y, b.y);
        bb = {x0, y0, std::max(bb.r(), b.r()) - x0, std::max(bb.b(), b.b()) - y0};
    }
    if (!any) bb = {0, 0, d.w, d.h};
    bb = {bb.x - kPad, bb.y - kPad, bb.w + 2 * kPad, bb.h + 2 * kPad};
    const Box& c = frame::content;
    FrameMap& f = d.frame;
    f.s = std::min(c.w / bb.w, c.h / bb.h);
    f.ox = c.x + (c.w - bb.w * f.s) * 0.5f - bb.x * f.s;
    f.oy = c.y + (c.h - bb.h * f.s) * 0.5f - bb.y * f.s;
    // top strip: title, preset keys, undo / redo, BYPASS, then any keys the layout had in its top bar
    d.title = f.toDesign({frame::titleX, frame::titleBase - 100, frame::titleMaxW, 120});
    d.undo = f.toDesign(frame::undo);
    d.redo = f.toDesign(frame::redo);
    d.preset = f.toDesign(frame::preset);
    d.presetPrev = f.toDesign(frame::presetPrev);
    d.presetNext = f.toDesign(frame::presetNext);
    d.presetSave = f.toDesign(frame::presetSave);
    d.bypass = f.toDesign(frame::bypass);
    for (std::size_t k = 0; k < topKeys.size(); ++k) {
        d.widgets[topKeys[k]].box = f.toDesign({frame::topKeysX + frame::topKeysStep * static_cast<float>(k), frame::undo.y, frame::topKeyW, frame::undo.h});
        d.widgets[topKeys[k]].inFrame = true;
    }
    // side panels: INPUT left, OUTPUT right
    d.inputTrim = f.toDesign(frame::inTrim);
    d.inputMeter = f.toDesign(frame::inMeter);
    d.outputTrim = f.toDesign(frame::outTrim);
    d.outputMeter = f.toDesign(frame::outMeter);
    d.footer.clear();
    d.footerBox = {};
}

Box bedInDesign(const PanelDef& d) noexcept { return d.frame.toDesign(frame::bed); }

std::vector<LayoutIssue> checkLayout(const PanelDef& def, const PanelBinding& b, std::vector<int> notOnPlate) {
    std::vector<LayoutIssue> out;
    const Box bed = bedInDesign(def);
    const Box topStrip = def.frame.toDesign({frame::bed.x, frame::undo.y - 10, frame::bed.w, frame::undo.h + 20});
    const Box emblem = def.frame.toDesign(frame::emblem);
    const Box sides[2] = {inputWell(def), outputWell(def)};
    std::vector<bool> bound(b.paramCount(), false);
    auto mark = [&](int p) { if (p >= 0 && static_cast<std::size_t>(p) < bound.size()) bound[static_cast<std::size_t>(p)] = true; };
    mark(def.bypassParam); mark(def.inputParam); mark(def.outputParam);
    for (int p : notOnPlate) mark(p);
    for (const auto& w : def.widgets) {
        if (w.kind == Kind::Label || w.kind == Kind::Meter || w.kind == Kind::Display) {
            if (w.kind != Kind::Label && w.kind != Kind::Display) continue;
        }
        mark(w.param);
        if (w.kind == Kind::Range || w.kind == Kind::Selector || w.kind == Kind::Handle || w.kind == Kind::Pad) mark(w.param2);
        if (!w.box.inside(bed) && !w.box.inside(topStrip)) out.push_back({"outside the glass bed: " + w.label});
        if (w.kind != Kind::Label && w.box.overlaps(emblem)) out.push_back({"under the emblem: " + w.label});
        for (const Box& sd : sides)
            if (w.box.overlaps(sd)) out.push_back({"on an INPUT / OUTPUT side panel: " + w.label});
    }
    for (std::size_t i = 0; i < bound.size(); ++i)
        if (!bound[i]) out.push_back({"no control on the faceplate for '" + std::string(b.desc(static_cast<int>(i)).id) + "'"});
    // Interactive controls must not overlap each other.
    auto interactive = [](Kind k) { return k == Kind::Knob || k == Kind::Fader || k == Kind::Range || k == Kind::Button || k == Kind::Readout || k == Kind::Trim || k == Kind::Pad; };
    // two widgets shown for different values of the same parameter are never on screen together
    auto exclusive = [](const Widget& a, const Widget& c) { return a.showParam >= 0 && a.showParam == c.showParam && a.showValue != c.showValue; };
    for (std::size_t i = 0; i < def.widgets.size(); ++i)
        for (std::size_t j = i + 1; j < def.widgets.size(); ++j) {
            const auto& a = def.widgets[i];
            const auto& c = def.widgets[j];
            const bool keys = (a.icon == Icon::KeyWhite || a.icon == Icon::KeyBlack) && (c.icon == Icon::KeyWhite || c.icon == Icon::KeyBlack);
            if (interactive(a.kind) && interactive(c.kind) && !keys && !exclusive(a, c) && a.box.overlaps(c.box))
                out.push_back({"controls overlap: '" + a.label + "' and '" + c.label + "'"});
        }
    return out;
}


// ===================================== HEM PHANTOM VOX ================================================
// Feed ids used by the Phantom Vox binding.
PanelDef phantomVoxPanel() {
    using namespace kit;
    using namespace dm::pv;
    PanelDef d;
    chassis(d, "HEM PHANTOM VOX");
    d.bypassParam = Bypass; d.inputParam = InputTrim; d.outputParam = OutputTrim;

    // ---- upper 4-bay mode row ----
    const float top = 238, upH = 520;
    const Box b1{165, top, 875, upH}, b2{1065, top, 850, upH}, b3{1940, top, 855, upH}, b4{2820, top, 850, upH};
    modeBay(d, b1, "DYNAMICS", Accent::Pink, Icon::Wave, DynOn, DynSolo, DynMode, DynTrim);
    modeBay(d, b2, "TONE", Accent::Cyan, Icon::Ring, ToneOn, ToneSolo, ToneMode, ToneTrim);
    modeBay(d, b3, "SPACE", Accent::Purple, Icon::Atom, SpaceOn, SpaceSolo, SpaceMode, SpaceTrim);
    modeBay(d, b4, "SFX", Accent::Gold, Icon::TubeXL, SfxOn, SfxSolo, SfxMode, SfxTrim);

    // ---- lower 4-column control deck ----
    const float lt = 785, lh = 1170;
    d.widgets.push_back(bay({165, lt, 875, lh}, "", Accent::Pink));
    d.widgets.push_back(bay({1065, lt, 850, lh}, "", Accent::Cyan));
    d.widgets.push_back(bay({1940, lt, 855, lh}, "", Accent::Purple));
    d.widgets.push_back(bay({2820, lt, 850, lh}, "", Accent::Gold));

    // Column 1: GATE fader + GR / DS meters, COMP + COLOR, DE-ESS + FOCUS
    d.widgets.push_back(fader(Gate, {200, 900, 130, 990}, Accent::Cyan, "GATE"));
    d.widgets.push_back(meter(4, {468, 900, 46, 990}, "GR", Accent::Cyan, -24, 0, true));
    d.widgets.push_back(meter(5, {560, 900, 46, 990}, "DS", Accent::Cyan, -24, 0, true));
    d.widgets.push_back(knob(Comp, 820, 1010, 150, Accent::Orange, "COMP"));
    d.widgets.push_back(readout(Color, {650, 1245, 340, 88}, "COLOR", Accent::Orange));
    d.widgets.push_back(knob(DeEss, 820, 1555, 150, Accent::Orange, "DE-ESS"));
    d.widgets.push_back(readout(Focus, {650, 1790, 340, 88}, "FOCUS", Accent::Orange));

    // Column 2: ANALYZER, LOW MID HIGH AIR+, M / V, HPF / LPF
    d.widgets.push_back(display(DisplayKind::Analyzer, 0, {1110, 830, 760, 400}, "ANALYZER", Accent::Cyan));
    const float fx[4] = {1165, 1385, 1605, 1825};
    const int fp[4] = {Low, Mid, High, Air};
    const char* fl[4] = {"LOW", "MID", "HIGH", "AIR+"};
    for (int i = 0; i < 4; ++i) d.widgets.push_back(fader(fp[i], {fx[i] - 55, 1290, 110, 380}, Accent::Cyan, fl[i]));
    d.widgets.push_back(button(VoiceMV, {1700, 1745, 190, 64}, "M / V", Accent::Cyan));
    { Widget r; r.kind = Kind::Range; r.param = Hpf; r.param2 = Lpf; r.box = {1120, 1840, 740, 78}; r.vertical = false;
      r.accent = Accent::Cyan; r.label = "HPF / LPF"; d.widgets.push_back(r); }

    // Column 3: REVERB / DELAY meter bay, REVERB + TIME, DELAY + RATE, SPACE TONE
    d.widgets.push_back(display(DisplayKind::BandMeters, -1, {1985, 830, 765, 400}, "", Accent::Purple));
    d.widgets.push_back(meter(6, {2030, 900, 40, 190}, "IN", Accent::Cyan, -48, 0));
    d.widgets.push_back(meter(7, {2110, 900, 40, 190}, "DUCK", Accent::Cyan, -24, 0, true));
    d.widgets.push_back(meter(8, {2190, 900, 40, 190}, "OUT", Accent::Cyan, -48, 0));
    d.widgets.push_back(meter(9, {2420, 900, 40, 190}, "IN", Accent::Orange, -48, 0));
    d.widgets.push_back(meter(10, {2500, 900, 40, 190}, "DUCK", Accent::Orange, -24, 0, true));
    d.widgets.push_back(meter(11, {2580, 900, 40, 190}, "OUT", Accent::Orange, -48, 0));
    d.widgets.push_back(readout(VrbDuck, {2010, 1140, 350, 72}, "VRB DUCK", Accent::Orange));
    d.widgets.push_back(readout(DlyDuck, {2380, 1140, 350, 72}, "DLY DUCK", Accent::Orange));
    d.widgets.push_back(knob(ReverbMix, 2175, 1420, 135, Accent::Orange, "REVERB"));
    d.widgets.push_back(knob(DelayMix, 2560, 1420, 135, Accent::Orange, "DELAY"));
    d.widgets.push_back(readout(ReverbTime, {2015, 1650, 320, 82}, "TIME", Accent::Orange));
    d.widgets.push_back(readout(DelaySync, {2400, 1650, 320, 82}, "RATE", Accent::Orange));
    d.widgets.push_back(fader(SpaceTone, {2010, 1840, 720, 78}, Accent::Cyan, "SPACE TONE", false));

    // Column 4: SPC POST / FLT POST, SFX, ENHANCER
    d.widgets.push_back(button(SpcPost, {2855, 820, 190, 72}, "SPC POST", Accent::Gold));
    d.widgets.push_back(button(FltPost, {3445, 820, 190, 72}, "FLT POST", Accent::Gold));
    d.widgets.push_back(knob(Sfx, 3245, 1110, 200, Accent::Orange, "SFX", 1.3f));
    d.widgets.push_back(knob(Enhancer, 3245, 1640, 190, Accent::Cyan, "ENHANCER", 1.3f));
    fitFrame(d);
    return d;
}

} // namespace dm::ui
