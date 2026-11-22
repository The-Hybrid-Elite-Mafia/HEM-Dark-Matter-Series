// HEM Dark Matter Series - HEM SYNDICATE RACK faceplate (layout from Steven's Syndicate mockup).
// Five columns: SATURATE | DYNAMICS | TONE | MODULATE | MAXIMIZE. Upper row = mode bays (power, solo,
// mode screen) with a live display each; lower row = the column's controls.
#include "dm/syndicate/SyndicateRack.hpp"
#include "dm/ui/LayoutKit.hpp"
#include "dm/ui/SyndicateFeeds.hpp"

namespace dm::ui {

namespace {
using namespace kit;
using namespace dm::sr;
constexpr int P(std::size_t i) { return static_cast<int>(i); }

// Mode bay with the Syndicate header: power + title + solo across the top, mode screen under it.
void syndBay(PanelDef& d, Box b, std::string title, Accent a, Icon icon, std::size_t on, std::size_t modeP) {
    Widget bayW = bay(b, title, a);
    bayW.screen = {b.x + 30, b.y + 140, b.w - 60, 335};
    bayW.titleBase = 94;
    d.widgets.push_back(bayW);
    d.widgets.push_back(button(P(on), {b.x + 28, b.y + 26, 92, 92}, title + " POWER", a, -1, Icon::Power));
    d.widgets.push_back(button(P(on + 1), {b.r() - 120, b.y + 26, 92, 92}, "S", a));
    d.widgets.push_back(selector(P(modeP), P(on), {b.x + 30, b.y + 140, b.w - 60, 300}, a, icon, title + " MODE"));
}
} // namespace

PanelDef syndicatePanel() {
    PanelDef d;
    chassis(d, "HEM SYNDICATE RACK");
    d.bypassParam = G_Bypass; d.inputParam = G_Input; d.outputParam = G_Output;

    // ---- upper row: mode bays + displays --------------------------------------------------------------
    const float top = 240, upH = 945;
    const Box sat{165, top, 600, upH}, dyn{790, top, 615, upH}, tone{1430, top, 985, upH}, mod{2440, top, 610, upH}, mx{3075, top, 600, upH};
    syndBay(d, sat, "SATURATE", Accent::Cyan, Icon::TapeReel, kSatOn, kSat + SatModule::Mode);
    syndBay(d, dyn, "DYNAMICS", Accent::Pink, Icon::VuMeter, kDynOn, kComp + CompModule::Mode);
    syndBay(d, tone, "TONE", Accent::Cyan, Icon::Tube, kToneOn, kTone + ToneModule::Mode);
    syndBay(d, mod, "MODULATE", Accent::Gold, Icon::Keys, kModOn, kMod + ModModule::Mode);
    syndBay(d, mx, "MAXIMIZE", Accent::Purple, Icon::Max, kMaxOn, kMax + MaxModule::Mode);

    const float dy = 730, dh = 430;
    // SATURATE display: input level + CRISP / WARM tilt
    d.widgets.push_back(display(DisplayKind::Panel, -1, {195, dy, 540, dh}, "", Accent::Cyan));
    d.widgets.push_back(meter(SrIn, {270, 800, 50, 290}, "IN", Accent::Cyan, -48, 0));
    d.widgets.push_back(meter(SrSat, {380, 800, 50, 290}, "SAT", Accent::Orange, -60, 0));
    { Widget l; l.kind = Kind::Label; l.label = "WARM"; l.box = {520, 745, 180, 40}; l.size = 0.5f; d.widgets.push_back(l); }
    d.widgets.push_back(fader(P(kSat + SatModule::Character), {555, 780, 110, 330}, Accent::Cyan, ""));
    { Widget l; l.kind = Kind::Label; l.label = "CRISP"; l.box = {520, 1112, 180, 40}; l.size = 0.5f; d.widgets.push_back(l); }

    // DYNAMICS display: gain reduction, sidechain, live transient
    d.widgets.push_back(display(DisplayKind::Panel, -1, {820, dy, 555, dh}, "", Accent::Pink));
    d.widgets.push_back(meter(SrGr, {880, 790, 50, 290}, "GR", Accent::Cyan, -24, 0, true));
    d.widgets.push_back(readout(P(kComp + CompModule::Sidechain), {990, 800, 360, 80}, "SIDECHAIN", Accent::Orange));
    d.widgets.push_back(button(P(kComp + CompModule::ExtKey), {990, 895, 240, 66}, "EXT KEY", Accent::Pink));
    d.widgets.push_back(hmeter(SrTransient, {1000, 1000, 340, 26}, "TRANSIENT", Accent::Pink, -12, 12));

    // TONE display: analyzer + EQ curve with the six band handles
    const Box an{1460, dy, 925, dh};
    d.widgets.push_back(display(DisplayKind::Analyzer, 0, an, "ANALYZER", Accent::Cyan));
    const char* names[6] = {"SUB", "LOW", "MID", "HI MID", "HIGH", "AIR+"};
    for (int i = 0; i < 6; ++i)
        d.widgets.push_back(handle(P(kTone + ToneModule::SubF + static_cast<std::size_t>(i)), P(kTone + ToneModule::Sub + static_cast<std::size_t>(i)),
                                   i, an, Accent::Orange, names[i]));

    // MODULATE display: stereo phase / correlation
    d.widgets.push_back(display(DisplayKind::Panel, -1, {2470, dy, 550, dh}, "", Accent::Gold));
    { Widget l; l.kind = Kind::Label; l.label = "STEREO PHASE"; l.box = {2520, 830, 450, 50}; d.widgets.push_back(l); }
    d.widgets.push_back(hmeter(SrCorr, {2520, 930, 450, 30}, "", Accent::Gold, -1, 1));

    // MAXIMIZE display: band activity, band amounts, crossovers
    d.widgets.push_back(display(DisplayKind::Panel, -1, {3105, dy, 540, dh}, "", Accent::Purple));
    const char* bl[3] = {"L", "M", "H"};
    for (int i = 0; i < 3; ++i) d.widgets.push_back(meter(SrBandL + i, {3135 + 62.0f * i, 775, 40, 230}, bl[i], Accent::Purple, 0, 100));
    const char* bn[3] = {"LOW", "MID", "HIGH"};
    for (int i = 0; i < 3; ++i)
        d.widgets.push_back(fader(P(kMax + MaxModule::BandLow + static_cast<std::size_t>(i)), {3335 + 100.0f * i, 750, 90, 250}, Accent::Purple, bn[i]));
    d.widgets.push_back(readout(P(kMax + MaxModule::XoverLow), {3120, 1080, 250, 66}, "LO", Accent::Orange));
    d.widgets.push_back(readout(P(kMax + MaxModule::XoverHigh), {3385, 1080, 250, 66}, "HI", Accent::Orange));

    // ---- lower row: the column controls --------------------------------------------------------------
    const float lt = 1210, lh = 755;
    d.widgets.push_back(bay({165, lt, 600, lh}, "", Accent::Cyan));
    d.widgets.push_back(bay({790, lt, 615, lh}, "", Accent::Pink));
    d.widgets.push_back(bay({1430, lt, 985, lh}, "", Accent::Cyan));
    d.widgets.push_back(bay({2440, lt, 610, lh}, "", Accent::Gold));
    d.widgets.push_back(bay({3075, lt, 600, lh}, "", Accent::Purple));

    // SATURATE
    d.widgets.push_back(fader(P(kSat + SatModule::Amount), {330, 1250, 130, 570}, Accent::Orange, "SATURATE"));
    d.widgets.push_back(meter(SrOut, {540, 1290, 56, 490}, "OUT", Accent::Cyan, -48, 0));

    // DYNAMICS: COMP + COLOR, SHAPE + TRANSIENT
    d.widgets.push_back(knob(P(kComp + CompModule::Amount), 960, 1395, 130, Accent::Orange, "COMP"));
    d.widgets.push_back(knob(P(kComp + CompModule::Color), 1240, 1395, 48, Accent::Orange, "COLOR", 0.7f));
    d.widgets.push_back(knob(P(kTrans + TransientModule::Shape), 905, 1735, 48, Accent::Orange, "SHAPE", 0.7f));
    d.widgets.push_back(knob(P(kTrans + TransientModule::Amount), 1185, 1735, 130, Accent::Orange, "TRANSIENT"));

    // TONE: six band faders, HPF / LPF + slope, routing
    const char* fl[6] = {"SUB", "LOW", "MID", "HI MID", "HIGH", "AIR+"};
    for (int i = 0; i < 6; ++i)
        d.widgets.push_back(fader(P(kTone + ToneModule::Sub + static_cast<std::size_t>(i)), {1475 + 155.0f * i, 1240, 110, 360}, Accent::Cyan, fl[i]));
    { Widget r; r.kind = Kind::Range; r.param = P(kTone + ToneModule::Hpf); r.param2 = P(kTone + ToneModule::Lpf);
      r.box = {1560, 1712, 620, 78}; r.vertical = false; r.accent = Accent::Cyan; r.label = "HPF / LPF"; d.widgets.push_back(r); }
    d.widgets.push_back(readout(P(kTone + ToneModule::Slope), {2205, 1712, 190, 78}, "SLOPE", Accent::Cyan));
    { Widget l; l.kind = Kind::Label; l.label = "TONE ROUTING"; l.box = {1720, 1808, 400, 40}; l.size = 0.5f; d.widgets.push_back(l); }
    const char* rn[3] = {"PRE", "POST DYN", "POST"};
    const float rx[3] = {1625, 1830, 2060}, rw[3] = {180, 205, 180};
    for (int i = 0; i < 3; ++i) d.widgets.push_back(button(P(kRoute), {rx[i], 1858, rw[i], 72}, rn[i], Accent::Cyan, i));

    // MODULATE: DEPTH + MOD, DRIFT + ANALOG
    d.widgets.push_back(knob(P(kMod + ModModule::Depth), 2575, 1395, 48, Accent::Orange, "DEPTH", 0.7f));
    d.widgets.push_back(knob(P(kMod + ModModule::Amount), 2860, 1395, 130, Accent::Orange, "MOD"));
    d.widgets.push_back(knob(P(kMod + ModModule::Drift), 2640, 1735, 130, Accent::Orange, "DRIFT"));
    d.widgets.push_back(knob(P(kMod + ModModule::Analog), 2925, 1735, 48, Accent::Orange, "ANALOG", 0.7f));

    // MAXIMIZE: MULTI + MAX faders, CLIP
    d.widgets.push_back(fader(P(kMax + MaxModule::Multi), {3170, 1245, 130, 470}, Accent::Cyan, "MULTI"));
    d.widgets.push_back(fader(P(kMax + MaxModule::Max), {3440, 1245, 130, 470}, Accent::Cyan, "MAX"));
    { Widget l; l.kind = Kind::Label; l.label = "CLIP"; l.box = {3275, 1808, 200, 40}; l.size = 0.5f; d.widgets.push_back(l); }
    const char* cn[4] = {"OFF", "SOFT", "HARD", "MST"};
    for (int i = 0; i < 4; ++i) d.widgets.push_back(button(P(kMax + MaxModule::Clip), {3098 + 140.0f * i, 1858, 128, 72}, cn[i], Accent::Purple, i));
    return d;
}

} // namespace dm::ui
