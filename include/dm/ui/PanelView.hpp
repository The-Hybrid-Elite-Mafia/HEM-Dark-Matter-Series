#pragma once
// HEM Dark Matter Series - faceplate drawing on the reused Hybrid Core renderer (OpenGL 3.3).
//   renderPlate() : static plate on the CPU - the HEM frame art (Steven's template: chassis, light strips,
//                   hooded crown emblem, circuit board under glass) with this plug-in's smoked-glass
//                   panels, metal mode plates, display windows and side panels cut into the glass bed.
//   PanelView     : draws everything that moves (knobs, faders, buttons, meters, displays, text) every
//                   frame from the binding. Top strip + side panels in frame pixels, the plug-in's own
//                   controls in its design units (contentCanvas).
#include "dm/ui/PanelController.hpp"
#include "dm/ui/PanelModel.hpp"
#include "hybrid/ui/Image.hpp"
#include "hybrid/ui/Renderer.hpp"

#include <vector>

namespace dm::ui {

[[nodiscard]] hc::ui::Rgba accentColour(Accent a) noexcept;
// frameArt: assets/faceplates/HEM-Frame-4K.jpg (3840 x 2160). pixelsPerUnit: output pixels per frame pixel.
[[nodiscard]] hc::ui::Image renderPlate(const PanelDef& def, float pixelsPerUnit, const hc::ui::Image& frameArt);
// Window canvas for the whole frame (frame pixels -> window pixels, letterboxed, centred).
[[nodiscard]] hc::ui::Canvas frameCanvas(float pixelW, float pixelH) noexcept;
// Canvas for a plug-in's own design units inside the frame canvas (what the controller's mouse uses).
[[nodiscard]] hc::ui::Canvas contentCanvas(const hc::ui::Canvas& frame, const PanelDef& def) noexcept;

class PanelView {
public:
    explicit PanelView(const PanelDef& def) : def_(def) {}
    void draw(hc::ui::Renderer& r, PanelBinding& b, const PanelController& c);
    // Records one step of every scrolling display (Waveform). draw() calls it once per frame; the
    // snapshot tool calls it while it plays audio so the picture shows a real history.
    void sample(PanelBinding& b);

private:
    void drawTopStrip(hc::ui::Renderer& r, PanelBinding& b, const PanelController& c);   // frame pixels
    void drawSidePanels(hc::ui::Renderer& r, PanelBinding& b, const PanelController& c); // frame pixels
    void drawWidget(hc::ui::Renderer& r, PanelBinding& b, const Widget& w, bool hot);
    void drawKnob(hc::ui::Renderer& r, PanelBinding& b, const Widget& w, bool hot);
    void drawFader(hc::ui::Renderer& r, PanelBinding& b, const Widget& w, bool hot);
    void drawRange(hc::ui::Renderer& r, PanelBinding& b, const Widget& w, bool hot);
    void drawButton(hc::ui::Renderer& r, PanelBinding& b, const Widget& w, bool hot);
    void drawSelector(hc::ui::Renderer& r, PanelBinding& b, const Widget& w, bool hot);
    void drawReadout(hc::ui::Renderer& r, PanelBinding& b, const Widget& w, bool hot);
    void drawMeter(hc::ui::Renderer& r, PanelBinding& b, const Widget& w);
    void drawDisplay(hc::ui::Renderer& r, PanelBinding& b, const Widget& w);
    void drawBay(hc::ui::Renderer& r, const Widget& w);
    void drawHandle(hc::ui::Renderer& r, PanelBinding& b, const Widget& w, bool hot);
    void drawPad(hc::ui::Renderer& r, PanelBinding& b, const Widget& w, bool hot);
    void drawIcon(hc::ui::Renderer& r, Icon icon, float cx, float cy, float size, hc::ui::Rgba c, bool lit);
    void drawPresetMenu(hc::ui::Renderer& r, PanelBinding& b, const PanelController& c);
    void vMeter(hc::ui::Renderer& r, Box box, float dbL, float dbR);
    void capChrome(hc::ui::Renderer& r, float cx, float cy, float w, float h, hc::ui::Rgba line, bool horizontal);

    const PanelDef& def_;
    std::vector<float> feed_;
    std::vector<float> smooth_;   // per meter source, falls back smoothly
    static constexpr std::size_t kHistory = 480;   // steps kept per scrolling display
    std::vector<std::vector<float>> hist_;         // per widget: kHistory x 3 values, ring
    std::vector<std::size_t> histPos_;
    bool sampledThisFrame_ = false;
};

} // namespace dm::ui
