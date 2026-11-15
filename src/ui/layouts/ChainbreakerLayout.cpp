// HEM Dark Matter Series - HEM CHAINBREAKER 108 faceplate (layout from Steven's Chainbreaker mockup):
// quick-load chains 1-8 in the top bar; four chain slots, each showing the controls of whichever
// module is loaded in it (STUTTER / MULTI-CLIP / GRAIN ENGINE / SPECTRO EQ); master deck: BROADCAST EQ
// + 5-band mini EQ, FLUX / DRIVE / TONE / MIX, MONITORING (CROSSFEED / WIDTH / OUT LEVEL, routing).
#include "dm/chainbreaker/Chainbreaker108.hpp"
#include "dm/ui/Feeds.hpp"
#include "dm/ui/LayoutKit.hpp"

namespace dm::ui {

namespace {
using namespace kit;
using namespace dm::cb;
constexpr int P(std::size_t i) { return static_cast<int>(i); }

void cond(PanelDef& d, Widget w, int slotParam, int type) {
    w.showParam = slotParam; w.showValue = type;
    d.widgets.push_back(std::move(w));
}

void slot(PanelDef& d, int i, float bx, float by, float bw) {
    const int typeP = P(S1Type + static_cast<std::size_t>(i));
    const Accent acc[4] = {Accent::Cyan, Accent::Orange, Accent::Cyan, Accent::Purple};
    d.widgets.push_back(bay({bx, by, bw, 990}, "", Accent::Orange));
    Widget hdr = selector(typeP, -1, {bx + 20, by + 24, bw - 160, 96}, Accent::Orange, Icon::None, "SLOT");
    hdr.size = 0.5f;
    hdr.prefix = "SLOT " + std::to_string(i + 1);
    d.widgets.push_back(hdr);
    d.widgets.push_back(button(P(S1On + static_cast<std::size_t>(i)), {bx + bw - 120, by + 26, 92, 92}, "SLOT POWER", Accent::Orange, -1, Icon::Power));
    const float cx = bx + bw * 0.5f;
    // 0 STUTTER
    Widget pad; pad.kind = Kind::Pad; pad.param = P(StutX); pad.param2 = P(StutY); pad.source = CbStutHz;
    pad.box = {bx + 40, by + 160, bw - 80, 590}; pad.accent = acc[0]; pad.label = "STUTTER";
    cond(d, pad, typeP, 0);
    cond(d, knob(P(Flux), cx, by + 845, 66, Accent::Cyan, "FLUX", 0.7f), typeP, 0);
    // 1 MULTI-CLIP
    const char* bl[3] = {"L", "M", "H"};
    for (int k = 0; k < 3; ++k) cond(d, meter(CbClipL + k, {cx - 250 + 220.0f * k, by + 180, 60, 360}, bl[k], Accent::Orange, 0, 100), typeP, 1);
    Widget xo; xo.kind = Kind::Range; xo.param = P(ClipLow); xo.param2 = P(ClipHigh); xo.box = {bx + 40, by + 610, bw - 80, 78};
    xo.vertical = false; xo.accent = Accent::Orange; xo.label = "CROSSOVER";
    cond(d, xo, typeP, 1);
    cond(d, knob(P(Drive), bx + bw * 0.28f, by + 840, 75, Accent::Orange, "DRIVE"), typeP, 1);
    cond(d, knob(P(ClipShape), bx + bw * 0.72f, by + 840, 75, Accent::Orange, "SHAPE"), typeP, 1);
    // 2 GRAIN ENGINE
    cond(d, display(DisplayKind::Grain, 0, {bx + 40, by + 160, bw - 80, 400}, "GRAIN", Accent::Cyan), typeP, 2);
    cond(d, button(P(GrainReverse), {cx - 130, by + 590, 260, 76}, "REVERSE", Accent::Cyan), typeP, 2);
    cond(d, knob(P(GrainPitch), bx + bw * 0.2f, by + 830, 75, Accent::Cyan, "PITCH"), typeP, 2);
    cond(d, knob(P(Wide), cx, by + 830, 75, Accent::Cyan, "WIDE"), typeP, 2);
    cond(d, knob(P(GrainMix), bx + bw * 0.8f, by + 830, 75, Accent::Cyan, "MIX"), typeP, 2);
    // 3 SPECTRO EQ
    Widget an = display(DisplayKind::Analyzer, 0, {bx + 40, by + 160, bw - 80, 440}, "SPECTRO EQ", Accent::Purple);
    an.hi = 18;
    cond(d, an, typeP, 3);
    for (int k = 0; k < 4; ++k) {
        Widget h = handle(P(Eq1F + 3 * static_cast<std::size_t>(k)), P(Eq1G + 3 * static_cast<std::size_t>(k)), k, an.box, Accent::Orange, "EQ " + std::to_string(k + 1));
        h.hi = 18;
        cond(d, h, typeP, 3);
        cond(d, knob(P(Eq1Q + 3 * static_cast<std::size_t>(k)), bx + bw * (0.14f + 0.24f * static_cast<float>(k)), by + 790, 55, Accent::Purple,
                     "Q" + std::to_string(k + 1), 0.7f), typeP, 3);
    }
}
} // namespace

PanelDef chainbreakerPanel() {
    PanelDef d;
    chassis(d, "HEM CHAINBREAKER 108");
    d.twoLineTitle = true;
    d.undo = {1050, 68, 116, 104};
    d.redo = {1180, 68, 116, 104};
    d.preset = {1340, 68, 620, 104};
    d.presetPrev = {1968, 68, 104, 104};
    d.presetNext = {2080, 68, 104, 104};
    d.presetSave = {2210, 68, 116, 104};
    d.bypassParam = G_Bypass; d.inputParam = G_Input; d.outputParam = G_Output;
    for (int i = 0; i < 8; ++i) {   // QUICK-LOAD chain slots
        Widget b = button(-1, {2370 + 112.0f * static_cast<float>(i), 68, 100, 104}, std::to_string(i + 1), Accent::Orange);
        b.source = i;
        d.widgets.push_back(b);
    }

    // ---- four chain slots ----
    for (int i = 0; i < 4; ++i) slot(d, i, 165 + 885.0f * static_cast<float>(i), 245, 855);

    // ---- master deck ----
    d.widgets.push_back(bay({165, 1265, 950, 685}, "BROADCAST EQ", Accent::Orange));
    d.widgets.back().titleBase = 72;
    d.widgets.push_back(knob(P(BcLow), 330, 1440, 75, Accent::Orange, "LOW"));
    d.widgets.push_back(knob(P(BcMid), 640, 1440, 75, Accent::Orange, "MID"));
    d.widgets.push_back(knob(P(BcHigh), 950, 1440, 75, Accent::Orange, "HIGH"));
    const char* gn[5] = {"SUB", "LOW", "MID", "HI MID", "HIGH"};
    for (int k = 0; k < 5; ++k)
        d.widgets.push_back(fader(P(GSub + static_cast<std::size_t>(k)), {230 + 170.0f * static_cast<float>(k), 1615, 100, 265}, Accent::Cyan, gn[k]));

    d.widgets.push_back(bay({1140, 1265, 1560, 685}, "MASTER", Accent::Orange));
    d.widgets.back().titleBase = 72;
    d.widgets.push_back(knob(P(MFlux), 1345, 1600, 150, Accent::Orange, "FLUX"));
    d.widgets.push_back(knob(P(MDrive), 1725, 1600, 150, Accent::Orange, "DRIVE"));
    d.widgets.push_back(knob(P(MTone), 2105, 1600, 150, Accent::Orange, "TONE"));
    d.widgets.push_back(knob(P(MMix), 2490, 1600, 150, Accent::Orange, "MIX"));

    d.widgets.push_back(bay({2725, 1265, 950, 685}, "MONITORING", Accent::Cyan));
    d.widgets.back().titleBase = 72;
    d.widgets.push_back(knob(P(Crossfeed), 2890, 1440, 75, Accent::Cyan, "CROSSFEED"));
    d.widgets.push_back(knob(P(Width), 3200, 1440, 75, Accent::Cyan, "WIDTH"));
    d.widgets.push_back(knob(P(OutLevel), 3510, 1440, 75, Accent::Cyan, "OUT LEVEL"));
    { Widget l; l.kind = Kind::Label; l.label = "UTILITY ROUTING"; l.box = {3000, 1660, 400, 40}; l.size = 0.5f; d.widgets.push_back(l); }
    const char* un[4] = {"PRE", "SOFT", "HARD", "MST"};
    for (int k = 0; k < 4; ++k) d.widgets.push_back(button(P(UtilRoute), {2765 + 225.0f * static_cast<float>(k), 1720, 200, 80}, un[k], Accent::Cyan, k));
    return d;
}

} // namespace dm::ui
