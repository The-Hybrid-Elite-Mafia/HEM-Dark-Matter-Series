// HEM Dark Matter Series - HEM DARK MATTER 4D faceplate (layout from Steven's Dark Matter 4D mockup):
// REVERB bay (depth tunnel, REVERB, colours, DECAY / DEPTH / PRE-DELAY) | routing + DUCKING / SHAPE |
// DELAY bay (echo taps, DELAY, TIME, FEEDBACK, 400% MULTIPLIER).
#include "dm/darkmatter/DarkMatter4D.hpp"
#include "dm/ui/Feeds.hpp"
#include "dm/ui/LayoutKit.hpp"

namespace dm::ui {

PanelDef darkMatterPanel() {
    using namespace kit;
    using namespace dm::d4;
    auto P = [](std::size_t i) { return static_cast<int>(i); };
    PanelDef d;
    chassis(d, "HEM DARK MATTER 4D");
    d.bypassParam = G_Bypass; d.inputParam = G_Input; d.outputParam = G_Output;

    // ---- REVERB ----
    d.widgets.push_back(bay({165, 250, 1340, 1700}, "", Accent::Cyan));
    d.widgets.push_back(display(DisplayKind::Tunnel, 0, {215, 295, 1240, 690}, "ACOUSTIC DEPTH", Accent::Cyan));
    d.widgets.push_back(knob(P(Reverb), 835, 1440, 210, Accent::Cyan, "REVERB", 1.3f));
    d.widgets.push_back(knob(P(LowColor), 370, 1250, 85, Accent::Cyan, "LOW COLOR"));
    d.widgets.push_back(knob(P(HighColor), 370, 1620, 85, Accent::Cyan, "HIGH COLOR"));
    d.widgets.push_back(knob(P(Decay), 1300, 1170, 85, Accent::Cyan, "DECAY"));
    d.widgets.push_back(knob(P(Depth), 1300, 1450, 85, Accent::Cyan, "DEPTH"));
    d.widgets.push_back(knob(P(Predelay), 1300, 1730, 85, Accent::Cyan, "PRE-DELAY"));

    // ---- centre: routing, ducking ----
    d.widgets.push_back(bay({1530, 250, 780, 1700}, "", Accent::Orange));
    d.widgets.push_back(meter(DmReverb, {1570, 320, 44, 430}, "REV", Accent::Cyan, -60, 0));
    d.widgets.push_back(meter(DmDelay, {2226, 320, 44, 430}, "DLY", Accent::Orange, -60, 0));
    const char* rn[3] = {"REV > DLY", "PARALLEL", "DLY > REV"};
    const Accent ra[3] = {Accent::Orange, Accent::Gold, Accent::Cyan};
    for (int i = 0; i < 3; ++i) d.widgets.push_back(button(P(Routing), {1660, 330 + 145.0f * i, 520, 110}, rn[i], ra[i], i));
    d.widgets.push_back(meter(DmDuckL, {1570, 880, 44, 960}, "L", Accent::Cyan, -24, 0, true));
    d.widgets.push_back(meter(DmDuckR, {2226, 880, 44, 960}, "R", Accent::Cyan, -24, 0, true));
    d.widgets.push_back(knob(P(Ducking), 1920, 1060, 145, Accent::Orange, "DUCKING"));
    d.widgets.push_back(display(DisplayKind::ShapeCurve, 0, {1690, 1330, 460, 300}, "", Accent::Orange));
    d.widgets.push_back(knob(P(Shape), 1990, 1765, 62, Accent::Orange, "SHAPE", 0.7f));
    d.widgets.push_back(button(P(ExtKey), {1650, 1730, 210, 70}, "EXT KEY", Accent::Orange));

    // ---- DELAY ----
    d.widgets.push_back(bay({2335, 250, 1340, 1700}, "", Accent::Orange));
    d.widgets.push_back(display(DisplayKind::Taps, 0, {2385, 295, 1240, 690}, "ECHOES", Accent::Orange));
    d.widgets.push_back(knob(P(Delay), 3005, 1440, 210, Accent::Orange, "DELAY", 1.3f));
    d.widgets.push_back(readout(P(Time), {2405, 1300, 330, 92}, "TIME", Accent::Orange));
    d.widgets.push_back(knob(P(Feedback), 3480, 1240, 90, Accent::Orange, "FEEDBACK"));
    d.widgets.push_back(button(P(Multiplier), {3360, 1720, 270, 84}, "400% MULTI", Accent::Gold));
    fitFrame(d);
    return d;
}

} // namespace dm::ui
