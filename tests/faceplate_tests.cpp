// HEM Dark Matter Series - faceplate tests (no graphics needed).
// For every plug-in faceplate: the layout check passes (every control bound, nothing off the plate,
// no two controls on top of each other), and every control on it is clicked / dragged through the real
// interaction code and must move its own parameter. Plus: bypass + trims, undo, the preset browser,
// curve handles, the XY pad and Chainbreaker's quick-load slots.
#include "TestHarness.hpp"

#include "dm/ui/Faceplates.hpp"
#include "dm/ui/PanelController.hpp"

#include <cmath>
#include <string>

using namespace dm::ui;

namespace {

int countChecked = 0;

// Snapshot of every parameter so a test can see what one gesture changed.
std::vector<float> values(const PanelBinding& b) {
    std::vector<float> v(b.paramCount());
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = b.value(static_cast<int>(i));
    return v;
}
bool changed(const PanelBinding& b, const std::vector<float>& before, int p) {
    return p >= 0 && std::fabs(b.value(p) - before[static_cast<std::size_t>(p)]) > 1e-6f;
}

// One gesture on one widget; returns true when it moved the widget's own parameter(s).
bool exercise(PanelController& c, PanelBinding& b, const PanelDef& def, std::size_t i) {
    const Widget& w = def.widgets[i];
    if (w.showParam >= 0) b.setValue(w.showParam, static_cast<float>(w.showValue));   // bring its slot up
    // start from defaults so earlier gestures in this test can't stack controls on top of each other
    if (w.kind == Kind::Range || w.kind == Kind::Handle) {
        b.setValue(w.param, b.desc(w.param).def);
        b.setValue(w.param2, b.desc(w.param2).def);
    }
    const auto before = values(b);
    const float cx = w.box.cx(), cy = w.box.cy();
    switch (w.kind) {
    case Kind::Knob: case Kind::Readout: case Kind::Trim: {
        c.mouseDown(cx, cy, false, false); c.mouseMove(cx, cy - 160, false); c.mouseUp(cx, cy - 160);
        if (changed(b, before, w.param)) return true;
        c.mouseDown(cx, cy, false, false); c.mouseMove(cx, cy + 320, false); c.mouseUp(cx, cy + 320);
        return changed(b, before, w.param);
    }
    case Kind::Fader: {
        const Track t = faderTrack(w);
        const float a = t.a + (t.b - t.a) * 0.15f, z = t.a + (t.b - t.a) * 0.85f;
        if (w.vertical) { c.mouseDown(cx, a, false, false); c.mouseUp(cx, a); }
        else { c.mouseDown(a, cy, false, false); c.mouseUp(a, cy); }
        const float v1 = b.value(w.param);
        if (w.vertical) { c.mouseDown(cx, z, false, false); c.mouseUp(cx, z); }
        else { c.mouseDown(z, cy, false, false); c.mouseUp(z, cy); }
        const float v2 = b.value(w.param);
        return w.vertical ? v1 > v2 : v2 > v1;   // top / right end is the higher value
    }
    case Kind::Range: {
        const float x = rangeX(w, b.value(w.param));
        c.mouseDown(x, cy, false, false); c.mouseMove(x + 40, cy, false); c.mouseUp(x + 40, cy);       // low handle inwards
        const float x2 = rangeX(w, b.value(w.param2));
        c.mouseDown(x2, cy, false, false); c.mouseMove(x2 - 40, cy, false); c.mouseUp(x2 - 40, cy);   // high handle inwards
        return changed(b, before, w.param) || changed(b, before, w.param2);
    }
    case Kind::Button: {
        if (w.param < 0) { c.mouseDown(cx, cy, false, false); c.mouseUp(cx, cy); return b.actionLit(w.source); }
        if (w.choice >= 0) {
            // pick another choice first so the click has something to change
            b.setValue(w.param, static_cast<float>(w.choice == 0 ? 1 : 0));
            c.mouseDown(cx, cy, false, false); c.mouseUp(cx, cy);
            return static_cast<int>(std::lround(b.value(w.param))) == w.choice;
        }
        c.mouseDown(cx, cy, false, false); c.mouseUp(cx, cy);
        return changed(b, before, w.param);
    }
    case Kind::Selector: {
        c.mouseDown(w.box.r() - 10, cy, false, false); c.mouseUp(w.box.r() - 10, cy);
        return changed(b, before, w.param);
    }
    case Kind::Handle: {
        float hx = 0, hy = 0;
        c.handlePos(w, hx, hy);
        c.mouseDown(hx, hy, false, false); c.mouseMove(hx + 60, hy - 60, false); c.mouseUp(hx + 60, hy - 60);
        return changed(b, before, w.param) && changed(b, before, w.param2);
    }
    case Kind::Pad: {
        const float x = w.box.x + w.box.w * 0.25f, y = w.box.y + w.box.h * 0.25f;
        c.mouseDown(x, y, false, false); c.mouseUp(x, y);
        const float nx = toNorm(b.desc(w.param), b.value(w.param)), ny = toNorm(b.desc(w.param2), b.value(w.param2));
        return std::fabs(nx - 0.25f) < 0.02f && std::fabs(ny - 0.75f) < 0.02f;
    }
    default: return true;
    }
}

bool interactive(const Widget& w) {
    switch (w.kind) {
    case Kind::Knob: case Kind::Readout: case Kind::Trim: case Kind::Fader: case Kind::Range:
    case Kind::Button: case Kind::Selector: case Kind::Handle: case Kind::Pad: return true;
    default: return false;
    }
}

} // namespace

void testPanel(const char* name, const PanelDef& def, PanelBinding& b, const std::vector<int>& hidden) {
    dmt::section(name);
    const auto issues = checkLayout(def, b, hidden);
    for (const auto& is : issues) std::printf("  layout: %s\n", is.what.c_str());
    dmt::check(issues.empty(), "layout: all bound, on the plate, no overlaps (issues)", double(issues.size()));

    PanelController c(def, b);
    int controls = 0, working = 0, reachable = 0;
    std::string broken;
    for (std::size_t i = 0; i < def.widgets.size(); ++i) {
        const Widget& w = def.widgets[i];
        if (!interactive(w)) continue;
        ++controls;
        if (w.showParam >= 0) b.setValue(w.showParam, static_cast<float>(w.showValue));
        if (w.kind == Kind::Handle) { b.setValue(w.param, b.desc(w.param).def); b.setValue(w.param2, b.desc(w.param2).def); }
        // the control must be the thing under the mouse at its own centre (handles: at their dot)
        float hx = w.box.cx(), hy = w.box.cy();
        if (w.kind == Kind::Handle) c.handlePos(w, hx, hy);
        const Hit h = c.hitTest(hx, hy);
        if (h.w == static_cast<int>(i)) ++reachable;
        else broken += " [unreachable " + w.label + "]";
        if (exercise(c, b, def, i)) ++working;
        else broken += " [no effect " + w.label + "]";
    }
    if (!broken.empty()) std::printf("  %s\n", broken.c_str());
    dmt::check(reachable == controls, "every control is under the mouse at its own spot (count)", double(reachable));
    dmt::check(working == controls, "every control moves its own parameter (count)", double(working));
    countChecked += controls;

    // chassis: BYPASS toggles, INPUT trim drags, undo puts it back
    const float bp = b.value(def.bypassParam);
    c.mouseDown(def.bypass.cx(), def.bypass.cy(), false, false); c.mouseUp(def.bypass.cx(), def.bypass.cy());
    dmt::check(b.value(def.bypassParam) != bp, "BYPASS button toggles bypass", b.value(def.bypassParam));
    c.mouseDown(def.bypass.cx(), def.bypass.cy(), false, false); c.mouseUp(def.bypass.cx(), def.bypass.cy());
    const float in0 = b.value(def.inputParam);
    c.mouseDown(def.inputTrim.cx(), def.inputTrim.cy(), false, false);
    c.mouseMove(def.inputTrim.cx(), def.inputTrim.cy() - 150, false);
    c.mouseUp(def.inputTrim.cx(), def.inputTrim.cy() - 150);
    const float in1 = b.value(def.inputParam);
    dmt::check(in1 > in0, "INPUT trim drags up (dB)", in1);
    b.undo();
    dmt::check(std::fabs(b.value(def.inputParam) - in0) < 1e-6f, "undo restores the INPUT trim (dB)", b.value(def.inputParam));

    // preset browser: open it, pick the last preset
    const auto names = b.presetNames();
    c.mouseDown(def.preset.cx(), def.preset.cy(), false, false); c.mouseUp(def.preset.cx(), def.preset.cy());
    dmt::check(c.menuOpen(), "preset box opens the browser", c.menuOpen() ? 1.0 : 0.0);
    const auto cats = c.categories();
    if (!cats.empty()) {   // open the last category, then click its last preset
        auto rows = c.menuRows();
        for (int r = 0; r < static_cast<int>(rows.size()); ++r)
            if (rows[static_cast<std::size_t>(r)].category && rows[static_cast<std::size_t>(r)].index == static_cast<int>(cats.size()) - 1) {
                const Box it = c.menuItem(r);
                c.mouseDown(it.cx(), it.cy(), false, false); c.mouseUp(it.cx(), it.cy());
            }
    }
    const auto rows = c.menuRows();
    int lastRow = -1;
    for (int r = 0; r < static_cast<int>(rows.size()); ++r) if (!rows[static_cast<std::size_t>(r)].category) lastRow = r;
    if (lastRow >= 0) {
        const int want = rows[static_cast<std::size_t>(lastRow)].index;
        const Box it = c.menuItem(lastRow);
        c.mouseDown(it.cx(), it.cy(), false, false); c.mouseUp(it.cx(), it.cy());
        dmt::check(b.currentPreset() == want && !c.menuOpen(), "clicking a preset in the browser loads it (index)", b.currentPreset());
    }
    dmt::check(names.size() >= 1, "factory presets available (count)", double(names.size()));
}

void runFaceplateTests() {
    {
        dm::pv::PhantomVox e; e.prepare(48000, 512);
        auto b = bindPhantomVox(e);
        testPanel("FACEPLATE HEM PHANTOM VOX", phantomVoxPanel(), *b, phantomVoxHidden());
    }
    {
        dm::sr::SyndicateRack e; e.prepare(48000, 512);
        auto b = bindSyndicate(e);
        testPanel("FACEPLATE HEM SYNDICATE RACK", syndicatePanel(), *b, shellHidden());
        const PanelDef def = syndicatePanel();
        PanelController c(def, *b);
        c.openMenu(-1);
        dmt::check(c.categories().size() == 8, "Syndicate preset browser has the 8 categories (count)", double(c.categories().size()));
    }
    {
        dm::sr::HemSat e1; e1.prepare(48000, 512); auto b1 = bindSplit(e1); testPanel("FACEPLATE HEM SAT", splitPanel(SplitKind::Sat), *b1, shellHidden());
        dm::sr::HemComp e2; e2.prepare(48000, 512); auto b2 = bindSplit(e2); testPanel("FACEPLATE HEM COMP", splitPanel(SplitKind::Comp), *b2, shellHidden());
        dm::sr::HemTransient e3; e3.prepare(48000, 512); auto b3 = bindSplit(e3); testPanel("FACEPLATE HEM TRANSIENT", splitPanel(SplitKind::Transient), *b3, shellHidden());
        dm::sr::HemTone e4; e4.prepare(48000, 512); auto b4 = bindSplit(e4); testPanel("FACEPLATE HEM TONE", splitPanel(SplitKind::Tone), *b4, shellHidden());
        dm::sr::HemMod e5; e5.prepare(48000, 512); auto b5 = bindSplit(e5); testPanel("FACEPLATE HEM MOD", splitPanel(SplitKind::Mod), *b5, shellHidden());
        dm::sr::HemMax e6; e6.prepare(48000, 512); auto b6 = bindSplit(e6); testPanel("FACEPLATE HEM MAX", splitPanel(SplitKind::Max), *b6, shellHidden());
    }
    {
        dm::ax::ApexCore e; e.prepare(48000, 512);
        auto b = bindApex(e);
        testPanel("FACEPLATE HEM APEX CORE", apexPanel(), *b, shellHidden());
    }
    {
        dm::il::IronLock e; e.prepare(48000, 512);
        auto b = bindIronLock(e);
        testPanel("FACEPLATE HEM IRON LOCK", ironLockPanel(), *b, shellHidden());
    }
    {
        dm::rd::OutlawRider e; e.prepare(48000, 512);
        auto b = bindRider(e);
        testPanel("FACEPLATE HEM OUTLAW RIDER", riderPanel(), *b, shellHidden());
    }
    {
        dm::d4::DarkMatter4D e; e.prepare(48000, 512);
        auto b = bindDarkMatter(e);
        testPanel("FACEPLATE HEM DARK MATTER 4D", darkMatterPanel(), *b, tempoHidden(static_cast<int>(dm::d4::Tempo)));
    }
    {
        dm::cb::Chainbreaker108 e; e.prepare(48000, 512);
        auto b = bindChainbreaker(e);
        testPanel("FACEPLATE HEM CHAINBREAKER 108", chainbreakerPanel(), *b, tempoHidden(static_cast<int>(dm::cb::Tempo)));
    }
    dmt::section("FACEPLATES");
    dmt::check(countChecked >= 300, "controls exercised across all 13 faceplates (count)", double(countChecked));
}
