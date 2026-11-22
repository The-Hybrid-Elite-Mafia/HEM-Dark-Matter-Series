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
};
SharedArt& shared() {
    static SharedArt s;
    return s;
}
bool loadShared(const std::filesystem::path& assets, hc::ui::FontSet& fonts, hc::ui::PartsAtlas& parts, std::string* error) {
    SharedArt& s = shared();
    std::lock_guard lock(s.mutex);
    if (!s.fonts || !s.parts || s.dir != assets) {
        auto f = hc::ui::FontSet::load(assets / "fonts", error);
        if (!f) return false;
        auto art = hc::ui::loadFaceplateArt(assets / "faceplates" / "Main-Faceplate.png", error);
        if (!art) return false;
        s.parts = hc::ui::buildPartsAtlas(art->master);
        s.fonts = std::move(*f);
        s.dir = assets;
    }
    fonts = *s.fonts;
    parts = *s.parts;
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
            const float x = canvas.toDesignX(e.x), y = canvas.toDesignY(e.y);
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

void PanelWindow::sizeFor(const ui::PanelDef& def, float scale, int& w, int& h) noexcept {
    w = std::max(320, static_cast<int>(std::lround(def.w * scale)));
    h = std::max(200, static_cast<int>(std::lround(def.h * scale)));
}

float PanelWindow::defaultScale(const ui::PanelDef& def) noexcept {
    const UINT dpi = ::GetDpiForSystem();
    float scale = 0.36f * static_cast<float>(dpi ? dpi : 96) / 96.0f;
    RECT work{};
    if (::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        const float fitW = 0.9f * static_cast<float>(work.right - work.left) / def.w;
        const float fitH = 0.9f * static_cast<float>(work.bottom - work.top) / def.h;
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
    if (!loadShared(assets, fonts, parts, error)) return fail();
    if (!m.renderer.init(4, error) || !m.renderer.setParts(parts, error)) return fail();
    m.def = &def;
    m.binding = &binding;
    m.controller = std::make_unique<ui::PanelController>(def, binding);
    m.view = std::make_unique<ui::PanelView>(def);
    m.canvas = hc::ui::Canvas{};
    m.canvas.scale = std::min(static_cast<float>(w) / def.w, static_cast<float>(h) / def.h);
    m.canvas.offsetX = std::round((static_cast<float>(w) - def.w * m.canvas.scale) * 0.5f);
    m.canvas.offsetY = std::round((static_cast<float>(h) - def.h * m.canvas.scale) * 0.5f);
    m.canvas.pixelW = static_cast<float>(w);
    m.canvas.pixelH = static_cast<float>(h);
    if (!m.font.bake(fonts, m.canvas.scale)) { if (error) *error = "could not bake the font atlas"; return fail(); }
    if (!m.renderer.setFont(m.font, error)) return fail();
    const float ppu = std::min({m.canvas.scale, 4096.0f / def.w, 4096.0f / def.h});
    if (!m.renderer.setPanel(hc::ui::buildMips(hc::ui::premultiply(ui::renderPlate(def, ppu))), error)) return fail();
    if (!m.renderer.setTargetSize(w, h, error)) return fail();
    m.renderer.beginStatic(m.canvas);
    m.renderer.panel({0, 0, def.w, def.h});
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
