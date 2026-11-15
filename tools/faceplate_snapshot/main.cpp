// dm_faceplate_snapshot - renders every Dark Matter Series faceplate off-screen with the real faceplate
// code (reused Hybrid Core OpenGL renderer), after running each engine on a few seconds of test audio so
// meters and displays are live. Writes one PNG per plug-in.
//   dm_faceplate_snapshot [--out <folder>] [--width 1920] [--assets <dir>]
// Windows: hidden GL window (CI uses Mesa's software driver). Linux: headless Mesa EGL.
// Exit 3 when a faceplate fails to render or comes out blank; exit 4 on a layout problem.
#include "dm/ui/Faceplates.hpp"
#include "dm/ui/PanelController.hpp"
#include "dm/ui/PanelView.hpp"
#include "hybrid/ui/Faceplate.hpp"
#include "hybrid/ui/FontAtlas.hpp"
#include "hybrid/ui/Gl.hpp"
#include "hybrid/ui/Parts.hpp"
#include "hybrid/ui/Renderer.hpp"

#if defined(_WIN32)
#include "hybrid/ui/GlWindow.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

using namespace dm::ui;
namespace fs = std::filesystem;

#if !defined(_WIN32)
// ---- minimal EGL (no headers needed) ----
extern "C" {
using EGLDisplay = void*; using EGLConfig = void*; using EGLContext = void*; using EGLSurface = void*; using EGLint = int;
EGLDisplay eglGetDisplay(void*); EGLDisplay eglGetPlatformDisplay(unsigned, void*, const long*); unsigned eglInitialize(EGLDisplay, EGLint*, EGLint*); unsigned eglBindAPI(unsigned);
unsigned eglChooseConfig(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
EGLSurface eglCreatePbufferSurface(EGLDisplay, EGLConfig, const EGLint*);
EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
unsigned eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext); EGLint eglGetError();
}
bool makeContext(std::string* err) {
    EGLDisplay d = eglGetPlatformDisplay(0x31DD, nullptr, nullptr);   // EGL_PLATFORM_SURFACELESS_MESA
    if (!d) d = eglGetDisplay(nullptr);
    EGLint maj = 0, min = 0;
    if (!d || !eglInitialize(d, &maj, &min)) { *err = "eglInitialize failed"; return false; }
    eglBindAPI(0x30A2);   // EGL_OPENGL_API
    const EGLint cfgAttr[] = {0x3040, 0x0008, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3038};
    EGLConfig cfg = nullptr; EGLint n = 0;
    if (!eglChooseConfig(d, cfgAttr, &cfg, 1, &n) || n < 1) cfg = nullptr;   // EGL_KHR_no_config_context
    EGLSurface s = nullptr;   // surfaceless: everything renders into the renderer's own framebuffers
    const EGLint ctxAttr[] = {0x3098, 3, 0x30FB, 3, 0x30FD, 0x00000001, 0x3038};   // 3.3 core
    EGLContext c = eglCreateContext(d, cfg, nullptr, ctxAttr);
    if (!c) { *err = "eglCreateContext failed: " + std::to_string(eglGetError()); return false; }
    if (!eglMakeCurrent(d, s, s, c)) { *err = "eglMakeCurrent failed"; return false; }
    if (const char* miss = hc::gl::loadFunctions()) { *err = std::string("missing GL function ") + miss; return false; }
    return true;
}
#endif

namespace {
struct Job {
    std::string name;
    std::function<void()> runAudio;
    std::function<PanelBinding&()> binding;
    PanelDef def;
    std::vector<int> hidden;
    int menuCategory = -1;   // >= 0: draw with the preset browser open on this category
};

double lumaStdDev(const hc::ui::Image& img) {
    double s = 0, sq = 0;
    const std::size_t n = static_cast<std::size_t>(img.width) * img.height;
    for (std::size_t i = 0; i < n; ++i) {
        const auto* p = &img.rgba[i * 4];
        const double l = 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2];
        s += l; sq += l * l;
    }
    const double m = s / double(std::max<std::size_t>(n, 1));
    return std::sqrt(std::max(0.0, sq / double(std::max<std::size_t>(n, 1)) - m * m));
}

// Vocal-ish test audio for the engines (glide voice + sibilants + gaps).
void vocal(std::vector<float>& l, std::vector<float>& r, double sr, std::size_t n) {
    l.assign(n, 0.0f); r.assign(n, 0.0f);
    double ph = 0; unsigned s = 7;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = double(i) / sr, cyc = std::fmod(t, 1.0);
        ph += 6.283185307 * (150.0 + 60.0 * std::sin(6.283185307 * 0.4 * t)) / sr;
        double v = 0;
        for (int h = 1; h <= 10; ++h) v += std::sin(ph * h) / (h * 1.2);
        s = s * 1664525u + 1013904223u;
        const double nz = (double(s >> 8) / 8388608.0 - 1.0);
        double x = cyc < 0.6 ? 0.22 * v * std::sin(3.14159 * cyc / 0.6) : (cyc < 0.75 ? 0.12 * nz : 0.0);
        l[i] = float(x); r[i] = float(x * 0.96);
    }
}
} // namespace

int main(int argc, char** argv) {
    fs::path out = "faceplates", assets = "assets";
    int width = 1920;
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--out") out = argv[++i];
        else if (a == "--width") width = std::atoi(argv[++i]);
        else if (a == "--assets") assets = argv[++i];
    }
    fs::create_directories(out);
    std::string err;

#if defined(_WIN32)
    hc::ui::GlWindow window;
    if (!window.create(L"HEM Dark Matter snapshot", 64, 64, false, &err)) { std::printf("FAIL window: %s\n", err.c_str()); return 3; }
#else
    if (!makeContext(&err)) { std::printf("FAIL context: %s\n", err.c_str()); return 3; }
#endif
    auto fonts = hc::ui::FontSet::load(assets / "fonts", &err);
    if (!fonts) { std::printf("FAIL fonts: %s\n", err.c_str()); return 3; }
    auto art = hc::ui::loadFaceplateArt(assets / "faceplates" / "Main-Faceplate.png", &err);
    if (!art) { std::printf("FAIL parts art: %s\n", err.c_str()); return 3; }
    const hc::ui::PartsAtlas parts = hc::ui::buildPartsAtlas(art->master);

    // ---- engines ----
    static dm::pv::PhantomVox pv;
    pv.prepare(48000, 512);
    auto pvBind = bindPhantomVox(pv);
    std::vector<Job> jobs;
    jobs.push_back({"HEM_PHANTOM_VOX",
                    [] { std::vector<float> l, r; vocal(l, r, 48000, 48000 * 3 - 19200);   // stop mid-phrase so the meters + analyzer show a live voice
                         for (std::size_t p = 0; p < l.size(); p += 512) pv.process(l.data() + p, r.data() + p, int(std::min<std::size_t>(512, l.size() - p))); },
                    [&]() -> PanelBinding& { return *pvBind; }, phantomVoxPanel(), phantomVoxHidden()});

    // Shell engines: feed the same voice through the engine's stereo block process.
    auto feedShell = [](auto& e) {
        std::vector<float> l, r;
        vocal(l, r, 48000, 48000 * 3 - 19200);
        for (std::size_t p = 0; p < l.size(); p += 512) e.process(l.data() + p, r.data() + p, int(std::min<std::size_t>(512, l.size() - p)));
    };
    static dm::sr::SyndicateRack rack;
    rack.prepare(48000, 512);
    rack.loadPreset(0);
    auto rackBind = bindSyndicate(rack);
    jobs.push_back({"HEM_SYNDICATE_RACK", [&] { feedShell(rack); }, [&]() -> PanelBinding& { return *rackBind; }, syndicatePanel(), shellHidden()});
    jobs.push_back({"HEM_SYNDICATE_RACK_PRESETS", [] {}, [&]() -> PanelBinding& { return *rackBind; }, syndicatePanel(), shellHidden(), 7});

    int failures = 0;
    for (auto& job : jobs) {
        const auto issues = checkLayout(job.def, job.binding(), job.hidden);
        for (const auto& is : issues) std::printf("LAYOUT %-18s %s\n", job.name.c_str(), is.what.c_str());
        if (!issues.empty()) ++failures;
        job.runAudio();

        hc::ui::Renderer r;
        if (!r.init(4, &err) || !r.setParts(parts, &err)) { std::printf("FAIL %s renderer: %s\n", job.name.c_str(), err.c_str()); ++failures; continue; }
        hc::ui::Canvas canvas{};
        const int height = static_cast<int>(std::lround(width * job.def.h / job.def.w));
        canvas.scale = static_cast<float>(width) / job.def.w;
        canvas.offsetX = 0; canvas.offsetY = 0;
        canvas.pixelW = static_cast<float>(width); canvas.pixelH = static_cast<float>(height);
        hc::ui::FontAtlas font;
        if (!font.bake(*fonts, canvas.scale) || !r.setFont(font, &err)) { std::printf("FAIL font bake\n"); ++failures; continue; }
        const float ppu = std::min(canvas.scale, 4096.0f / job.def.w);
        if (!r.setPanel(hc::ui::buildMips(hc::ui::premultiply(renderPlate(job.def, ppu))), &err) || !r.setTargetSize(width, height, &err)) {
            std::printf("FAIL %s targets: %s\n", job.name.c_str(), err.c_str()); ++failures; continue;
        }
        r.beginStatic(canvas);
        r.panel({0, 0, job.def.w, job.def.h});
        r.endStatic();
        PanelController ctl(job.def, job.binding());
        if (job.menuCategory >= 0) ctl.openMenu(job.menuCategory);   // preset-browser view
        PanelView view(job.def);
        r.beginFrame(canvas);
        view.draw(r, job.binding(), ctl);
        r.endFrameToCapture();
        const hc::ui::Image img = r.capture();
        const double sd = img.empty() ? 0.0 : lumaStdDev(img);
        hc::ui::writePng(out / (job.name + ".png"), img);
        const bool ok = !img.empty() && sd > 4.0;
        std::printf("%s %-18s %u x %u  luma sd %.1f  quads %u\n", ok ? "ok  " : "FAIL", job.name.c_str(), img.width, img.height, sd, r.stats().quads);
        if (!ok) ++failures;
        r.shutdown();
    }
    return failures ? 3 : 0;
}
