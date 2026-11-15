#pragma once
// HEM Dark Matter Series - shared building blocks for the hand-placed faceplate layouts.
#include "dm/ui/PanelModel.hpp"

#include <string>
#include <utility>

namespace dm::ui::kit {

// ---- shared chassis (top + bottom bars) ---------------------------------------------------------------
inline void chassis(PanelDef& d, std::string name) {
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
inline Widget knob(int p, float cx, float cy, float r, Accent a, std::string label, float size = 1.0f) {
    Widget w; w.kind = Kind::Knob; w.param = p; w.box = circleBox(cx, cy, r); w.accent = a; w.label = std::move(label); w.size = size;
    return w;
}
inline Widget readout(int p, Box b, std::string prefix, Accent a) {
    Widget w; w.kind = Kind::Readout; w.param = p; w.box = b; w.prefix = std::move(prefix); w.accent = a; w.label = w.prefix;
    return w;
}
inline Widget fader(int p, Box b, Accent a, std::string label, bool vertical = true) {
    Widget w; w.kind = Kind::Fader; w.param = p; w.box = b; w.accent = a; w.label = std::move(label); w.vertical = vertical;
    return w;
}
inline Widget button(int p, Box b, std::string label, Accent a, int choice = -1, Icon icon = Icon::None) {
    Widget w; w.kind = Kind::Button; w.param = p; w.box = b; w.label = std::move(label); w.accent = a; w.choice = choice; w.icon = icon;
    return w;
}
inline Widget meter(int src, Box b, std::string label, Accent a, float lo, float hi, bool fromTop = false) {
    Widget w; w.kind = Kind::Meter; w.source = src; w.box = b; w.label = std::move(label); w.accent = a; w.lo = lo; w.hi = hi; w.fromTop = fromTop;
    return w;
}
inline Widget bay(Box b, std::string title, Accent a) {
    Widget w; w.kind = Kind::Bay; w.box = b; w.label = std::move(title); w.accent = a;
    return w;
}
inline Widget display(DisplayKind k, int src, Box b, std::string label, Accent a) {
    Widget w; w.kind = Kind::Display; w.display = k; w.source = src; w.box = b; w.label = std::move(label); w.accent = a;
    return w;
}
// An upper mode bay: bay plate, power button, solo button, mode selector with icon, trim knob.
inline void modeBay(PanelDef& d, Box b, std::string title, Accent a, Icon icon, int onP, int soloP, int modeP, int trimP) {
    d.widgets.push_back(bay(b, title, a));
    d.widgets.push_back(button(onP, {b.x + 28, b.y + 26, 92, 92}, title + " POWER", a, -1, Icon::Power));
    d.widgets.push_back(button(soloP, {b.r() - 120, b.y + 26, 92, 92}, "S", a));
    Widget s; s.kind = Kind::Selector; s.param = modeP; s.param2 = onP; s.box = {b.x + 40, b.y + 135, b.w - 80, 290};
    s.accent = a; s.icon = icon; s.label = title + " MODE";
    d.widgets.push_back(s);
    d.widgets.push_back(knob(trimP, b.r() - 78, b.b() - 62, 46, a, "TRIM", 0.5f));
    d.widgets[d.widgets.size() - 5].screen = {b.x + b.w * 0.20f, b.y + 22, b.w * 0.60f, b.h - 44};
}

// Compact chassis for the split plug-ins: same top / bottom bars scaled into a w x h plate.
inline void compactChassis(PanelDef& d, std::string name, float w, float h) {
    d.name = std::move(name);
    d.w = w; d.h = h;
    d.footer = "~ HYBRID ELITE MAFIA ~";
    d.title = {270, 58, w * 0.40f, 150};
    const float pr = w - 1830;   // the right-hand cluster keeps the full-size spacing
    d.preset = {pr + 0, 68, 560, 104};
    d.presetPrev = {pr + 568, 68, 104, 104};
    d.presetNext = {pr + 680, 68, 104, 104};
    d.presetSave = {pr + 806, 68, 116, 104};
    d.undo = {pr + 1000, 68, 116, 104};
    d.redo = {pr + 1130, 68, 116, 104};
    d.bypass = {pr + 1300, 64, 240, 112};
    d.inputTrim = {300, h - 150, 200, 90};
    d.inputMeter = {640, h - 126, 420, 42};
    d.footerBox = {1120, h - 165, w - 2240, 120};
    d.outputMeter = {w - 1070, h - 126, 420, 42};
    d.outputTrim = {w - 490, h - 150, 200, 90};
}
// A curve handle on an Analyzer display.
inline Widget handle(int freqP, int gainP, int band, Box display, Accent a, std::string label) {
    Widget w; w.kind = Kind::Handle; w.param = freqP; w.param2 = gainP; w.source = band; w.box = display; w.accent = a; w.label = std::move(label);
    return w;
}
inline Widget hmeter(int src, Box b, std::string label, Accent a, float lo, float hi) {
    Widget w = meter(src, b, std::move(label), a, lo, hi); w.vertical = false;
    return w;
}
inline Widget selector(int modeP, int onP, Box b, Accent a, Icon icon, std::string label) {
    Widget s; s.kind = Kind::Selector; s.param = modeP; s.param2 = onP; s.box = b; s.accent = a; s.icon = icon; s.label = std::move(label);
    return s;
}

} // namespace dm::ui::kit
