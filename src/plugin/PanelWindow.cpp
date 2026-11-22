// HEM Dark Matter Series - faceplate in a host's editor window (see PanelWindow.hpp). Windows only.
#include "dm/plugin/PanelWindow.hpp"

#include "hybrid/ui/Faceplate.hpp"
#include "hybrid/ui/FontAtlas.hpp"
#include "hybrid/ui/GlWindow.hpp"
#include "hybrid/ui/Parts.hpp"
#include "hybrid/ui/Renderer.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <optional>

namespace dm::plugin {

namespace {
struct SharedArt {
    std::mutex mutex;
    std::filesystem::path dir;
    std::optional<hc::ui::FontSet> fonts;
    std::optional<hc::ui::PartsAtlas> parts;
    std::optional<hc::ui::Image> frame;   // HEM frame art (3840 x 2160)
};
SharedArt& shared() {
    static SharedArt s;
    return s;
}
bool loadShared(const std::filesystem::path& assets, hc::ui::FontSet& fonts, hc::ui::PartsAtlas& parts, hc::ui::Image& frameArt,
                std::string* error) {
    SharedArt& s = shared();
    std::lock_guard lock(s.mutex);
    if (!s.fonts || !s.parts || !s.frame || s.dir != assets) {
        auto f = hc::ui::FontSet::load(assets / "fonts", error);
        if (!f) return false;
        auto art = hc::ui::loadFaceplateArt(assets / "faceplates" / "Main-Faceplate.png", error);
        if (!art) return false;
        auto fr = hc::ui::loadImageFile(assets / "faceplates" / "HEM-Frame-4K.jpg", error);
        if (!fr) return false;
        s.parts = hc::ui::buildPartsAtlas(art->master);
        s.fonts = std::move(*f);
        s.frame = std::move(*fr);
        s.dir = assets;
    }
    fonts = *s.fonts;
    parts = *s.parts;
    frameArt = *s.frame;
    return true;
}
} // namespace

struct PanelWindow::Impl {
    hc::ui::GlWindow window;
    hc::ui::Renderer renderer;
    hc::ui::FontAtlas font;
    hc::ui::Canvas canvas;
    const ui::PanelDef* def = nullptr;
    ui::PanelBinding* binding = nullptr;
    std::unique_ptr<ui::PanelController> controller;
    std::unique_ptr<ui::PanelView> view;
    bool open = false;

    void frame() {
        if (!open) return;
        window.makeCurrent();
        for (const hc::ui::InputEvent& e : window.takeEvents()) {
            const hc::ui::Canvas content = ui::contentCanvas(canvas, *def);   // the controller works in the plug-in's own units
            const float x = content.toDesignX(e.x), y = content.toDesignY(e.y);
            switch (e.type) {
            case hc::ui::InputEvent::Type::MouseDown: controller->mouseDown(x, y, e.doubleClick, e.fine); break;
            case hc::ui::InputEvent::Type::MouseMove: controller->mouseMove(x, y, e.fine); break;
            case hc::ui::InputEvent::Type::MouseUp: controller->mouseUp(x, y); break;
            case hc::ui::InputEvent::Type::Wheel: controller->wheel(x, y, e.wheel, e.fine); break;
            default: break;
            }
        }
        renderer.beginFrame(canvas);
        view->draw(renderer, *binding, *controller);
        renderer.endFrame(0);
        window.swap();
        window.restoreContext();
    }
};

PanelWindow::PanelWindow() : impl_(std::make_unique<Impl>()) {}
PanelWindow::~PanelWindow() { close(); }

// Every plug-in sits in the same HEM frame (3840 x 2160 frame pixels), so every editor window is 16:9.
void PanelWindow::sizeFor(const ui::PanelDef&, float scale, int& w, int& h) noexcept {
    w = std::max(320, static_cast<int>(std::lround(ui::frame::kW * scale)));
    h = std::max(180, static_cast<int>(std::lround(ui::frame::kH * scale)));
}

float PanelWindow::defaultScale(const ui::PanelDef&) noexcept {
    const UINT dpi = ::GetDpiForSystem();
    float scale = 0.45f * static_cast<float>(dpi ? dpi : 96) / 96.0f;
    RECT work{};
    if (::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        const float fitW = 0.9f * static_cast<float>(work.right - work.left) / ui::frame::kW;
        const float fitH = 0.9f * static_cast<float>(work.bottom - work.top) / ui::frame::kH;
        scale = std::max(0.15f, std::min({scale, fitW, fitH}));
    }
    return scale;
}

bool PanelWindow::open(void* parentHwnd, const ui::PanelDef& def, ui::PanelBinding& binding, const std::filesystem::path& assets,
                       float scale, std::string* error) {
    close();
    Impl& m = *impl_;
    int w = 0, h = 0;
    sizeFor(def, scale, w, h);
    if (!m.window.createChild(parentHwnd, w, h, error)) return false;
    auto fail = [&] { m.window.restoreContext(); close(); return false; };
    hc::ui::FontSet fonts;
    hc::ui::PartsAtlas parts;
    hc::ui::Image frameArt;
    if (!loadShared(assets, fonts, parts, frameArt, error)) return fail();
    if (!m.renderer.init(4, error) || !m.renderer.setParts(parts, error)) return fail();
    m.def = &def;
    m.binding = &binding;
    m.controller = std::make_unique<ui::PanelController>(def, binding);
    m.view = std::make_unique<ui::PanelView>(def);
    m.canvas = ui::frameCanvas(static_cast<float>(w), static_cast<float>(h));   // frame pixels -> window
    // text is drawn both at frame scale (top strip, side panels) and at the plug-in's scale: bake for the larger
    if (!m.font.bake(fonts, m.canvas.scale * std::max(1.0f, def.frame.s))) { if (error) *error = "could not bake the font atlas"; return fail(); }
    if (!m.renderer.setFont(m.font, error)) return fail();
    const float ppu = std::min(m.canvas.scale, 4096.0f / ui::frame::kW);
    if (!m.renderer.setPanel(hc::ui::buildMips(hc::ui::premultiply(ui::renderPlate(def, ppu, frameArt))), error)) return fail();
    if (!m.renderer.setTargetSize(w, h, error)) return fail();
    m.renderer.beginStatic(m.canvas);
    m.renderer.panel({0, 0, ui::frame::kW, ui::frame::kH});
    m.renderer.endStatic();
    m.window.restoreContext();
    m.open = true;
    m.window.setTimer(33, [this] { impl_->frame(); });
    return true;
}

void PanelWindow::close() noexcept {
    Impl& m = *impl_;
    if (m.window.nativeHandle()) {
        m.window.makeCurrent();
        m.renderer.shutdown();
        m.window.restoreContext();
    }
    m.window.destroy();
    m.view.reset();
    m.controller.reset();
    m.open = false;
}

bool PanelWindow::isOpen() const noexcept { return impl_->open; }

} // namespace dm::plugin
