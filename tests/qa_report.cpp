// HEM QA pass for the 13 HEM Dark Matter plug-ins: stability, parameter sweep, meter liveness and the
// HEM-board faceplate (PluginInfo::panel()). Report only — writes qa_report_darkmatter.csv and never fails
// the test run. Run it with: dm_tests qa
#include "QaCommon.hpp"
#include "dm/ironlock/IronLock.hpp"
#include "dm/plugin/Registry.hpp"
#include "dm/ui/PanelModel.hpp"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

using hemqa::Stereo;
using namespace dm::plugin;
using namespace dm::ui;

// In-place processing with the same test signal as the sidechain key (a copy, so the key is the dry signal).
Stereo run(EngineApi& e, const Stereo& in, bool key) {
    Stereo out = in;
    for (std::size_t pos = 0; pos < out.size(); pos += hemqa::kBlock) {
        const int n = static_cast<int>(std::min<std::size_t>(hemqa::kBlock, out.size() - pos));
        e.process(out.l.data() + pos, out.r.data() + pos, n, key ? in.l.data() + pos : nullptr, key ? in.r.data() + pos : nullptr);
    }
    return out;
}

hemqa::Rect qr(const Box& b) { return {b.x, b.y, b.w, b.h}; }

bool stepped(const dm::ParamDesc& d) { return d.isChoice() || d.isToggle(); }
float stepMax(const dm::ParamDesc& d) { return d.isChoice() ? static_cast<float>(d.choices.size() - 1) : d.max; }

float middle(const dm::ParamDesc& d) {
    if (d.isChoice()) return static_cast<float>(d.choices.size() / 2);
    return fromNorm(d, 0.5f);
}

bool keepAtDefault(const dm::ParamDesc& d, std::size_t index, int bypassParam) {
    if (static_cast<int>(index) == bypassParam) return true;
    const std::string id(d.id);
    for (const char* k : {"bypass", "solo", "listen", "mute", "freeze", "delta", "audition", "learn", "capture"})
        if (id.find(k) != std::string::npos) return true;
    return false;
}

std::string id(const EngineApi& e, std::size_t i) { return std::string(e.desc(i).id); }

// Two IRON LOCK instances in one process, as two tracks in a DAW. A has GROUPS on and a scale of C only;
// B plays a voice with its own default scale. B's GROUPS on must make it follow A's scale (different audio).
bool ironLockGroupsLive(const PluginInfo& info, const Stereo& voice) {
    auto renderB = [&](bool groups) {
        auto a = info.create();
        auto b = info.create();
        for (auto* x : {a.get(), b.get()}) {
            x->prepare(static_cast<float>(hemqa::kRate), hemqa::kBlock);
            for (std::size_t k = 0; k < x->paramCount(); ++k) x->set(k, x->desc(k).def);
            x->set(static_cast<std::size_t>(dm::il::Strength), 100.0f);
            x->set(static_cast<std::size_t>(dm::il::Range), 600.0f);
            x->set(static_cast<std::size_t>(dm::il::Mix), 100.0f);   // MIX loads at 0 (zero colour on load)
            x->reset();
        }
        a->set(static_cast<std::size_t>(dm::il::Groups), 1.0f);
        b->set(static_cast<std::size_t>(dm::il::Groups), groups ? 1.0f : 0.0f);
        Stereo sa(hemqa::kBlock);
        (void)run(*a, sa, false);   // A applies GROUPS
        for (std::size_t n = 0; n < 12; ++n)
            a->set(static_cast<std::size_t>(dm::il::NoteC) + n, n == 0 ? 1.0f : 0.0f);   // A changes its scale: C only
        (void)run(*a, sa, false);   // ...and shares it
        Stereo o = run(*b, voice, false);
        hemqa::Hash h;
        h.add(o);
        return h.h;
    };
    return renderB(true) != renderB(false);
}

void auditDsp(const PluginInfo& info, hemqa::Report& rep) {
    const std::string name = info.name;
    auto e = info.create();
    const std::size_t count = e->paramCount();
    const std::vector<int> hiddenList = info.hidden ? info.hidden() : std::vector<int>{};
    const std::set<int> hidden(hiddenList.begin(), hiddenList.end());
    auto sweepable = [&](std::size_t i) { return hostVisible(info, i) && !hidden.count(static_cast<int>(i)); };
    const bool key = info.sidechain;
    const Stereo full = hemqa::testAudio(1.0), voice = hemqa::voiceAudio(1.0), silence(static_cast<std::size_t>(0.5 * hemqa::kRate));

    const int bypassParam = info.panel().bypassParam;
    auto defaults = [&] { for (std::size_t k = 0; k < count; ++k) e->set(k, e->desc(k).def); };

    e->prepare(static_cast<float>(hemqa::kRate), hemqa::kBlock);
    defaults();
    e->reset();
    if (!hemqa::finite(run(*e, full, key))) rep.add(name, "DSP", "NAN_OR_INF", "defaults", "non-finite output on 1 s of test audio at default settings");

    // Settings of the other controls: 0 defaults, 1 all at min, 2 all at max, 3 all at middle,
    // 4 defaults played with a single voice (pitch tools), 5 defaults with ONE other control (pairK) at pairV.
    std::size_t pairK = 0;
    float pairV = 0;
    auto setContext = [&](std::size_t skip, int mode) {
        defaults();
        if (mode == 0 || mode == 4) return;
        if (mode == 5) { if (pairK != skip) e->set(pairK, pairV); return; }
        if (mode >= 6) {   // 6 / 8: the coloured starting point (the old defaults the factory presets were written over),
                           // full mix / single voice; 7: the same with ONE other control (pairK) at pairV
            for (const auto& pv : e->presetBase())
                for (std::size_t k = 0; k < count; ++k)
                    if (k != skip && e->desc(k).id == pv.id) e->set(k, pv.value);
            if (mode == 7 && pairK != skip) e->set(pairK, pairV);
            return;
        }
        for (std::size_t k = 0; k < count; ++k) {
            const auto& d = e->desc(k);
            if (k == skip || !sweepable(k) || keepAtDefault(d, k, bypassParam)) continue;
            e->set(k, mode == 1 ? d.min : mode == 2 ? stepMax(d) : middle(d));
        }
    };
    constexpr std::size_t kNone = static_cast<std::size_t>(-1);
    bool fresh = false;
    auto stateHash = [&](int mode, std::size_t param, float value, bool& blewUp, bool& getOk) {
        if (fresh) { e = info.create(); e->prepare(static_cast<float>(hemqa::kRate), hemqa::kBlock); }
        setContext(param, mode);
        if (param != kNone) {
            e->set(param, value);
            const auto& d = e->desc(param);
            const float want = std::clamp(value, d.min, stepMax(d));
            getOk = hemqa::closeTo(e->get(param), stepped(d) ? std::round(want) : want);
        }
        e->reset();
        const Stereo o = run(*e, mode == 4 || mode == 8 ? voice : full, key);
        blewUp = !hemqa::finite(o);
        hemqa::Hash h;
        h.add(o);
        return h.h;
    };
    bool dummyBlew = false, dummyGet = true;
    bool deterministic = stateHash(0, kNone, 0, dummyBlew, dummyGet) == stateHash(0, kNone, 0, dummyBlew, dummyGet);
    if (!deterministic) {
        // reset() does not bring the engine back to the same start: run every state on a fresh instance instead
        fresh = true;
        if (stateHash(0, kNone, 0, dummyBlew, dummyGet) == stateHash(0, kNone, 0, dummyBlew, dummyGet)) {
            deterministic = true;
            rep.add(name, "DSP", "RESET_NOT_REPEATABLE", "reset()", "two runs after reset() differ (a fresh instance repeats): offline renders may not match; sweep ran on fresh instances");
        } else {
            rep.note(name, "output differs even between two fresh instances (random source): DEAD_WIRE check skipped");
        }
    }
    for (int mode = 1; mode <= 3; ++mode) {
        bool b = false, g = true;
        (void)stateHash(mode, kNone, 0, b, g);
        if (b) rep.add(name, "DSP", "NAN_OR_INF_SWEEP", mode == 1 ? "all controls at min" : mode == 2 ? "all controls at max" : "all controls at middle", "non-finite output");
    }

    for (std::size_t i = 0; i < count; ++i) {
        if (!sweepable(i)) continue;
        const auto& d = e->desc(i);
        bool sampled = false;
        const std::vector<float> values = hemqa::sweepValues(d.min, d.def, stepMax(d), stepped(d), &sampled);
        if (sampled) rep.note(name, id(*e, i) + ": stepped control with more than 64 steps, 64 sampled");
        bool blew = false, mism = false, live = !deterministic;
        std::string blewAt, mismAt;
        for (int mode = 0; mode <= 3 && !live; ++mode) {
            std::set<std::uint64_t> hashes;
            for (float v : values) {
                bool b = false, g = true;
                hashes.insert(stateHash(mode, i, v, b, g));
                if (b && !blew) { blew = true; blewAt = hemqa::fmt("%g", v); }
                if (!g && !mism) { mism = true; mismAt = hemqa::fmt("%g", v); }
            }
            live = hashes.size() > 1;
        }
        if (deterministic && live && !blew) {
            for (float v : values) {
                bool b = false, g = true;
                (void)stateHash(0, i, v, b, g);
                if (b) { blew = true; blewAt = hemqa::fmt("%g", v); break; }
                if (!g && !mism) { mism = true; mismAt = hemqa::fmt("%g", v); }
            }
        }
        if (blew) rep.add(name, "DSP", "NAN_OR_INF_SWEEP", id(*e, i), "non-finite output at " + blewAt);
        if (mism) rep.add(name, "DSP", "GET_SET_MISMATCH", id(*e, i), "get() != set() at " + mismAt);
        auto liveIn = [&](int mode) {
            std::set<std::uint64_t> hashes;
            for (float v : values) { bool b = false, g = true; hashes.insert(stateHash(mode, i, v, b, g)); }
            return hashes.size() > 1;
        };
        // Still silent: a control can depend on what is played (a pitch tool needs a voice) or on ONE other
        // switch (a stutter's rate only matters once FLUX triggers it). Try both before calling it dead.
        if (!live) live = liveIn(4);
        // Zero colour on load: many controls only act once the colour is up. Try the coloured starting point
        // (the old defaults the presets were written over), with the full mix and with a single voice.
        if (!live) live = liveIn(6) || liveIn(8);
        for (std::size_t k = 0; k < count && !live; ++k) {
            if (k == i || !sweepable(k) || keepAtDefault(e->desc(k), k, bypassParam)) continue;
            const auto& dk = e->desc(k);
            for (float o : hemqa::sweepValues(dk.min, dk.def, stepMax(dk), stepped(dk))) {
                if (o == dk.def) continue;
                pairK = k; pairV = o;
                if ((live = liveIn(5) || liveIn(7))) break;
            }
        }
        // IRON LOCK GROUPS shares the scale between instances: it can only be heard with a second instance.
        if (!live && std::string(info.name) == "HEM IRON LOCK" && id(*e, i) == "groups") live = ironLockGroupsLive(info, voice);
        if (!live) rep.add(name, "DSP", "DEAD_WIRE", id(*e, i), "audio bit-identical at all " + std::to_string(values.size()) +
                           " values, with the other controls at default / min / max / middle, with a single voice, and with each"
                           " other control moved on its own");
    }

    // meters: the bottom-bar IN / OUT meters and every meter widget on the plate
    const PanelDef def = info.panel();
    struct Src { std::string name; int source; };
    std::vector<Src> srcs{{"IN L", def.meterInL}, {"IN R", def.meterInR}, {"OUT L", def.meterOutL}, {"OUT R", def.meterOutR}};
    std::set<int> have{def.meterInL, def.meterInR, def.meterOutL, def.meterOutR};
    for (const Widget& w : def.widgets)
        if (w.kind == Kind::Meter && w.source >= 0 && have.insert(w.source).second)
            srcs.push_back({w.label.empty() ? "meter " + std::to_string(w.source) : w.label, w.source});
    std::vector<int> moved(srcs.size(), 0), bad(srcs.size(), 0);
    std::vector<float> lastHot(srcs.size(), 0.0f);
    auto meterPass = [&](int mode) {
        e = info.create();
        e->prepare(static_cast<float>(hemqa::kRate), hemqa::kBlock);
        auto binding = e->binding();
        setContext(kNone, mode);
        e->reset();
        (void)run(*e, silence, key);
        std::vector<float> quiet;
        for (const auto& s : srcs) quiet.push_back(binding->meter(s.source));
        (void)run(*e, mode == 4 || mode == 8 ? voice : full, key);
        for (std::size_t m = 0; m < srcs.size(); ++m) {
            const float hot = binding->meter(srcs[m].source);
            if (!std::isfinite(hot) || !std::isfinite(quiet[m])) bad[m] = 1;
            else if (quiet[m] != hot) moved[m] = 1;
            lastHot[m] = hot;
        }
    };
    auto allMoved = [&] { return std::all_of(moved.begin(), moved.end(), [](int x) { return x != 0; }); };
    for (int mode = 0; mode <= 4; ++mode) meterPass(mode);
    meterPass(6);   // the coloured starting point (zero colour on load)
    meterPass(8);
    // Same as the sweep: a meter can need one other control moved (a clipper's meter needs DRIVE to clip).
    for (std::size_t k = 0; k < count && !allMoved(); ++k) {
        if (!sweepable(k) || keepAtDefault(e->desc(k), k, bypassParam)) continue;
        const auto& dk = e->desc(k);
        for (float o : hemqa::sweepValues(dk.min, dk.def, stepMax(dk), stepped(dk))) {
            if (o == dk.def) continue;
            pairK = k; pairV = o;
            meterPass(5);
            if (!allMoved()) meterPass(7);   // the same, from the coloured starting point
            if (allMoved()) break;
        }
    }
    for (std::size_t m = 0; m < srcs.size(); ++m) {
        if (bad[m]) rep.add(name, "METER", "METER_NAN_OR_INF", srcs[m].name, "non-finite meter value (source " + std::to_string(srcs[m].source) + ")");
        if (!moved[m]) rep.add(name, "METER", "METER_STUCK", srcs[m].name, "reads " + hemqa::fmt("%g", lastHot[m]) +
                               " for both silence and hot signal, with the controls at default / min / max / middle, with a single voice"
                               " and with each other control moved on its own (source " +
                               std::to_string(srcs[m].source) + ")");
    }
}

bool interactive(Kind k) {
    return k == Kind::Knob || k == Kind::Fader || k == Kind::Range || k == Kind::Button || k == Kind::Selector || k == Kind::Readout ||
           k == Kind::Trim || k == Kind::Handle || k == Kind::Pad;
}
const char* kindName(Kind k) {
    switch (k) {
    case Kind::Knob: return "knob"; case Kind::Fader: return "fader"; case Kind::Range: return "range"; case Kind::Button: return "button";
    case Kind::Selector: return "selector"; case Kind::Readout: return "readout"; case Kind::Trim: return "trim"; case Kind::Handle: return "handle";
    case Kind::Pad: return "pad"; case Kind::Meter: return "meter"; case Kind::Display: return "display"; case Kind::Bay: return "bay";
    default: return "label";
    }
}

void auditFace(const PluginInfo& info, hemqa::Report& rep) {
    const std::string name = info.name;
    auto e = info.create();
    const int count = static_cast<int>(e->paramCount());
    const PanelDef def = info.panel();
    const std::vector<int> hiddenList = info.hidden ? info.hidden() : std::vector<int>{};
    const std::set<int> hidden(hiddenList.begin(), hiddenList.end());

    auto label = [&](const Widget& w) {
        std::string s = kindName(w.kind);
        if (!w.label.empty()) s += " " + w.label;
        else if (w.param >= 0 && w.param < count) s += " " + id(*e, static_cast<std::size_t>(w.param));
        return s;
    };
    std::vector<bool> bound(static_cast<std::size_t>(count), false);
    auto mark = [&](int p) { if (p >= 0 && p < count) bound[static_cast<std::size_t>(p)] = true; };
    auto checkIdx = [&](const Widget& w, int p, const char* which) {
        if (p >= count) rep.add(name, "UI", "BAD_PARAM_INDEX", label(w), std::string(which) + " " + std::to_string(p) + " (has " + std::to_string(count) + ")");
    };
    for (int p : {def.bypassParam, def.inputParam, def.outputParam}) {
        if (p < 0 || p >= count) rep.add(name, "UI", "BAD_PARAM_INDEX", "frame key", "bypass / input / output param " + std::to_string(p));
        mark(p);
    }
    for (const Widget& w : def.widgets) {
        checkIdx(w, w.param, "param");
        checkIdx(w, w.param2, "param2");
        checkIdx(w, w.param3, "param3");
        checkIdx(w, w.showParam, "showParam");
        if (w.kind == Kind::Button && w.choice >= 0 && w.param >= 0 && w.param < count) {
            const auto& d = e->desc(static_cast<std::size_t>(w.param));
            if (w.choice > static_cast<int>(std::lround(stepMax(d))))
                rep.add(name, "UI", "BAD_PARAM_INDEX", label(w), "selects choice " + std::to_string(w.choice) + " past the last one");
        }
        if (w.kind == Kind::Meter || w.kind == Kind::Display || w.kind == Kind::Label) { if (w.kind == Kind::Display) mark(w.param); continue; }
        mark(w.param);
        mark(w.param2);
        mark(w.param3);
    }
    for (int p = 0; p < count; ++p)
        if (!bound[static_cast<std::size_t>(p)] && !hidden.count(p) && hostVisible(info, static_cast<std::size_t>(p)))
            rep.add(name, "UI", "ORPHAN_PARAM", id(*e, static_cast<std::size_t>(p)), "no control on the faceplate");

    struct Hot { Box box; std::string what; const Widget* w; };
    std::vector<Hot> hot;
    // curve handles are hit at their dot (moves with the values), not by their box: left out of the overlap check
    for (const Widget& w : def.widgets) if (interactive(w.kind) && w.kind != Kind::Handle) hot.push_back({w.box, label(w), &w});
    hot.push_back({def.bypass, "BYPASS", nullptr});
    hot.push_back({def.inputTrim, "INPUT trim", nullptr});
    hot.push_back({def.outputTrim, "OUTPUT trim", nullptr});
    const hemqa::Rect bed = qr(frame::bed), emblem = qr(frame::emblem);
    auto isKey = [](const Widget* w) { return w && (w->icon == Icon::KeyWhite || w->icon == Icon::KeyBlack); };
    for (std::size_t a = 0; a < hot.size(); ++a) {
        const hemqa::Rect f = qr(def.frame.toFrame(hot[a].box));
        if (!hemqa::within(f, bed) || hemqa::overlaps(f, emblem))
            rep.add(name, "UI", "OUT_OF_BOUNDS", hot[a].what, hemqa::fmt("frame x %.0f", f.x) + hemqa::fmt(" y %.0f", f.y) + hemqa::fmt(" w %.0f", f.w) +
                    hemqa::fmt(" h %.0f", f.h) + " (glass bed 230,230 3380x1690; emblem 1720,0 400x545)");
        for (std::size_t b = a + 1; b < hot.size(); ++b) {
            const Widget* x = hot[a].w;
            const Widget* y = hot[b].w;
            if (x && y && x->showParam >= 0 && x->showParam == y->showParam && x->showValue != y->showValue) continue;   // never shown together
            if (isKey(x) && isKey(y)) continue;   // piano keys: black keys sit on the white ones by design
            if (hemqa::overlaps(qr(hot[a].box), qr(hot[b].box))) rep.add(name, "UI", "HITBOX_OVERLAP", hot[a].what + " / " + hot[b].what, "hit boxes collide");
        }
    }
}

} // namespace

void runQaReport() {
    std::printf("[QA] HEM Dark Matter: %zu plug-ins — stability, sweep, meters, faceplate\n", plugins().size());
    hemqa::Report rep("HEM Dark Matter");
    for (const PluginInfo& info : plugins()) {
        rep.plugin(info.name);
        auditDsp(info, rep);
        auditFace(info, rep);
        std::printf("[QA]   %s done\n", info.name);
        std::fflush(stdout);
    }
    const auto file = hemqa::outDir() / "qa_report_darkmatter.csv";
    std::printf("[QA] report: %s (%s)\n", file.string().c_str(), rep.write(file) ? "written" : "COULD NOT WRITE");
    rep.printSummary();
}
