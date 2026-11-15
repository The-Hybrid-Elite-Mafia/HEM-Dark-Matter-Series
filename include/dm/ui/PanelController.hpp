#pragma once
// HEM Dark Matter Series - faceplate interaction (no graphics code; unit-tested on any OS).
// Mouse / wheel input in design units -> parameter edits through the binding.
//   Knob, Readout, Trim : drag up/down (600 units = full range; Shift/Ctrl = 4x finer), wheel steps
//   Fader               : click/drag along the slot; Range: the nearer handle moves
//   Button              : toggle, or select its choice; Selector: < > step, click the face = next
//   Double-click        : back to the default value
//   Top bar             : undo / redo, preset < > + and the preset menu, BYPASS
#include "dm/ui/PanelModel.hpp"

namespace dm::ui {

struct Hit {
    enum class T : unsigned char {
        None, Widget, Undo, Redo, Preset, PresetPrev, PresetNext, PresetSave, Bypass, InTrim, OutTrim,
        SelPrev, SelNext, RangeLo, RangeHi, MenuItem
    };
    T   t = T::None;
    int w = -1;   // widget index (or menu item)
    [[nodiscard]] bool operator==(const Hit& o) const noexcept { return t == o.t && w == o.w; }
};

class PanelController {
public:
    PanelController(const PanelDef& def, PanelBinding& b) : def_(def), b_(b) {}

    void mouseDown(float x, float y, bool doubleClick, bool fine);
    void mouseMove(float x, float y, bool fine);
    void mouseUp(float x, float y);
    void wheel(float x, float y, float notches, bool fine);

    [[nodiscard]] Hit hover() const noexcept { return hover_; }
    [[nodiscard]] Hit active() const noexcept { return active_; }
    [[nodiscard]] bool menuOpen() const noexcept { return menu_; }
    // Preset menu geometry (shared with the view).
    [[nodiscard]] Box menuBox() const;
    [[nodiscard]] Box menuItem(int i) const;

    [[nodiscard]] Hit hitTest(float x, float y) const;

private:
    void nudge(int param, float normDelta);
    void setFromPosition(const Widget& w, int param, float x, float y);

    const PanelDef& def_;
    PanelBinding& b_;
    Hit hover_{}, active_{};
    int editParam_ = -1;
    float lastY_ = 0, lastX_ = 0, choiceAcc_ = 0;
    bool menu_ = false;
};

} // namespace dm::ui
