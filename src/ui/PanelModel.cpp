// HEM Dark Matter Series - faceplate model: value mapping, layout checks, shared chassis, layouts.
#include "dm/ui/PanelModel.hpp"

#include "dm/phantom_vox/PhantomVox.hpp"

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

// ---- layout checks ----------------------------------------------------------------------------------
std::vector<LayoutIssue> checkLayout(const PanelDef& def, const PanelBinding& b, std::vector<int> notOnPlate) {
    std::vector<LayoutIssue> out;
    const Box plate{0, 0, kPlateW, kPlateH};
    std::vector<bool> bound(b.paramCount(), false);
    auto mark = [&](int p) { if (p >= 0 && static_cast<std::size_t>(p) < bound.size()) bound[static_cast<std::size_t>(p)] = true; };
    mark(def.bypassParam); mark(def.inputParam); mark(def.outputParam);
    for (int p : notOnPlate) mark(p);
    for (const auto& w : def.widgets) {
        if (w.kind == Kind::Label || w.kind == Kind::Meter || w.kind == Kind::Display) {
            if (w.kind != Kind::Label && w.kind != Kind::Display) continue;
        }
        mark(w.param);
        if (w.kind == Kind::Range || w.kind == Kind::Selector) mark(w.param2);
        if (!w.box.inside(plate)) out.push_back({"outside the plate: " + w.label});
    }
    for (std::size_t i = 0; i < bound.size(); ++i)
        if (!bound[i]) out.push_back({"no control on the faceplate for '" + std::string(b.desc(static_cast<int>(i)).id) + "'"});
    // Interactive controls must not overlap each other.
    auto interactive = [](Kind k) { return k == Kind::Knob || k == Kind::Fader || k == Kind::Range || k == Kind::Button || k == Kind::Readout || k == Kind::Trim; };
    for (std::size_t i = 0; i < def.widgets.size(); ++i)
        for (std::size_t j = i + 1; j < def.widgets.size(); ++j) {
            const auto& a = def.widgets[i];
            const auto& c = def.widgets[j];
            if (interactive(a.kind) && interactive(c.kind) && a.box.overlaps(c.box))
                out.push_back({"controls overlap: '" + a.label + "' and '" + c.label + "'"});
        }
    return out;
}

// ---- shared chassis (top + bottom bars) ---------------------------------------------------------------
namespace {
void chassis(PanelDef& d, std::string name) {
    d.name = std::move(name);
    d.title = {270, 58, 1500, 150};
    d.preset = {1815, 68, 665, 104};
    d.presetPrev = {2488, 68, 104, 104};
    d.presetNext = {2600, 68, 104, 104};
    d.presetSave = {2740, 68, 116, 104};
    d.undo = {3040, 68, 116, 104};
    d.redo = {3170, 68, 116, 104};
    d.bypass = {3338, 64, 240, 112};
    d.inputTrim = {300, 2010, 200, 90};
    d.inputMeter = {640, 2034, 560, 42};
    d.footerBox = {1300, 1995, 1240, 120};
    d.outputMeter = {2770, 2034, 560, 42};
    d.outputTrim = {3350, 2010, 200, 90};
}
Widget knob(int p, float cx, float cy, float r, Accent a, std::string label, float size = 1.0f) {
    Widget w; w.kind = Kind::Knob; w.param = p; w.box = circleBox(cx, cy, r); w.accent = a; w.label = std::move(label); w.size = size;
    return w;
}
Widget readout(int p, Box b, std::string prefix, Accent a) {
    Widget w; w.kind = Kind::Readout; w.param = p; w.box = b; w.prefix = std::move(prefix); w.accent = a; w.label = w.prefix;
    return w;
}
Widget fader(int p, Box b, Accent a, std::string label, bool vertical = true) {
    Widget w; w.kind = Kind::Fader; w.param = p; w.box = b; w.accent = a; w.label = std::move(label); w.vertical = vertical;
    return w;
}
Widget button(int p, Box b, std::string label, Accent a, int choice = -1, Icon icon = Icon::None) {
    Widget w; w.kind = Kind::Button; w.param = p; w.box = b; w.label = std::move(label); w.accent = a; w.choice = choice; w.icon = icon;
    return w;
}
Widget meter(int src, Box b, std::string label, Accent a, float lo, float hi, bool fromTop = false) {
    Widget w; w.kind = Kind::Meter; w.source = src; w.box = b; w.label = std::move(label); w.accent = a; w.lo = lo; w.hi = hi; w.fromTop = fromTop;
    return w;
}
Widget bay(Box b, std::string title, Accent a) {
    Widget w; w.kind = Kind::Bay; w.box = b; w.label = std::move(title); w.accent = a;
    return w;
}
Widget display(DisplayKind k, int src, Box b, std::string label, Accent a) {
    Widget w; w.kind = Kind::Display; w.display = k; w.source = src; w.box = b; w.label = std::move(label); w.accent = a;
    return w;
}
// An upper mode bay: bay plate, power button, solo button, mode selector with icon, trim knob.
void modeBay(PanelDef& d, Box b, std::string title, Accent a, Icon icon, int onP, int soloP, int modeP, int trimP) {
    d.widgets.push_back(bay(b, title, a));
    d.widgets.push_back(button(onP, {b.x + 28, b.y + 26, 92, 92}, title + " POWER", a, -1, Icon::Power));
    d.widgets.push_back(button(soloP, {b.r() - 120, b.y + 26, 92, 92}, "S", a));
    Widget s; s.kind = Kind::Selector; s.param = modeP; s.param2 = onP; s.box = {b.x + 40, b.y + 135, b.w - 80, 290};
    s.accent = a; s.icon = icon; s.label = title + " MODE";
    d.widgets.push_back(s);
    d.widgets.push_back(knob(trimP, b.r() - 78, b.b() - 62, 46, a, "TRIM", 0.5f));
}
} // namespace

// ===================================== HEM PHANTOM VOX ================================================
// Feed ids used by the Phantom Vox binding.
PanelDef phantomVoxPanel() {
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
    modeBay(d, b4, "SFX", Accent::Gold, Icon::Tube, SfxOn, SfxSolo, SfxMode, SfxTrim);

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
    return d;
}

} // namespace dm::ui
