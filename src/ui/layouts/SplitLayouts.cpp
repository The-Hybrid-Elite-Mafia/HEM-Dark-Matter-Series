// HEM Dark Matter Series - the six Syndicate split plug-ins (HEM SAT / COMP / TRANSIENT / TONE / MOD /
// MAX) on a compact 2560 x 1600 plate, laid out from Steven's split-plug-in mockup: a control bay on the
// left, a raised mode plate (power, title, mode screen) with the live display on the right.
#include "dm/syndicate/SyndicateRack.hpp"
#include "dm/ui/LayoutKit.hpp"
#include "dm/ui/SyndicateFeeds.hpp"

namespace dm::ui {

namespace {
using namespace kit;
using namespace dm::sr;
constexpr float kW = 2560, kH = 1600;
constexpr int kOn = static_cast<int>(G_Count);          // POWER
constexpr int F(std::size_t moduleParam) { return static_cast<int>(G_Count + 1 + moduleParam); }

void base(PanelDef& d, std::string name) {
    compactChassis(d, std::move(name), kW, kH);
    d.bypassParam = G_Bypass; d.inputParam = G_Input; d.outputParam = G_Output;
}
Widget lbl(std::string text, Box b, float size = 0.5f) {
    Widget l; l.kind = Kind::Label; l.label = std::move(text); l.box = b; l.size = size;
    return l;
}
// Raised mode plate: power + title header, optional mode screen; returns the glass display area below.
Box modePlate(PanelDef& d, Box b, std::string title, Accent a, Icon icon, int modeP) {
    Widget p = bay(b, title, a);
    p.titleBase = 94;
    p.screen = modeP >= 0 ? Box{b.x + 30, b.y + 140, b.w - 60, 335} : Box{b.x + 30, b.y + 140, b.w - 60, b.h - 170};
    d.widgets.push_back(p);
    d.widgets.push_back(button(kOn, {b.x + 28, b.y + 26, 92, 92}, title + " POWER", a, -1, Icon::Power));
    if (modeP < 0) return p.screen;
    d.widgets.push_back(selector(modeP, kOn, {b.x + 30, b.y + 140, b.w - 60, 300}, a, icon, title + " MODE"));
    return {b.x + 30, b.y + 500, b.w - 60, b.h - 530};
}
constexpr Box kLeft{165, 250, 760, 1135};
constexpr Box kRight{950, 250, 1445, 1135};
} // namespace

PanelDef splitPanel(SplitKind k) {
    PanelDef d;
    switch (k) {
    case SplitKind::Sat: {
        base(d, "HEM SAT");
        d.widgets.push_back(bay(kLeft, "", Accent::Cyan));
        d.widgets.push_back(fader(F(SatModule::Amount), {480, 330, 130, 830}, Accent::Orange, "SATURATE"));
        const Box disp = modePlate(d, kRight, "SATURATE", Accent::Cyan, Icon::TapeReel, F(SatModule::Mode));
        d.widgets.push_back(display(DisplayKind::Panel, -1, disp, "", Accent::Cyan));
        d.widgets.push_back(meter(SrIn, {1060, 800, 56, 470}, "IN", Accent::Cyan, -48, 0));
        d.widgets.push_back(meter(SrSat, {1200, 800, 56, 470}, "SAT", Accent::Orange, -60, 0));
        d.widgets.push_back(meter(SrOut, {1340, 800, 56, 470}, "OUT", Accent::Cyan, -48, 0));
        d.widgets.push_back(lbl("WARM", {1830, 760, 240, 40}));
        d.widgets.push_back(fader(F(SatModule::Character), {1890, 790, 120, 480}, Accent::Cyan, ""));
        d.widgets.push_back(lbl("CRISP", {1830, 1272, 240, 40}));
        break;
    }
    case SplitKind::Comp: {
        base(d, "HEM COMP");
        d.widgets.push_back(bay(kLeft, "", Accent::Pink));
        d.widgets.push_back(knob(F(CompModule::Amount), 545, 600, 190, Accent::Cyan, "COMP", 1.3f));
        d.widgets.push_back(knob(F(CompModule::Color), 360, 1090, 70, Accent::Orange, "COLOR", 0.7f));
        d.widgets.push_back(knob(F(CompModule::Sidechain), 730, 1090, 70, Accent::Orange, "SIDECHAIN", 0.7f));
        const Box disp = modePlate(d, kRight, "DYNAMICS", Accent::Pink, Icon::VuMeter, F(CompModule::Mode));
        d.widgets.push_back(display(DisplayKind::Panel, -1, disp, "", Accent::Pink));
        d.widgets.push_back(meter(SrIn, {1060, 800, 56, 470}, "IN", Accent::Cyan, -48, 0));
        d.widgets.push_back(meter(SrGr, {1240, 800, 44, 470}, "GR", Accent::Cyan, -24, 0, true));
        d.widgets.push_back(meter(SrOut, {1400, 800, 56, 470}, "OUT", Accent::Cyan, -48, 0));
        d.widgets.push_back(meter(SrLimiter, {1560, 800, 44, 470}, "LIMIT", Accent::Orange, -12, 0, true));
        d.widgets.push_back(button(F(CompModule::ExtKey), {1900, 820, 300, 84}, "EXT KEY", Accent::Pink));
        break;
    }
    case SplitKind::Transient: {
        base(d, "HEM TRANSIENT");
        d.widgets.push_back(bay(kLeft, "", Accent::Orange));
        d.widgets.push_back(knob(F(TransientModule::Amount), 545, 600, 190, Accent::Orange, "TRANSIENT", 1.3f));
        d.widgets.push_back(knob(F(TransientModule::Shape), 545, 1090, 70, Accent::Orange, "SHAPE", 0.7f));
        const Box disp = modePlate(d, kRight, "TRANSIENT", Accent::Orange, Icon::None, -1);
        d.widgets.push_back(display(DisplayKind::Panel, -1, disp, "", Accent::Orange));
        d.widgets.push_back(lbl("LIVE TRANSIENT", {1300, 470, 750, 50}, 1.2f));
        d.widgets.push_back(hmeter(SrTransient, {1080, 640, 1180, 40}, "", Accent::Orange, -12, 12));
        d.widgets.push_back(hmeter(SrIn, {1080, 900, 1180, 26}, "INPUT", Accent::Cyan, -48, 0));
        d.widgets.push_back(hmeter(SrOut, {1080, 1080, 1180, 26}, "OUTPUT", Accent::Cyan, -48, 0));
        break;
    }
    case SplitKind::Tone: {
        base(d, "HEM TONE");
        // left: mode plate; right: analyzer with handles over the band faders
        const Box plate{165, 250, 640, 1135};
        Widget p = bay(plate, "TONE", Accent::Cyan);
        p.titleBase = 94;
        p.screen = {plate.x + 30, plate.y + 140, plate.w - 60, 335};
        d.widgets.push_back(p);
        d.widgets.push_back(button(kOn, {plate.x + 28, plate.y + 26, 92, 92}, "TONE POWER", Accent::Cyan, -1, Icon::Power));
        d.widgets.push_back(selector(F(ToneModule::Mode), kOn, {plate.x + 30, plate.y + 140, plate.w - 60, 300}, Accent::Cyan, Icon::Tube, "TONE MODE"));
        d.widgets.push_back(display(DisplayKind::Panel, -1, {plate.x + 30, plate.y + 500, plate.w - 60, 605}, "", Accent::Cyan));
        d.widgets.push_back(meter(SrIn, {300, 820, 56, 420}, "IN", Accent::Cyan, -48, 0));
        d.widgets.push_back(meter(SrOut, {440, 820, 56, 420}, "OUT", Accent::Cyan, -48, 0));
        d.widgets.push_back(readout(F(ToneModule::Slope), {560, 820, 200, 80}, "SLOPE", Accent::Cyan));
        d.widgets.push_back(bay({830, 250, 1565, 1135}, "", Accent::Cyan));
        const Box an{870, 290, 1485, 460};
        d.widgets.push_back(display(DisplayKind::Analyzer, 0, an, "ANALYZER", Accent::Cyan));
        const char* names[6] = {"SUB", "LOW", "MID", "HI MID", "HIGH", "AIR+"};
        for (int i = 0; i < 6; ++i)
            d.widgets.push_back(handle(F(ToneModule::SubF + static_cast<std::size_t>(i)), F(ToneModule::Sub + static_cast<std::size_t>(i)), i, an, Accent::Orange, names[i]));
        for (int i = 0; i < 6; ++i)
            d.widgets.push_back(fader(F(ToneModule::Sub + static_cast<std::size_t>(i)), {955 + 235.0f * i, 800, 120, 380}, Accent::Cyan, names[i]));
        Widget r; r.kind = Kind::Range; r.param = F(ToneModule::Hpf); r.param2 = F(ToneModule::Lpf);
        r.box = {1000, 1270, 1230, 78}; r.vertical = false; r.accent = Accent::Cyan; r.label = "HPF / LPF";
        d.widgets.push_back(r);
        break;
    }
    case SplitKind::Mod: {
        base(d, "HEM MOD");
        d.widgets.push_back(bay(kLeft, "", Accent::Gold));
        d.widgets.push_back(knob(F(ModModule::Amount), 360, 560, 130, Accent::Cyan, "MOD"));
        d.widgets.push_back(knob(F(ModModule::Depth), 730, 560, 100, Accent::Cyan, "DEPTH"));
        d.widgets.push_back(knob(F(ModModule::Drift), 360, 1030, 100, Accent::Orange, "DRIFT"));
        d.widgets.push_back(knob(F(ModModule::Analog), 730, 1030, 100, Accent::Orange, "ANALOG"));
        const Box disp = modePlate(d, kRight, "MODULATE", Accent::Gold, Icon::Keys, F(ModModule::Mode));
        d.widgets.push_back(display(DisplayKind::Panel, -1, disp, "", Accent::Gold));
        d.widgets.push_back(lbl("STEREO PHASE", {1350, 820, 600, 50}, 1.2f));
        d.widgets.push_back(hmeter(SrCorr, {1080, 930, 1180, 40}, "", Accent::Gold, -1, 1));
        d.widgets.push_back(hmeter(SrOut, {1080, 1150, 1180, 26}, "OUTPUT", Accent::Cyan, -48, 0));
        break;
    }
    case SplitKind::Max: {
        base(d, "HEM MAX");
        d.widgets.push_back(bay(kLeft, "", Accent::Purple));
        d.widgets.push_back(fader(F(MaxModule::Multi), {280, 320, 140, 700}, Accent::Cyan, "MULTI"));
        d.widgets.push_back(fader(F(MaxModule::Max), {610, 320, 140, 700}, Accent::Cyan, "MAX"));
        d.widgets.push_back(lbl("CLIP", {445, 1160, 200, 40}));
        const char* cn[4] = {"OFF", "SOFT", "HARD", "MST"};
        for (int i = 0; i < 4; ++i) d.widgets.push_back(button(F(MaxModule::Clip), {200 + 175.0f * i, 1220, 160, 76}, cn[i], Accent::Purple, i));
        const Box disp = modePlate(d, kRight, "MAXIMIZE", Accent::Purple, Icon::Max, F(MaxModule::Mode));
        d.widgets.push_back(display(DisplayKind::Panel, -1, disp, "", Accent::Purple));
        const char* bl[3] = {"L", "M", "H"};
        for (int i = 0; i < 3; ++i) d.widgets.push_back(meter(SrBandL + i, {1050 + 90.0f * i, 790, 56, 380}, bl[i], Accent::Purple, 0, 100));
        d.widgets.push_back(meter(SrLimiter, {1340, 790, 44, 380}, "LIMIT", Accent::Orange, -12, 0, true));
        const char* bn[3] = {"LOW", "MID", "HIGH"};
        for (int i = 0; i < 3; ++i)
            d.widgets.push_back(fader(F(MaxModule::BandLow + static_cast<std::size_t>(i)), {1500 + 150.0f * i, 770, 110, 340}, Accent::Purple, bn[i]));
        d.widgets.push_back(readout(F(MaxModule::XoverLow), {1980, 830, 330, 80}, "LO X", Accent::Orange));
        d.widgets.push_back(readout(F(MaxModule::XoverHigh), {1980, 1000, 330, 80}, "HI X", Accent::Orange));
        break;
    }
    }
    return d;
}

} // namespace dm::ui
