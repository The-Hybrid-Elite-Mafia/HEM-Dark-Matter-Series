// HEM Dark Matter Series - HEM IRON LOCK faceplate (layout from Steven's Iron Lock mockup):
// QUANTUM KEY grid | 3-octave scale-lock keyboard | GROUPS + REALTIME / HQ; lower deck: input meters,
// TUNING (TRANSITION, SUSTAIN, STRENGTH, RANGE), PITCH / FORMANT (PITCH SHIFT, FORMANT, LINK, MIX),
// output meters.
#include "dm/ironlock/IronLock.hpp"
#include "dm/ui/Feeds.hpp"
#include "dm/ui/LayoutKit.hpp"
#include "dm/ui/SyndicateFeeds.hpp"

namespace dm::ui {

PanelDef ironLockPanel() {
    using namespace kit;
    using namespace dm::il;
    auto P = [](std::size_t i) { return static_cast<int>(i); };
    PanelDef d;
    chassis(d, "HEM IRON LOCK");
    d.bypassParam = G_Bypass; d.inputParam = G_Input; d.outputParam = G_Output;

    // ---- QUANTUM KEY ----
    d.widgets.push_back(bay({165, 290, 470, 740}, "QUANTUM KEY", Accent::Orange));
    d.widgets.back().titleBase = 80;
    d.widgets.push_back(button(P(QuantumKey), {205, 400, 390, 76}, "AUTO DETECT", Accent::Orange));
    d.widgets.push_back(display(DisplayKind::NoteGrid, 0, {195, 510, 410, 490}, "", Accent::Cyan));

    // ---- keyboard: C3 .. B5, every key toggles its note in the scale ----
    d.widgets.push_back(bay({660, 290, 2780, 740}, "", Accent::Cyan));
    const Box kb{690, 320, 2720, 680};
    const int whites[7] = {0, 2, 4, 5, 7, 9, 11};
    const int blacks[5] = {1, 3, 6, 8, 10};
    const float blackPos[5] = {1, 2, 4, 5, 6};   // black key sits on the boundary before this white key
    const float ww = kb.w / 21.0f;
    for (int o = 0; o < 3; ++o)
        for (int k = 0; k < 7; ++k) {
            Widget w = button(P(NoteC + static_cast<std::size_t>(whites[k])), {kb.x + ww * static_cast<float>(o * 7 + k), kb.y, ww - 4, kb.h},
                              kNoteNames[whites[k]], Accent::Cyan, -1, Icon::KeyWhite);
            w.source = 48 + 12 * o + whites[k];
            d.widgets.push_back(w);
        }
    for (int o = 0; o < 3; ++o)
        for (int k = 0; k < 5; ++k) {
            const float x = kb.x + ww * (static_cast<float>(o * 7) + blackPos[k]) - ww * 0.32f;
            Widget w = button(P(NoteC + static_cast<std::size_t>(blacks[k])), {x, kb.y, ww * 0.64f, kb.h * 0.6f},
                              kNoteNames[blacks[k]], Accent::Cyan, -1, Icon::KeyBlack);
            w.source = 48 + 12 * o + blacks[k];
            d.widgets.push_back(w);
        }

    // ---- GROUPS, REALTIME / HQ, live correction ----
    d.widgets.push_back(bay({3465, 290, 210, 740}, "", Accent::Orange));
    d.widgets.push_back(button(P(Groups), {3490, 340, 160, 80}, "GROUPS", Accent::Orange));
    d.widgets.push_back(button(P(Quality), {3490, 500, 160, 76}, "REALTIME", Accent::Cyan, 0));
    d.widgets.push_back(button(P(Quality), {3490, 600, 160, 76}, "HQ", Accent::Orange, 1));
    d.widgets.push_back(meter(IlCorrection, {3542, 740, 56, 230}, "CORR", Accent::Cyan, 0, 100));

    // ---- lower deck ----
    d.widgets.push_back(bay({165, 1100, 230, 850}, "", Accent::Cyan));
    d.widgets.push_back(meter(SrInL, {205, 1200, 56, 640}, "L", Accent::Cyan, -48, 0));
    d.widgets.push_back(meter(SrInR, {300, 1200, 56, 640}, "R", Accent::Cyan, -48, 0));
    { Widget l; l.kind = Kind::Label; l.label = "IN dBFS"; l.box = {165, 1125, 230, 40}; l.size = 0.5f; d.widgets.push_back(l); }

    d.widgets.push_back(bay({420, 1100, 1450, 850}, "TUNING", Accent::Orange));
    d.widgets.back().titleBase = 80;
    d.widgets.push_back(knob(P(Transition), 690, 1340, 100, Accent::Orange, "TRANSITION"));
    d.widgets.push_back(knob(P(Sustain), 690, 1690, 100, Accent::Orange, "SUSTAIN"));
    d.widgets.push_back(knob(P(Strength), 1220, 1500, 190, Accent::Orange, "STRENGTH", 1.3f));
    d.widgets.push_back(fader(P(Range), {1600, 1200, 130, 600}, Accent::Orange, "RANGE"));

    d.widgets.push_back(bay({1895, 1100, 1430, 850}, "PITCH / FORMANT", Accent::Orange));
    d.widgets.back().titleBase = 80;
    d.widgets.push_back(knob(P(PitchShift), 2170, 1500, 150, Accent::Orange, "PITCH SHIFT"));
    d.widgets.push_back(knob(P(Formant), 2620, 1500, 150, Accent::Orange, "FORMANT"));
    d.widgets.push_back(button(P(Link), {2545, 1215, 150, 70}, "LINK", Accent::Gold));
    d.widgets.push_back(knob(P(Mix), 3060, 1500, 150, Accent::Orange, "MIX"));

    d.widgets.push_back(bay({3350, 1100, 325, 850}, "", Accent::Cyan));
    d.widgets.push_back(meter(SrOutL, {3420, 1200, 56, 640}, "L", Accent::Cyan, -48, 0));
    d.widgets.push_back(meter(SrOutR, {3540, 1200, 56, 640}, "R", Accent::Cyan, -48, 0));
    { Widget l; l.kind = Kind::Label; l.label = "OUT dBFS"; l.box = {3350, 1125, 325, 40}; l.size = 0.5f; d.widgets.push_back(l); }
    fitFrame(d);
    return d;
}

} // namespace dm::ui
