// HEM Dark Matter Series - HEM APEX CORE faceplate (layout from Steven's Apex mockup):
// ENGINE screen + GR / WIDTH bay | AMOUNT dial between CONTAIN and BOOST, PARALLEL, MONO-SAFE |
// transfer (PEAKS / CURVE), envelope (ATTACK / RELEASE) and colour profile (LOW / HIGH).
#include "dm/apex/ApexCore.hpp"
#include "dm/ui/Feeds.hpp"
#include "dm/ui/LayoutKit.hpp"
#include "dm/ui/SyndicateFeeds.hpp"

namespace dm::ui {

PanelDef apexPanel() {
    using namespace kit;
    using namespace dm::ax;
    auto P = [](std::size_t i) { return static_cast<int>(i); };
    PanelDef d;
    chassis(d, "HEM APEX CORE");
    d.bypassParam = G_Bypass; d.inputParam = G_Input; d.outputParam = G_Output;

    // ---- left: ENGINE screen, GR + WIDTH ----
    Widget eng = bay({165, 300, 740, 560}, "ENGINE", Accent::Orange);
    eng.titleBase = 94;
    eng.screen = {195, 410, 680, 420};
    d.widgets.push_back(eng);
    d.widgets.push_back(selector(P(Engine), -1, {195, 430, 680, 300}, Accent::Orange, Icon::Atom, "ENGINE"));
    d.widgets.push_back(bay({165, 885, 740, 1065}, "", Accent::Cyan));
    d.widgets.push_back(display(DisplayKind::Panel, -1, {195, 915, 680, 1005}, "", Accent::Cyan));
    d.widgets.push_back(meter(SrIn, {260, 1000, 56, 560}, "IN", Accent::Cyan, -48, 0));
    d.widgets.push_back(meter(AxGr, {512, 1000, 44, 560}, "GR", Accent::Orange, -24, 0, true));
    d.widgets.push_back(meter(SrOut, {750, 1000, 56, 560}, "OUT", Accent::Cyan, -48, 0));
    d.widgets.push_back(hmeter(AxWidth, {260, 1680, 560, 22}, "MEASURED WIDTH", Accent::Cyan, 0, 200));
    d.widgets.push_back(readout(P(Width), {330, 1770, 420, 90}, "WIDTH", Accent::Orange));

    // ---- centre: AMOUNT between CONTAIN and BOOST ----
    d.widgets.push_back(bay({930, 300, 1980, 1650}, "", Accent::Orange));
    d.widgets.push_back(knob(P(Amount), 1920, 980, 330, Accent::Orange, "AMOUNT", 2.2f));
    d.widgets.push_back(knob(P(Contain), 1290, 980, 140, Accent::Orange, "CONTAIN"));
    d.widgets.push_back(knob(P(Boost), 2550, 980, 140, Accent::Orange, "BOOST"));
    d.widgets.push_back(fader(P(Parallel), {1600, 1720, 640, 80}, Accent::Orange, "PARALLEL", false));
    d.widgets.push_back(button(P(MonoSafe), {990, 1815, 280, 80}, "MONO-SAFE", Accent::Cyan));

    // ---- right: transfer, envelope, colour profile ----
    d.widgets.push_back(bay({2935, 300, 740, 1650}, "", Accent::Cyan));
    d.widgets.push_back(display(DisplayKind::Transfer, 0, {2965, 335, 680, 370}, "TRANSFER", Accent::Cyan));
    d.widgets.push_back(readout(P(Peaks), {2965, 725, 330, 80}, "PEAKS", Accent::Orange));
    d.widgets.push_back(readout(P(Curve), {3315, 725, 330, 80}, "CURVE", Accent::Orange));
    d.widgets.push_back(display(DisplayKind::Envelope, 0, {2965, 855, 680, 370}, "", Accent::Orange));
    d.widgets.push_back(readout(P(Attack), {2965, 1245, 330, 80}, "ATTACK", Accent::Orange));
    d.widgets.push_back(readout(P(Release), {3315, 1245, 330, 80}, "RELEASE", Accent::Orange));
    d.widgets.push_back(display(DisplayKind::Tilt, 0, {2965, 1375, 680, 370}, "COLOR PROFILE", Accent::Orange));
    d.widgets.push_back(readout(P(ColorLow), {2965, 1765, 330, 80}, "LOW", Accent::Orange));
    d.widgets.push_back(readout(P(ColorHigh), {3315, 1765, 330, 80}, "HIGH", Accent::Orange));
    return d;
}

} // namespace dm::ui
