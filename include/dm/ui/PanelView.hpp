#pragma once
// HEM Dark Matter Series - faceplate drawing on the reused Hybrid Core renderer (OpenGL 3.3).
//   renderPlate() : static "Hybrid Elite Mafia" chassis on the CPU - chamfered brushed-gunmetal bezel,
//                   brushed titanium field, raised top / bottom bars, recessed obsidian bays,
//                   dark glass windows, hex screws. Lit from the top-left like every HEM faceplate.
//   PanelView     : draws everything that moves (knobs, faders, buttons, meters, displays, text,
//                   neon edge light) every frame from the binding.
#include "dm/ui/PanelController.hpp"
#include "dm/ui/PanelModel.hpp"
#include "hybrid/ui/Image.hpp"
#include "hybrid/ui/Renderer.hpp"

#include <vector>

namespace dm::ui {

[[nodiscard]] hc::ui::Rgba accentColour(Accent a) noexcept;
[[nodiscard]] hc::ui::Image renderPlate(const PanelDef& def, float pixelsPerUnit);

class PanelView {
public:
    explicit PanelView(const PanelDef& def) : def_(def) {}
    void draw(hc::ui::Renderer& r, PanelBinding& b, const PanelController& c);
    // Records one step of every scrolling display (Waveform). draw() calls it once per frame; the
    // snapshot tool calls it while it plays audio so the picture shows a real history.
    void sample(PanelBinding& b);

private:
    void drawChassisLights(hc::ui::Renderer& r);
    void drawTopBar(hc::ui::Renderer& r, PanelBinding& b, const PanelController& c);
    void drawBottomBar(hc::ui::Renderer& r, PanelBinding& b, const PanelController& c);
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
    void hMeter(hc::ui::Renderer& r, Box box, float dbL, float dbR);
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
