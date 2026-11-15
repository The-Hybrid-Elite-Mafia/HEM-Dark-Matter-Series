// HEM Dark Matter Series - faceplate interaction.
#include "dm/ui/PanelController.hpp"

#include <algorithm>
#include <cmath>

namespace dm::ui {

namespace {
constexpr float kDragRange = 600.0f;   // design units of vertical drag for the full range
constexpr float kFine = 0.25f;
constexpr float kHandleRadius = 44.0f;
constexpr std::size_t kMenuRows = 22;
bool hitBox(const Box& b, float x, float y) { return b.contains(x, y); }
} // namespace

std::vector<std::string> PanelController::categories() const {
    std::vector<std::string> out;
    for (const auto& c : b_.presetCategories())
        if (!c.empty() && std::find(out.begin(), out.end(), c) == out.end()) out.push_back(c);
    return out;
}
std::vector<PanelController::MenuRow> PanelController::menuRows() const {
    std::vector<MenuRow> rows;
    const auto names = b_.presetNames();
    const auto cats = b_.presetCategories();
    const auto list = categories();
    if (list.empty() || cats.size() != names.size()) {
        for (int i = 0; i < static_cast<int>(names.size()); ++i) rows.push_back({false, i});
    } else {
        for (int c = 0; c < static_cast<int>(list.size()); ++c) {
            rows.push_back({true, c});
            if (c != openCat_) continue;
            for (int i = 0; i < static_cast<int>(names.size()); ++i)
                if (cats[static_cast<std::size_t>(i)] == list[static_cast<std::size_t>(c)]) rows.push_back({false, i});
        }
    }
    if (rows.size() > kMenuRows) rows.resize(kMenuRows);
    return rows;
}
Box PanelController::menuBox() const {
    const auto n = static_cast<float>(menuRows().size());
    return {def_.preset.x, def_.preset.b() + 8, def_.preset.w + 230, n * 66.0f + 20.0f};
}
Box PanelController::menuItem(int i) const {
    const Box m = menuBox();
    return {m.x + 10, m.y + 10 + static_cast<float>(i) * 66.0f, m.w - 20, 62};
}
void PanelController::handlePos(const Widget& w, float& x, float& y) const {
    std::vector<float> hz(1, 1000.0f);
    b_.feed(DisplayKind::Handles, w.source, hz);
    x = curveX(w.box, hz[0]);
    y = curveY(w.box, b_.value(w.param2), curveRange(w));
}

Hit PanelController::hitTest(float x, float y) const {
    if (menu_) {
        const auto rows = menuRows();
        for (int i = 0; i < static_cast<int>(rows.size()); ++i)
            if (hitBox(menuItem(i), x, y)) return {rows[static_cast<std::size_t>(i)].category ? Hit::T::MenuCategory : Hit::T::MenuItem, rows[static_cast<std::size_t>(i)].index};
        if (hitBox(menuBox(), x, y)) return {Hit::T::None, -1};
    }
    if (hitBox(def_.undo, x, y)) return {Hit::T::Undo, -1};
    if (hitBox(def_.redo, x, y)) return {Hit::T::Redo, -1};
    if (hitBox(def_.presetPrev, x, y)) return {Hit::T::PresetPrev, -1};
    if (hitBox(def_.presetNext, x, y)) return {Hit::T::PresetNext, -1};
    if (hitBox(def_.presetSave, x, y)) return {Hit::T::PresetSave, -1};
    if (hitBox(def_.preset, x, y)) return {Hit::T::Preset, -1};
    if (hitBox(def_.bypass, x, y)) return {Hit::T::Bypass, -1};
    if (hitBox(def_.inputTrim, x, y)) return {Hit::T::InTrim, -1};
    if (hitBox(def_.outputTrim, x, y)) return {Hit::T::OutTrim, -1};
    // Topmost widget wins: walk backwards (later widgets are drawn on top).
    for (int i = static_cast<int>(def_.widgets.size()) - 1; i >= 0; --i) {
        const Widget& w = def_.widgets[static_cast<std::size_t>(i)];
        if (w.kind == Kind::Bay || w.kind == Kind::Label || w.kind == Kind::Meter || w.kind == Kind::Display) continue;
        if (!isShown(w, b_)) continue;
        if (w.kind == Kind::Handle) {
            float hx = 0, hy = 0;
            handlePos(w, hx, hy);
            if ((x - hx) * (x - hx) + (y - hy) * (y - hy) <= kHandleRadius * kHandleRadius) return {Hit::T::Widget, i};
            continue;
        }
        if (!hitBox(w.box, x, y)) continue;
        if (w.kind == Kind::Selector) {
            if (x < w.box.x + w.box.w * 0.22f) return {Hit::T::SelPrev, i};
            if (x > w.box.r() - w.box.w * 0.22f) return {Hit::T::SelNext, i};
            return {Hit::T::SelNext, i};
        }
        if (w.kind == Kind::Range) {
            const float pa = rangeX(w, b_.value(w.param));
            const float pb = rangeX(w, b_.value(w.param2));
            return {std::fabs(x - pa) <= std::fabs(x - pb) ? Hit::T::RangeLo : Hit::T::RangeHi, i};
        }
        return {Hit::T::Widget, i};
    }
    return {};
}

void PanelController::nudge(int param, float normDelta) {
    if (param < 0) return;
    const auto& d = b_.desc(param);
    if (d.isChoice()) {
        const int n = static_cast<int>(d.choices.size());
        const int cur = static_cast<int>(std::lround(b_.value(param)));
        const int step = normDelta > 0 ? 1 : -1;
        b_.setValue(param, static_cast<float>(std::min(std::max(cur + step, 0), n - 1)));
        return;
    }
    b_.setValue(param, fromNorm(d, toNorm(d, b_.value(param)) + normDelta));
}

void PanelController::setFromPosition(const Widget& w, int param, float x, float y) {
    const auto& d = b_.desc(param);
    const Track t = faderTrack(w);
    float v;
    if (w.kind == Kind::Range) v = std::min(std::max(rangeHz(w, x), d.min), d.max);
    else {
        const float n = w.vertical ? 1.0f - (y - t.a) / std::max(t.b - t.a, 1.0f) : (x - t.a) / std::max(t.b - t.a, 1.0f);
        v = fromNorm(d, n);
    }
    if (w.kind == Kind::Range) { // handles may not cross
        if (param == w.param) v = std::min(v, b_.value(w.param2));
        else v = std::max(v, b_.value(w.param));
    }
    b_.setValue(param, v);
}

void PanelController::padSet(const Widget& w, float x, float y) {
    const float nx = std::min(std::max((x - w.box.x) / std::max(w.box.w, 1.0f), 0.0f), 1.0f);
    const float ny = std::min(std::max(1.0f - (y - w.box.y) / std::max(w.box.h, 1.0f), 0.0f), 1.0f);
    b_.setValue(w.param, fromNorm(b_.desc(w.param), nx));
    b_.setValue(w.param2, fromNorm(b_.desc(w.param2), ny));
}

void PanelController::mouseDown(float x, float y, bool doubleClick, bool fine) {
    (void)fine;
    const Hit h = hitTest(x, y);
    active_ = h;
    lastX_ = x; lastY_ = y;
    editParam_ = -1;
    if (menu_ && h.t != Hit::T::MenuItem && h.t != Hit::T::MenuCategory && h.t != Hit::T::Preset) menu_ = false;
    switch (h.t) {
    case Hit::T::MenuItem: b_.loadPreset(h.w); menu_ = false; return;
    case Hit::T::MenuCategory: openCat_ = openCat_ == h.w ? -1 : h.w; return;
    case Hit::T::Undo: b_.undo(); return;
    case Hit::T::Redo: b_.redo(); return;
    case Hit::T::Preset:
        menu_ = !menu_;
        if (menu_) {   // open on the current preset's category
            const auto cats = b_.presetCategories();
            const auto list = categories();
            const int cur = b_.currentPreset();
            openCat_ = -1;
            if (cur >= 0 && static_cast<std::size_t>(cur) < cats.size())
                for (int c = 0; c < static_cast<int>(list.size()); ++c)
                    if (list[static_cast<std::size_t>(c)] == cats[static_cast<std::size_t>(cur)]) openCat_ = c;
        }
        return;
    case Hit::T::PresetPrev: { const int n = static_cast<int>(b_.presetNames().size());
        if (n) b_.loadPreset((b_.currentPreset() - 1 + n) % n);
        return; }
    case Hit::T::PresetNext: { const int n = static_cast<int>(b_.presetNames().size());
        if (n) b_.loadPreset((b_.currentPreset() + 1) % n);
        return; }
    case Hit::T::PresetSave: b_.savePreset(); return;
    case Hit::T::Bypass:
        b_.beginEdit(def_.bypassParam);
        b_.setValue(def_.bypassParam, b_.value(def_.bypassParam) >= 0.5f ? 0.0f : 1.0f);
        b_.endEdit(def_.bypassParam);
        return;
    case Hit::T::InTrim: editParam_ = def_.inputParam; break;
    case Hit::T::OutTrim: editParam_ = def_.outputParam; break;
    case Hit::T::SelPrev: case Hit::T::SelNext: {
        const Widget& w = def_.widgets[static_cast<std::size_t>(h.w)];
        const auto& d = b_.desc(w.param);
        const int n = static_cast<int>(d.choices.size());
        const int cur = static_cast<int>(std::lround(b_.value(w.param)));
        b_.beginEdit(w.param);
        b_.setValue(w.param, static_cast<float>((cur + (h.t == Hit::T::SelNext ? 1 : n - 1)) % n));
        b_.endEdit(w.param);
        return;
    }
    case Hit::T::RangeLo: case Hit::T::RangeHi: {
        const Widget& w = def_.widgets[static_cast<std::size_t>(h.w)];
        editParam_ = h.t == Hit::T::RangeLo ? w.param : w.param2;
        b_.beginEdit(editParam_);
        if (doubleClick) b_.setValue(editParam_, b_.desc(editParam_).def);
        else setFromPosition(w, editParam_, x, y);
        return;
    }
    case Hit::T::Widget: {
        const Widget& w = def_.widgets[static_cast<std::size_t>(h.w)];
        if (w.kind == Kind::Button && w.param < 0) { if (w.source >= 0) b_.action(w.source); return; }
        if (w.kind == Kind::Pad) {
            editParam_ = w.param;
            b_.beginEdit(w.param);
            b_.beginEdit(w.param2);
            if (doubleClick) { b_.setValue(w.param, b_.desc(w.param).def); b_.setValue(w.param2, b_.desc(w.param2).def); }
            else padSet(w, x, y);
            return;
        }
        if (w.kind == Kind::Button) {
            b_.beginEdit(w.param);
            if (w.choice >= 0) b_.setValue(w.param, static_cast<float>(w.choice));
            else b_.setValue(w.param, b_.value(w.param) >= 0.5f ? 0.0f : 1.0f);
            b_.endEdit(w.param);
            return;
        }
        editParam_ = w.param;
        if (w.kind == Kind::Handle) {
            b_.beginEdit(w.param);
            b_.beginEdit(w.param2);
            if (doubleClick) { b_.setValue(w.param, b_.desc(w.param).def); b_.setValue(w.param2, b_.desc(w.param2).def); }
            return;
        }
        if (w.kind == Kind::Fader && !doubleClick) { b_.beginEdit(editParam_); setFromPosition(w, editParam_, x, y); return; }
        break;
    }
    default: return;
    }
    if (editParam_ >= 0) {
        b_.beginEdit(editParam_);
        if (doubleClick) b_.setValue(editParam_, b_.desc(editParam_).def);
    }
}

void PanelController::mouseMove(float x, float y, bool fine) {
    if (editParam_ < 0) { hover_ = hitTest(x, y); return; }
    const float dy = lastY_ - y;
    const float dx = x - lastX_;
    lastX_ = x; lastY_ = y;
    if (active_.t == Hit::T::Widget || active_.t == Hit::T::RangeLo || active_.t == Hit::T::RangeHi) {
        const Widget& w = def_.widgets[static_cast<std::size_t>(active_.w)];
        if (w.kind == Kind::Pad) { padSet(w, x, y); return; }
        if (w.kind == Kind::Handle) {
            // across: band frequency move over the display width; up / down: gain over its height
            const float k = fine ? kFine : 1.0f;
            nudge(w.param, dx / std::max(w.box.w * 0.5f, 1.0f) * k);
            const float span = std::max((w.box.h - 80.0f) * 0.5f, 1.0f);   // the display's dB range over half its curve area
            const auto& g = b_.desc(w.param2);
            b_.setValue(w.param2, std::min(std::max(b_.value(w.param2) + dy / span * curveRange(w) * k, g.min), g.max));
            return;
        }
        if (w.kind == Kind::Fader || w.kind == Kind::Range) { setFromPosition(w, editParam_, x, y); return; }
    }
    const auto& d = b_.desc(editParam_);
    if (d.isChoice()) {
        // choices step every 40 units of drag
        choiceAcc_ += dy;
        if (std::fabs(choiceAcc_) >= 40.0f) { nudge(editParam_, choiceAcc_); choiceAcc_ = 0; }
        return;
    }
    nudge(editParam_, dy / kDragRange * (fine ? kFine : 1.0f));
}

void PanelController::mouseUp(float, float) {
    if (active_.t == Hit::T::Widget && active_.w >= 0) {
        const Widget& w = def_.widgets[static_cast<std::size_t>(active_.w)];
        if (w.kind == Kind::Handle || w.kind == Kind::Pad) b_.endEdit(w.param2);
    }
    if (editParam_ >= 0) b_.endEdit(editParam_);
    editParam_ = -1;
    active_ = {};
}

void PanelController::wheel(float x, float y, float notches, bool fine) {
    const Hit h = hitTest(x, y);
    int p = -1;
    if (h.t == Hit::T::Widget) p = def_.widgets[static_cast<std::size_t>(h.w)].param;
    else if (h.t == Hit::T::SelPrev || h.t == Hit::T::SelNext) p = def_.widgets[static_cast<std::size_t>(h.w)].param;
    else if (h.t == Hit::T::InTrim) p = def_.inputParam;
    else if (h.t == Hit::T::OutTrim) p = def_.outputParam;
    if (p < 0) return;
    b_.beginEdit(p);
    nudge(p, notches * 0.01f * (fine ? kFine : 1.0f));
    b_.endEdit(p);
}

} // namespace dm::ui
