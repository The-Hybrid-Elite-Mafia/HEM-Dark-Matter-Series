// HEM Dark Matter Series - HEM OUTLAW RIDER faceplate (layout from Steven's Rider mockup):
// MAX boost / MIN cut faders beside a big scrolling ride display (input level, output level, the gain
// the rider applied, the TARGET line); knob row SPEED, SENSITIVITY, TARGET, RANGE, OUTPUT.
#include "dm/rider/OutlawRider.hpp"
#include "dm/ui/Feeds.hpp"
#include "dm/ui/LayoutKit.hpp"

namespace dm::ui {

PanelDef riderPanel() {
    using namespace kit;
    using namespace dm::rd;
    auto P = [](std::size_t i) { return static_cast<int>(i); };
    PanelDef d;
    chassis(d, "HEM OUTLAW RIDER");
    d.bypassParam = G_Bypass; d.inputParam = G_Input; d.outputParam = G_Output;

    d.widgets.push_back(bay({165, 280, 3510, 980}, "", Accent::Cyan));
    d.widgets.push_back(fader(P(MaxBoost), {205, 340, 120, 820}, Accent::Orange, "MAX"));
    d.widgets.back().size = 0.5f;   // ticks only: the value shows while you drag
    d.widgets.push_back(fader(P(MinCut), {355, 340, 120, 820}, Accent::Cyan, "MIN"));
    d.widgets.back().size = 0.5f;
    Widget wave = display(DisplayKind::Waveform, 0, {520, 305, 3125, 930}, "RIDE", Accent::Cyan);
    wave.param = P(Target);   // the display draws the TARGET line from this parameter (read only)
    d.widgets.push_back(wave);

    d.widgets.push_back(bay({165, 1290, 3510, 660}, "", Accent::Orange));
    d.widgets.push_back(knob(P(Speed), 560, 1555, 140, Accent::Orange, "SPEED"));
    d.widgets.push_back(knob(P(Sensitivity), 1200, 1555, 140, Accent::Cyan, "SENSITIVITY"));
    d.widgets.push_back(knob(P(Target), 1920, 1555, 190, Accent::Orange, "TARGET", 1.3f));
    d.widgets.push_back(knob(P(Range), 2640, 1555, 140, Accent::Orange, "RANGE"));
    d.widgets.push_back(knob(P(RiderOut), 3280, 1555, 140, Accent::Cyan, "OUTPUT"));
    return d;
}

} // namespace dm::ui
